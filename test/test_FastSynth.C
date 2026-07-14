//  test_FastSynth.C
//
//  Comparison harness for the fast block synthesizer (fastsynth) against
//  the standard Loris Synthesizer.
//
//  Layer 1: deterministic (bw = 0) partials rendered through both engines,
//           aligned by cross-correlation, compared sample-wise over the
//           steady-state region (block-edge fades excluded).
//  Layer 3: bandwidth-enhanced real analysis (clarinet) compared in the
//           short-time RMS envelope domain (the two engines use different
//           noise generators by design, so samples cannot match).
//  Layer 4: render timing for both engines on the clarinet set (reported,
//           not asserted).
//
//  Assertion tolerances are set with headroom above measured baselines
//  (recorded in comments below); the test's job is to catch regressions.

#include "AiffFile.h"
#include "Analyzer.h"
#include "Breakpoint.h"
#include "Channelizer.h"
#include "Distiller.h"
#include "FrequencyReference.h"
#include "Partial.h"
#include "PartialList.h"
#include "PartialUtils.h"
#include "Synthesizer.h"

#include "fastsynth.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

using std::cout;
using std::endl;
using std::vector;

using namespace Loris;

const double Rate = 44100;
const double TwoPi = 2 * 3.14159265358979324;

//  --------------------------------------------------------------------
//  rendering helpers
//  --------------------------------------------------------------------

static vector< double > renderStandard( PartialList & partials )
{
    vector< double > buf;
    Synthesizer synth( Rate, buf );
    for ( PartialList::iterator it = partials.begin(); it != partials.end(); ++it )
    {
        synth.synthesize( *it );
    }
    return buf;
}

static vector< double > renderFast( PartialList & partials )
{
    const double dur = PartialUtils::timeSpan( partials.begin(), partials.end() ).second;
    //  fastsynth writes through &samps_out.front() without resizing:
    //  the caller pre-sizes (same sizing as loris_fastsynth_main.cpp)
    vector< Fastsynth_Float_Type > buf(
        (size_t)std::ceil( dur * Rate ) + 2 * Fastsynth_BlockSize_samples, 0. );
    fastsynth( partials, Rate, buf );
    return vector< double >( buf.begin(), buf.end() );
}

//  --------------------------------------------------------------------
//  comparison helpers
//  --------------------------------------------------------------------

//  find the lag (applied to b) maximizing |cross-correlation| with a,
//  searched over +/- maxlag samples; corrSign reports the sign of the
//  correlation at the best lag (negative would mean polarity inversion)
static long bestLag( const vector< double > & a, const vector< double > & b,
                     long maxlag, int * corrSign = 0 )
{
    long best = 0;
    double bestcorr = 0;
    for ( long lag = -maxlag; lag <= maxlag; ++lag )
    {
        double corr = 0;
        for ( size_t i = 0; i < a.size(); ++i )
        {
            long j = (long)i + lag;
            if ( 0 <= j && j < (long)b.size() )
            {
                corr += a[i] * b[j];
            }
        }
        if ( std::fabs( corr ) > std::fabs( bestcorr ) )
        {
            bestcorr = corr;
            best = lag;
        }
    }
    if ( corrSign )
    {
        *corrSign = ( bestcorr < 0 ) ? -1 : 1;
    }
    return best;
}

//  measure the phase of a (nearly) sinusoidal signal at time t by
//  quadrature demodulation at the expected instantaneous frequency,
//  over a +/- 10 ms window
static double measurePhase( const vector< double > & x, double t, double freq )
{
    const long c = (long)( t * Rate );
    const long w = (long)( 0.010 * Rate );
    double re = 0, im = 0;
    for ( long i = c - w; i <= c + w; ++i )
    {
        if ( 0 <= i && i < (long)x.size() )
        {
            const double tau = ( i - c ) / Rate;
            re += x[i] * std::cos( TwoPi * freq * tau );
            im -= x[i] * std::sin( TwoPi * freq * tau );
        }
    }
    return std::atan2( im, re );
}

//  wrap a phase difference into (-pi, pi]
static double wrapPhase( double dp )
{
    while ( dp > 0.5 * TwoPi )  dp -= TwoPi;
    while ( dp <= -0.5 * TwoPi ) dp += TwoPi;
    return dp;
}

struct DiffStats
{
    double maxAbs;      //  largest absolute sample difference
    double rmsErrDb;    //  RMS error relative to signal RMS, in dB
};

//  compare a (reference) against b shifted by lag, skipping `skip`
//  samples at each end of the overlap (fade/edge exclusion)
static DiffStats sampleDiff( const vector< double > & a, const vector< double > & b,
                             long lag, size_t skip )
{
    double sumsq = 0, refsq = 0, maxAbs = 0;
    size_t n = 0;
    for ( size_t i = skip; i + skip < a.size(); ++i )
    {
        long j = (long)i + lag;
        if ( 0 <= j && j < (long)b.size() )
        {
            double d = a[i] - b[j];
            sumsq += d * d;
            refsq += a[i] * a[i];
            maxAbs = std::max( maxAbs, std::fabs( d ) );
            ++n;
        }
    }
    DiffStats s;
    s.maxAbs = maxAbs;
    s.rmsErrDb = ( n && refsq > 0 && sumsq > 0 )
                     ? 10 * std::log10( sumsq / refsq )
                     : -999;
    return s;
}

//  short-time RMS envelope (winSamps window, hop = winSamps)
static vector< double > rmsEnvelope( const vector< double > & x, size_t winSamps )
{
    vector< double > env;
    for ( size_t i = 0; i + winSamps <= x.size(); i += winSamps )
    {
        double ss = 0;
        for ( size_t j = i; j < i + winSamps; ++j )
        {
            ss += x[j] * x[j];
        }
        env.push_back( std::sqrt( ss / winSamps ) );
    }
    return env;
}

//  --------------------------------------------------------------------
//  test partials (deterministic, phase-correct)
//  --------------------------------------------------------------------

//  phase-correct breakpoints: phase(t) = phase(0) + 2 pi integral of f
static Partial makeSteady( double freq, double amp, double dur )
{
    Partial p;
    p.insert( 0, Breakpoint( freq, amp, 0, 0 ) );
    p.insert( dur, Breakpoint( freq, amp, 0,
                               std::fmod( TwoPi * freq * dur, TwoPi ) ) );
    return p;
}

static Partial makeGlide( double f0, double f1, double amp, double dur )
{
    Partial p;
    p.insert( 0, Breakpoint( f0, amp, 0, 0 ) );
    const double phase = TwoPi * ( f0 * dur + 0.5 * ( f1 - f0 ) * dur );
    p.insert( dur, Breakpoint( f1, amp, 0, std::fmod( phase, TwoPi ) ) );
    return p;
}

static Partial makeRamp( double freq, double amp1, double dur )
{
    Partial p;
    p.insert( 0, Breakpoint( freq, 0, 0, 0 ) );
    p.insert( dur, Breakpoint( freq, amp1, 0,
                               std::fmod( TwoPi * freq * dur, TwoPi ) ) );
    return p;
}

//  --------------------------------------------------------------------
//  layer 1: deterministic sample-domain comparison
//  --------------------------------------------------------------------

//  The fast engine renders with a designed, uniform latency of exactly
//  one block (frame k's Breakpoint is the target reached at the end of
//  block k). Sample-domain comparison therefore uses that fixed lag;
//  cross-correlation alignment is reported as a diagnostic only (it is
//  ambiguous for stationary tones, where |corr| peaks every half period).
static DiffStats compareCase( const std::string & name, const Partial & p )
{
    PartialList plist;
    plist.push_back( p );

    vector< double > std_out = renderStandard( plist );
    vector< double > fast_out = renderFast( plist );

    const long lag = Fastsynth_BlockSize_samples;

    //  exclude 3 blocks at each end (fade handling differs legitimately)
    DiffStats s = sampleDiff( std_out, fast_out, lag, 3 * Fastsynth_BlockSize_samples );

    int sign = 1;
    long xlag = bestLag( std_out, fast_out, 2 * Fastsynth_BlockSize_samples, &sign );

    cout << "  " << std::left << std::setw( 26 ) << name
         << "max|diff| " << std::setw( 12 ) << s.maxAbs
         << "rms err " << std::setprecision( 4 ) << std::setw( 8 ) << s.rmsErrDb
         << " dB   (xcorr lag " << xlag << ( sign < 0 ? " NEG)" : ")" ) << endl;
    return s;
}

//  --------------------------------------------------------------------
//  layer 2: per-engine phase accuracy vs analytic phase
//  --------------------------------------------------------------------

//  probe both engines' rendered phase against the analytic phase of the
//  partial at several times; lagComp compensates the fast engine's
//  measured render latency (in samples)
//  returns the worst fast-engine phase error (radians, lag-compensated)
static double phaseProbe( const std::string & name, const Partial & p,
                          double f0, double chirpRate, long lagComp )
{
    PartialList plist;
    plist.push_back( p );
    vector< double > std_out = renderStandard( plist );
    vector< double > fast_out = renderFast( plist );

    cout << "  " << name << " (fast lag-compensated by " << lagComp
         << " samples):" << endl;
    cout << "    t(s)    inst f(Hz)   std err(rad)   fast err(rad)" << endl;
    double worst = 0;
    for ( double t = 0.1; t < 0.45; t += 0.1 )
    {
        const double instf = f0 + chirpRate * t;
        const double analytic = TwoPi * ( f0 * t + 0.5 * chirpRate * t * t );
        const double stdPh = measurePhase( std_out, t, instf );
        const double fastPh = measurePhase( fast_out, t + lagComp / Rate, instf );
        const double fastErr = wrapPhase( fastPh - analytic );
        worst = std::max( worst, std::fabs( fastErr ) );
        cout << "    " << std::fixed << std::setprecision( 1 ) << t
             << "     " << std::setw( 6 ) << instf
             << "      " << std::setprecision( 3 ) << std::setw( 8 )
             << wrapPhase( stdPh - analytic )
             << "      " << std::setw( 8 )
             << fastErr << std::defaultfloat << endl;
    }
    return worst;
}

//  --------------------------------------------------------------------
//  main
//  --------------------------------------------------------------------

int main( void )
{
    cout << "--- fast synthesizer comparison harness ---\n" << endl;
    bool ok = true;

    //  ---- layer 1: deterministic cases ----
    //  397 Hz: period ~111 samples, incommensurate with the 100-sample
    //  block, so cross-correlation alignment is unambiguous
    cout << "deterministic (bw = 0) cases, steady state, "
         << "relative RMS error vs standard Synthesizer:" << endl;

    DiffStats steady = compareCase( "steady 397 Hz",
                                    makeSteady( 397, 0.5, 0.5 ) );
    DiffStats glide  = compareCase( "glide 397->595 Hz",
                                    makeGlide( 397, 595.5, 0.5, 0.5 ) );
    DiffStats ramp   = compareCase( "amp ramp 0->0.5",
                                    makeRamp( 397, 0.5, 0.5 ) );

    //  Measured baseline (2026-07-13, double precision, block = 100,
    //  after the BlockOscillator::initOnset fix): all three cases align
    //  at -55 dB at the designed one-block latency, and the fast
    //  engine's phase matches the standard engine's to ~0.01 rad.
    if ( !( steady.rmsErrDb < -40 && glide.rmsErrDb < -40 && ramp.rmsErrDb < -40 ) )
    {
        cout << "FAILED: deterministic-case error above -40 dB "
             << "(baseline is -55 dB)" << endl;
        ok = false;
    }

    //  ---- layer 2: phase accuracy vs analytic phase ----
    cout << "\nphase accuracy vs analytic phase:" << endl;
    double worstPh = phaseProbe( "steady 397 Hz", makeSteady( 397, 0.5, 0.5 ),
                                 397, 0, Fastsynth_BlockSize_samples );
    worstPh = std::max( worstPh,
                phaseProbe( "glide 397->595 Hz", makeGlide( 397, 595.5, 0.5, 0.5 ),
                            397, ( 595.5 - 397 ) / 0.5, Fastsynth_BlockSize_samples ) );
    if ( worstPh > 0.1 )
    {
        cout << "FAILED: fast-engine phase error " << worstPh
             << " rad exceeds 0.1 rad (baseline ~0.05)" << endl;
        ok = false;
    }

    //  ---- layers 3 + 4: real analysis (clarinet), envelopes + timing ----
    std::string path( "" );
    if ( std::getenv( "srcdir" ) )
    {
        path = std::string( std::getenv( "srcdir" ) ) + "/";
    }
    cout << "\nanalyzing clarinet for bandwidth-enhanced comparison..." << endl;
    AiffFile f( path + "clarinet.aiff" );
    Analyzer a( 415 * .8, 415 * 1.6 );
    PartialList clar = a.analyze( f.samples(), f.sampleRate() );
    FrequencyReference clarRef( clar.begin(), clar.end(), 0, 1000, 20 );
    Channelizer ch( clarRef.envelope(), 1 );
    ch.channelize( clar.begin(), clar.end() );
    Distiller still;
    still.distill( clar );
    cout << "distilled to " << clar.size() << " partials" << endl;

    using clock = std::chrono::steady_clock;

    clock::time_point t0 = clock::now();
    vector< double > std_out = renderStandard( clar );
    clock::time_point t1 = clock::now();
    vector< double > fast_out = renderFast( clar );
    clock::time_point t2 = clock::now();

    double std_ms = std::chrono::duration< double, std::milli >( t1 - t0 ).count();
    double fast_ms = std::chrono::duration< double, std::milli >( t2 - t1 ).count();
    cout << "\nrender time, standard: " << std::setprecision( 4 ) << std_ms
         << " ms, fast: " << fast_ms << " ms  ->  speedup x"
         << ( fast_ms > 0 ? std_ms / fast_ms : 0 ) << endl;

    //  envelope-domain comparison (10 ms windows), over the region where
    //  the reference envelope is within 40 dB of its peak
    const size_t win = (size_t)( 0.010 * Rate );
    long lag = bestLag( std_out, fast_out, 2 * Fastsynth_BlockSize_samples );
    vector< double > env_std = rmsEnvelope( std_out, win );
    //  apply lag before enveloping the fast render
    vector< double > fast_shift( std_out.size(), 0. );
    for ( size_t i = 0; i < fast_shift.size(); ++i )
    {
        long j = (long)i + lag;
        if ( 0 <= j && j < (long)fast_out.size() )
        {
            fast_shift[i] = fast_out[j];
        }
    }
    vector< double > env_fast = rmsEnvelope( fast_shift, win );

    double peak = *std::max_element( env_std.begin(), env_std.end() );
    double maxDevDb = 0, meanDevDb = 0;
    size_t n = 0;
    for ( size_t i = 0; i < std::min( env_std.size(), env_fast.size() ); ++i )
    {
        if ( env_std[i] > peak * 0.01 && env_fast[i] > 0 )    //  within 40 dB of peak
        {
            double dev = std::fabs( 20 * std::log10( env_fast[i] / env_std[i] ) );
            maxDevDb = std::max( maxDevDb, dev );
            meanDevDb += dev;
            ++n;
        }
    }
    if ( n )
    {
        meanDevDb /= n;
    }
    cout << "clarinet envelope deviation (10 ms RMS, active region): mean "
         << meanDevDb << " dB, max " << maxDevDb << " dB over "
         << n << " windows (alignment lag " << lag << ")" << endl;

    //  sanity assertions (tightened after characterization)
    if ( !( n > 50 && meanDevDb < 6.0 ) )
    {
        cout << "FAILED: envelope comparison implausible "
             << "(n=" << n << ", mean dev " << meanDevDb << " dB)" << endl;
        ok = false;
    }

    cout << "\n" << ( ok ? "harness completed" : "HARNESS FAILED" ) << endl;
    return ok ? 0 : 1;
}
