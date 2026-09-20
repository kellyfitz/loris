/*
 * This is the Loris C++ Class Library, implementing analysis,
 * manipulation, and synthesis of digitized sounds using the Reassigned
 * Bandwidth-Enhanced Additive Sound Model.
 *
 * Loris is Copyright (c) 1999-2026 by Kelly Fitz and Lippold Haken
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY, without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 *	test_TimeWarp.cpp
 *
 *	Unit tests for Loris non-monotonic time warping, class TimeWarp.
 *
 *
 * Kelly Fitz, 20 Sep 2026
 * loris@cerlsoundgroup.org
 *
 * http://www.cerlsoundgroup.org/Loris/
 *
 */

#include "Breakpoint.h"
#include "BreakpointUtils.h"
#include "Exception.h"
#include "LinearEnvelope.h"
#include "Partial.h"
#include "Synthesizer.h"
#include "TimeWarp.h"

#include <cmath>
#include <iostream>
#include <vector>

using namespace Loris;
using namespace std;

// --- macros ---

//	define this to see pages and pages of spew
// #define VERBOSE
#ifdef VERBOSE
#define TEST(invariant)                                                        \
    do                                                                         \
    {                                                                          \
        std::cout << "TEST: " << #invariant << endl;                           \
        Assert(invariant);                                                     \
        std::cout << " PASS" << endl << endl;                                  \
    } while (false)

#define TEST_VALUE(expr, val)                                                  \
    do                                                                         \
    {                                                                          \
        std::cout << "TEST: " << #expr << "==" << (val) << endl;               \
        Assert((expr) == (val));                                               \
        std::cout << "  PASS" << endl << endl;                                 \
    } while (false)
#else
#define TEST(invariant)                                                        \
    do                                                                         \
    {                                                                          \
        Assert(invariant);                                                     \
    } while (false)

#define TEST_VALUE(expr, val)                                                  \
    do                                                                         \
    {                                                                          \
        Assert((expr) == (val));                                               \
    } while (false)
#endif

//	Times are compared exactly up to the arithmetic that computed them,
//	parameters up to linear interpolation of doubles. Neither tolerance
//	absorbs an error in the warp; both absorb only rounding.
static const double TimeTolerance = 1.0E-12;
static const double ParamTolerance = 1.0E-9;

static bool
close(double x, double y, double tol)
{
#ifdef VERBOSE
    cout << "\t" << x << " == " << y << " ?" << endl;
#endif
    const double scale = (std::fabs(y) > 1.) ? std::fabs(y) : 1.;
    return std::fabs(x - y) < (tol * scale);
}

#define SAME_TIME(x, y) TEST(close((x), (y), TimeTolerance))
#define SAME_PARAM(x, y) TEST(close((x), (y), ParamTolerance))

//	The source Partial used by most of these tests: three Breakpoints, at
//	0.1, 0.2 and 0.4 seconds, all parameters distinct so that a Breakpoint
//	mapped to the wrong time cannot pass for the right one.
static const double SrcTimes[] = {0.1, 0.2, 0.4};
static const double SrcFreqs[] = {400., 460., 520.};
static const double SrcAmps[] = {0.2, 0.9, 0.5};
static const double SrcBws[] = {0., 0.25, 0.75};
static const double SrcPhs[] = {0.1, 1.1, 2.1};
static const int NumSrcBps = 3;

static Partial
makeSource(void)
{
    Partial p;
    p.setLabel(7);
    for (int i = 0; i < NumSrcBps; ++i)
    {
        p.insert(SrcTimes[i],
                 Breakpoint(SrcFreqs[i], SrcAmps[i], SrcBws[i], SrcPhs[i]));
    }
    return p;
}

//	Collect the times of a Partial's Breakpoints, in order.
static vector<double>
breakpointTimes(const Partial &p)
{
    vector<double> times;
    for (Partial::const_iterator it = p.begin(); it != p.end(); ++it)
    {
        times.push_back(it.time());
    }
    return times;
}

// ----------- test_identity -----------
//
//	The identity trajectory reproduces the source exactly: its knees fall
//	outside the source Partial and contribute nothing, and every source
//	Breakpoint is crossed at its own time. The rendered samples must be
//	bit-for-bit those of the unwarped Partial.
//
static void
test_identity(void)
{
    cout << "\t--- testing the identity warp... ---\n\n";

    const Partial p = makeSource();
    const TimeWarp w = TimeWarp::identity(0.5);

    SAME_TIME(w.startTime(), 0.);
    SAME_TIME(w.endTime(), 0.5);
    SAME_TIME(w.duration(), 0.5);
    SAME_TIME(w.warpedTime(0.137), 0.137);

    const Partial warped = w.warp(p);

    TEST(warped.label() == p.label());
    TEST(warped.numBreakpoints() == p.numBreakpoints());

    int i = 0;
    for (Partial::const_iterator it = warped.begin(); it != warped.end();
         ++it, ++i)
    {
        SAME_TIME(it.time(), SrcTimes[i]);
        SAME_PARAM(it.breakpoint().frequency(), SrcFreqs[i]);
        SAME_PARAM(it.breakpoint().amplitude(), SrcAmps[i]);
        SAME_PARAM(it.breakpoint().bandwidth(), SrcBws[i]);
        SAME_PARAM(it.breakpoint().phase(), SrcPhs[i]);
    }

    //	the two must render to the very same samples
    vector<double> vSrc, vWarped;
    Synthesizer(44100., vSrc).synthesize(p);
    Synthesizer(44100., vWarped).synthesize(warped);

    TEST(vSrc.size() == vWarped.size());
    TEST(0 < vSrc.size());
    for (vector<double>::size_type n = 0; n < vSrc.size(); ++n)
    {
        TEST(vSrc[n] == vWarped[n]);
    }
}

// ----------- test_exactness -----------
//
//	The central claim: a warped Partial is not an approximation. Over a
//	trajectory that runs backwards, freezes, and then runs forwards, the
//	warped Partial's interpolated parameters at every synthesis time t
//	agree with the source's parameters at envelope time tau(t).
//
static void
test_exactness(void)
{
    cout << "\t--- testing that the warp is exact, not a resampling... ---\n\n";

    const Partial p = makeSource();

    LinearEnvelope timing;
    timing.insert(0.0, 0.4); //	start at the end of the Partial
    timing.insert(0.3, 0.1); //	run backwards to its beginning
    timing.insert(0.5, 0.1); //	hold there
    timing.insert(0.8, 0.4); //	run forwards to the end again

    const TimeWarp w(timing);
    const Partial warped = w.warp(p);

    //	Breakpoints are expected at the images of the crossed source
    //	Breakpoints and at the knees, and nowhere else: a Breakpoint more
    //	would mean the warp resampled, a Breakpoint less that it dropped an
    //	inflection.
    const double expected[] = {0.0, 0.2, 0.3, 0.5, 0.6, 0.8};
    const int numExpected = 6;

    const vector<double> times = breakpointTimes(warped);
    TEST(times.size() == static_cast<vector<double>::size_type>(numExpected));
    for (int i = 0; i < numExpected; ++i)
    {
        SAME_TIME(times[i], expected[i]);
    }

    //	and between them, nothing drifts
    const int numProbes = 200;
    for (int i = 0; i <= numProbes; ++i)
    {
        const double t = w.startTime() + ((w.duration() * i) / numProbes);
        const double tau = w.warpedTime(t);

        SAME_PARAM(warped.frequencyAt(t), p.frequencyAt(tau));
        SAME_PARAM(warped.bandwidthAt(t), p.bandwidthAt(tau));

        //	amplitudeAt ramps a Partial to zero over ShortestSafeFadeTime
        //	(1 ns) at each end of its extent. A tau that rounds a hair
        //	outside the source's extent is most of the way down that
        //	cliff, while the warped Partial, which has a real Breakpoint
        //	there, is not. That is the source's own discontinuity, not an
        //	error in the warp, so step over it.
        if ((tau > (p.startTime() + Partial::ShortestSafeFadeTime)) &&
            (tau < (p.endTime() - Partial::ShortestSafeFadeTime)))
        {
            SAME_PARAM(warped.amplitudeAt(t), p.amplitudeAt(tau));
        }
    }
}

// ----------- test_reverse -----------
//
//	A reversed Partial carries the source's Breakpoints at mirrored times,
//	and renders with the mirrored amplitude envelope at the source's own
//	pitch.
//
static void
test_reverse(void)
{
    cout << "\t--- testing reversal... ---\n\n";

    const Partial p = makeSource();
    const TimeWarp w = TimeWarp::reverse(0.1, 0.4);

    SAME_TIME(w.startTime(), 0.);
    SAME_TIME(w.endTime(), 0.3);
    SAME_TIME(w.warpedTime(0.), 0.4);
    SAME_TIME(w.warpedTime(0.3), 0.1);

    const Partial warped = w.warp(p);

    //	mirrored: source time s appears at synthesis time 0.3 - (s - 0.1)
    TEST(warped.numBreakpoints() == p.numBreakpoints());

    int i = NumSrcBps - 1;
    for (Partial::const_iterator it = warped.begin(); it != warped.end();
         ++it, --i)
    {
        SAME_TIME(it.time(), 0.3 - (SrcTimes[i] - 0.1));
        SAME_PARAM(it.breakpoint().frequency(), SrcFreqs[i]);
        SAME_PARAM(it.breakpoint().amplitude(), SrcAmps[i]);
        SAME_PARAM(it.breakpoint().bandwidth(), SrcBws[i]);
    }

    //	Render a Partial whose amplitude ramps and whose frequency and
    //	bandwidth are constant, so that the peak of each block of samples
    //	tracks the amplitude envelope and nothing stochastic is involved.
    //	Reversed, the block peaks must run the other way.
    Partial ramp;
    ramp.insert(0.1, Breakpoint(500., 0.1, 0., 0.));
    ramp.insert(0.4, Breakpoint(500., 1.0, 0., 0.));

    const double fs = 44100.;
    vector<double> vFwd, vRev;
    Synthesizer(fs, vFwd).synthesize(ramp);
    Synthesizer(fs, vRev).synthesize(TimeWarp::reverse(0.1, 0.4).warp(ramp));

    //	compare 10 ms blocks of the reverse render against the mirrored
    //	blocks of the forward render, skipping the outermost blocks, where
    //	the Synthesizer's own fades are not mirror images
    const int blockLen = static_cast<int>(0.010 * fs);
    const int numBlocks = 28;
    for (int b = 1; b < numBlocks - 1; ++b)
    {
        //	reverse block b covers synthesis time [b, b+1) * 0.010, which
        //	is envelope time (0.4 - (b+1)*0.010, 0.4 - b*0.010]; the
        //	forward render has that envelope time at the same index
        const int revFirst = b * blockLen;
        const int fwdFirst = static_cast<int>((0.4 - ((b + 1) * 0.010)) * fs);

        double revPeak = 0., fwdPeak = 0.;
        for (int n = 0; n < blockLen; ++n)
        {
            revPeak = std::max(revPeak, std::fabs(vRev[revFirst + n]));
            fwdPeak = std::max(fwdPeak, std::fabs(vFwd[fwdFirst + n]));
        }

        //	the peaks bound the same stretch of the amplitude ramp, so they
        //	agree to within the error of peak-picking a sampled sinusoid
        TEST(0.05 < revPeak);
        TEST(std::fabs(revPeak - fwdPeak) < (0.01 * fwdPeak));
    }
}

// ----------- test_freeze -----------
//
//	A stationary trajectory sustains the Partial's parameters at one
//	envelope time. A stationary trajectory outside the Partial produces
//	nothing at all.
//
static void
test_freeze(void)
{
    cout << "\t--- testing a frozen trajectory... ---\n\n";

    const Partial p = makeSource();

    LinearEnvelope hold;
    hold.insert(0.0, 0.2);
    hold.insert(0.5, 0.2);

    const Partial frozen = TimeWarp(hold).warp(p);

    //	two knees, and no crossings in between
    TEST(2 == frozen.numBreakpoints());
    SAME_TIME(frozen.startTime(), 0.0);
    SAME_TIME(frozen.endTime(), 0.5);

    for (Partial::const_iterator it = frozen.begin(); it != frozen.end(); ++it)
    {
        SAME_PARAM(it.breakpoint().frequency(), p.frequencyAt(0.2));
        SAME_PARAM(it.breakpoint().amplitude(), p.amplitudeAt(0.2));
        SAME_PARAM(it.breakpoint().bandwidth(), p.bandwidthAt(0.2));
    }
    SAME_PARAM(frozen.amplitudeAt(0.25), p.amplitudeAt(0.2));
    SAME_PARAM(frozen.frequencyAt(0.25), p.frequencyAt(0.2));

    //	held past the end of the Partial: silence, an empty Partial
    LinearEnvelope beyond;
    beyond.insert(0.0, 2.0);
    beyond.insert(0.5, 2.0);

    const Partial nothing = TimeWarp(beyond).warp(p);
    TEST(0 == nothing.numBreakpoints());
    TEST(nothing.label() == p.label());
}

// ----------- test_fold_with_gap -----------
//
//	A trajectory that leaves the Partial and comes back must fall silent
//	in between, and must announce its return as an onset. Nulls are
//	inserted on both sides of the interior gap; the ends of the trajectory
//	need none, because the Synthesizer fades a Partial in and out itself.
//
static void
test_fold_with_gap(void)
{
    cout << "\t--- testing a fold that leaves the Partial... ---\n\n";

    const Partial p = makeSource();
    const double fade = 0.001;

    LinearEnvelope timing;
    timing.insert(0.0, 0.1); //	enter at the start of the Partial
    timing.insert(0.3, 0.6); //	run out past its end
    timing.insert(0.6, 0.1); //	and come back

    const TimeWarp w(timing, fade);
    TEST(w.fadeTime() == fade);

    const Partial warped = w.warp(p);

    //	0.18 is where the trajectory crosses the last source Breakpoint on
    //	the way out, 0.42 where it crosses it again on the way back
    const double expected[] = {0.0,         0.06, 0.18, 0.18 + fade,
                               0.42 - fade, 0.42, 0.54, 0.6};
    const int numExpected = 8;

    const vector<double> times = breakpointTimes(warped);
    TEST(times.size() == static_cast<vector<double>::size_type>(numExpected));
    for (int i = 0; i < numExpected; ++i)
    {
        SAME_TIME(times[i], expected[i]);
    }

    //	exactly the two bracketing Breakpoints are null; in particular the
    //	ends of the trajectory are not padded
    int i = 0;
    for (Partial::const_iterator it = warped.begin(); it != warped.end();
         ++it, ++i)
    {
        const bool shouldBeNull = (3 == i) || (4 == i);
        TEST(BreakpointUtils::isNull(it.breakpoint()) == shouldBeNull);
    }

    //	and the gap really is silent when rendered
    const double fs = 44100.;
    vector<double> v;
    Synthesizer(fs, v).synthesize(warped);

    for (int n = static_cast<int>(0.19 * fs); n < static_cast<int>(0.41 * fs);
         ++n)
    {
        TEST(0. == v[n]);
    }

    //	while the parts on either side are not
    double before = 0., after = 0.;
    for (int n = static_cast<int>(0.05 * fs); n < static_cast<int>(0.15 * fs);
         ++n)
    {
        before = std::max(before, std::fabs(v[n]));
    }
    for (int n = static_cast<int>(0.45 * fs); n < static_cast<int>(0.55 * fs);
         ++n)
    {
        after = std::max(after, std::fabs(v[n]));
    }
    TEST(0.01 < before);
    TEST(0.01 < after);
}

// ----------- test_narrow_gap -----------
//
//	When the excursion outside the Partial is shorter than two fade times
//	there is no room for a fade out and a fade in, and a single null is
//	placed between the two crossings. It must still silence the Partial,
//	and it must not fall so close to its neighbours that Partial::insert
//	erases one of them.
//
static void
test_narrow_gap(void)
{
    cout << "\t--- testing a gap too short for two fades... ---\n\n";

    const Partial p = makeSource();
    const double fade = 0.02; //	long, to force the narrow case

    LinearEnvelope timing;
    timing.insert(0.00, 0.35); //	inside the Partial
    timing.insert(0.05, 0.41); //	just past its end
    timing.insert(0.10, 0.35); //	and back inside

    const Partial warped = TimeWarp(timing, fade).warp(p);

    //	knee, crossing out, null, crossing back, knee
    TEST(5 == warped.numBreakpoints());

    const vector<double> times = breakpointTimes(warped);

    int numNulls = 0;
    int i = 0;
    for (Partial::const_iterator it = warped.begin(); it != warped.end();
         ++it, ++i)
    {
        if (BreakpointUtils::isNull(it.breakpoint()))
        {
            ++numNulls;
            TEST(2 == i); //	between the two crossings, not at an end
            //	and midway between them, clear of both
            SAME_TIME(it.time(), 0.5 * (times[1] + times[3]));
            TEST((it.time() - times[1]) > Partial::ShortestSafeFadeTime);
            TEST((times[3] - it.time()) > Partial::ShortestSafeFadeTime);
        }
    }
    TEST(1 == numNulls);

    //	the crossings on either side of the null survived intact
    SAME_PARAM(warped.amplitudeAt(times[1]), p.amplitudeAt(0.4));
    SAME_PARAM(warped.amplitudeAt(times[3]), p.amplitudeAt(0.4));
}

// ----------- test_source_unmodified -----------
//
//	Warping is not performed in place. Unlike dilate and resample, the
//	source Partial is left exactly as it was.
//
static void
test_source_unmodified(void)
{
    cout << "\t--- testing that warping does not modify the source... ---\n\n";

    Partial p = makeSource();

    LinearEnvelope timing;
    timing.insert(0.0, 0.1);
    timing.insert(0.3, 0.6);
    timing.insert(0.6, 0.1);

    const Partial warped = TimeWarp(timing).warp(p);
    TEST(0 < warped.numBreakpoints());

    TEST(p.label() == 7);
    TEST(p.numBreakpoints() == static_cast<Partial::size_type>(NumSrcBps));

    int i = 0;
    for (Partial::const_iterator it = p.begin(); it != p.end(); ++it, ++i)
    {
        SAME_TIME(it.time(), SrcTimes[i]);
        SAME_PARAM(it.breakpoint().frequency(), SrcFreqs[i]);
        SAME_PARAM(it.breakpoint().amplitude(), SrcAmps[i]);
        SAME_PARAM(it.breakpoint().bandwidth(), SrcBws[i]);
        SAME_PARAM(it.breakpoint().phase(), SrcPhs[i]);
    }
}

// ----------- test_validation -----------
//
//	A trajectory that cannot be rendered is rejected at construction,
//	rather than producing a Partial at negative time or a silent surprise.
//
static void
test_validation(void)
{
    cout << "\t--- testing rejection of unusable trajectories... ---\n\n";

    bool caught;

#define TEST_THROWS(expr)                                                      \
    do                                                                         \
    {                                                                          \
        caught = false;                                                        \
        try                                                                    \
        {                                                                      \
            expr;                                                              \
        }                                                                      \
        catch (InvalidArgument &)                                              \
        {                                                                      \
            caught = true;                                                     \
        }                                                                      \
        TEST(caught);                                                          \
    } while (false)

    //	an envelope that does not span an interval of synthesis time
    LinearEnvelope empty;
    TEST_THROWS(TimeWarp w(empty); (void)w);

    LinearEnvelope single;
    single.insert(0.2, 0.3);
    TEST_THROWS(TimeWarp w(single); (void)w);

    //	synthesis time cannot be negative
    LinearEnvelope negTime;
    negTime.insert(-0.1, 0.0);
    negTime.insert(0.5, 0.5);
    TEST_THROWS(TimeWarp w(negTime); (void)w);

    //	nor can envelope time
    LinearEnvelope negValue;
    negValue.insert(0.0, 0.1);
    negValue.insert(0.5, -0.5);
    TEST_THROWS(TimeWarp w(negValue); (void)w);

    //	nor the fade time
    LinearEnvelope ok;
    ok.insert(0.0, 0.0);
    ok.insert(0.5, 0.5);
    TEST_THROWS(TimeWarp w(ok, -0.001); (void)w);

    //	the named constructors have their own preconditions
    TEST_THROWS((void)TimeWarp::reverse(0.4, 0.1));
    TEST_THROWS((void)TimeWarp::reverse(-0.1, 0.4));
    TEST_THROWS((void)TimeWarp::identity(0.));
    TEST_THROWS((void)TimeWarp::identity(-1.));

#undef TEST_THROWS
}

// ----------- main -----------
//
int
main(void)
{
    std::cout << "Unit test for class TimeWarp." << endl;
    std::cout << "Relies on Assert. Failure is indicated by a thrown"
              << " exception or a failed assertion." << endl
              << endl;
    std::cout << "Built: " << __DATE__ << endl << endl;

    try
    {
        test_identity();
        test_exactness();
        test_reverse();
        test_freeze();
        test_fold_with_gap();
        test_narrow_gap();
        test_source_unmodified();
        test_validation();
    }
    catch (Exception &ex)
    {
        cout << "Caught Loris exception: " << ex.what() << endl;
        return 1;
    }
    catch (std::exception &ex)
    {
        cout << "Caught std C++ exception: " << ex.what() << endl;
        return 1;
    }

    // ----------- done -----------
    //
    cout << "\t--- TimeWarp passed all tests. ---" << endl;
    return 0;
}
