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
 *	test_Partial.cpp
 *
 *	Unit tests for Partial class. Relies on Breakpoint,
 *	Partial::iterator, and Loris Exceptions.
 *
 *	Registered with CTest in test/CMakeLists.txt, and linked
 *	against the Loris library, so no source list is needed.
 *	Build and run the suite with:
 *
 *	    cmake --build build
 *	    ctest --test-dir build
 *
 * Kelly Fitz, 15 April 2003
 * loris@cerlsoundgroup.org
 *
 * http://www.cerlsoundgroup.org/Loris/
 *
 */

#include "Exception.h"
#include "Partial.h"
#include "PartialList.h"
#include "Synthesizer.h"

#include <cmath>
#include <iostream>
#include <type_traits>
#include <utility>
#include <vector>

using namespace Loris;
using namespace std;

const double Pi = 3.14159265358979324;

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

static bool
float_equal(double x, double y)
{
#ifdef VERBOSE
    cout << "\t" << x << " == " << y << " ?" << endl;
#endif
#define EPSILON .0000001
    if (std::fabs(x) > 0.)
        return std::fabs((x - y) / x) < EPSILON;
    else
        return std::fabs(x - y) < EPSILON;
}

#define SAME_PARAM_VALUES(x, y) TEST(float_equal((x), (y)))

inline double
m2pi(double x)
{
    const double EPS = .01;
    x += EPS;
    x = fmod(x, 2 * Pi);
    if (x < 0)
        x = x + (2 * Pi);
    return x - EPS;
}

#define SAME_PHASE_VALUES(x, y) SAME_PARAM_VALUES(m2pi(x), m2pi(y))

// ----------- test_parametersAt -----------
//
static void
test_parametersAt(void)
{
    std::cout << "\t--- testing Partial::parameterAt members... ---\n\n";

    //	Fabricate a Partial, and verify that parameter estimation works:
    Partial p1;
    const int NUM_BPTS = 3;
    const double P1_TIMES[] = {0.2, .8, 1.0};
    const double P1_FREQS[] = {100, 100, 120};
    const double P1_AMPS[] = {.2, .2, .4};
    const double P1_BWS[] = {0, 0, .2};
    const double P1_PHS[] = {
        .8, .8, .8}; // std::fmod( .8 + (2 * Pi * (0.2*110)), 2. * Pi )};

    for (int i = 0; i < NUM_BPTS; ++i)
    {
        p1.insert(P1_TIMES[i],
                  Breakpoint(P1_FREQS[i], P1_AMPS[i], P1_BWS[i], P1_PHS[i]));
    }

    // check parameters at t = 0.2
    double t = 0.2;
    SAME_PARAM_VALUES(p1.frequencyAt(t), P1_FREQS[0]);
    SAME_PARAM_VALUES(p1.amplitudeAt(t), P1_AMPS[0]);
    SAME_PARAM_VALUES(p1.bandwidthAt(t), P1_BWS[0]);
    SAME_PHASE_VALUES(p1.phaseAt(t), P1_PHS[0]);
    SAME_PARAM_VALUES(p1.parametersAt(t).frequency(), P1_FREQS[0]);
    SAME_PARAM_VALUES(p1.parametersAt(t).amplitude(), P1_AMPS[0]);
    SAME_PARAM_VALUES(p1.parametersAt(t).bandwidth(), P1_BWS[0]);
    SAME_PHASE_VALUES(p1.parametersAt(t).phase(), P1_PHS[0]);

    // check parameters at t = 0.8
    t = 0.8;
    SAME_PARAM_VALUES(p1.frequencyAt(t), P1_FREQS[1]);
    SAME_PARAM_VALUES(p1.amplitudeAt(t), P1_AMPS[1]);
    SAME_PARAM_VALUES(p1.bandwidthAt(t), P1_BWS[1]);
    SAME_PHASE_VALUES(p1.phaseAt(t), P1_PHS[1]);
    SAME_PARAM_VALUES(p1.parametersAt(t).frequency(), P1_FREQS[1]);
    SAME_PARAM_VALUES(p1.parametersAt(t).amplitude(), P1_AMPS[1]);
    SAME_PARAM_VALUES(p1.parametersAt(t).bandwidth(), P1_BWS[1]);
    SAME_PHASE_VALUES(p1.parametersAt(t).phase(), P1_PHS[1]);

    // check parameters at t = 1.0
    t = 1.0;
    SAME_PARAM_VALUES(p1.frequencyAt(t), P1_FREQS[2]);
    SAME_PARAM_VALUES(p1.amplitudeAt(t), P1_AMPS[2]);
    SAME_PARAM_VALUES(p1.bandwidthAt(t), P1_BWS[2]);
    SAME_PHASE_VALUES(p1.phaseAt(t), P1_PHS[2]);
    SAME_PARAM_VALUES(p1.parametersAt(t).frequency(), P1_FREQS[2]);
    SAME_PARAM_VALUES(p1.parametersAt(t).amplitude(), P1_AMPS[2]);
    SAME_PARAM_VALUES(p1.parametersAt(t).bandwidth(), P1_BWS[2]);
    SAME_PHASE_VALUES(p1.parametersAt(t).phase(), P1_PHS[2]);

    // check parameters at t = 0.1
    t = 0.1;
    SAME_PARAM_VALUES(p1.frequencyAt(t), P1_FREQS[0]);
    SAME_PARAM_VALUES(p1.amplitudeAt(t), 0);
    SAME_PARAM_VALUES(p1.bandwidthAt(t), P1_BWS[0]);
    // no phase change, exactly ten periods
    SAME_PHASE_VALUES(p1.phaseAt(t), P1_PHS[0]);
    SAME_PARAM_VALUES(p1.parametersAt(t).frequency(), P1_FREQS[0]);
    SAME_PARAM_VALUES(p1.parametersAt(t).amplitude(), 0);
    SAME_PARAM_VALUES(p1.parametersAt(t).bandwidth(), P1_BWS[0]);
    SAME_PHASE_VALUES(p1.parametersAt(t).phase(), P1_PHS[0]);

    // check parameters at t = 0.9
    t = 0.9;
    SAME_PARAM_VALUES(p1.frequencyAt(t), .5 * (P1_FREQS[1] + P1_FREQS[2]));
    SAME_PARAM_VALUES(p1.amplitudeAt(t), .5 * (P1_AMPS[1] + P1_AMPS[2]));
    SAME_PARAM_VALUES(p1.bandwidthAt(t), .5 * (P1_BWS[1] + P1_BWS[2]));
    // .1 s at avg 105 Hz equals 10.5 periods, half a period (Pi)
    // different from phase at 0.8
    SAME_PHASE_VALUES(p1.phaseAt(t), P1_PHS[1] + Pi);
    SAME_PARAM_VALUES(p1.parametersAt(t).frequency(),
                      .5 * (P1_FREQS[1] + P1_FREQS[2]));
    SAME_PARAM_VALUES(p1.parametersAt(t).amplitude(),
                      .5 * (P1_AMPS[1] + P1_AMPS[2]));
    SAME_PARAM_VALUES(p1.parametersAt(t).bandwidth(),
                      .5 * (P1_BWS[1] + P1_BWS[2]));
    SAME_PHASE_VALUES(p1.parametersAt(t).phase(), P1_PHS[1] + Pi);

    // check parameters at t = 1.1
    t = 1.1;
    SAME_PARAM_VALUES(p1.frequencyAt(t), P1_FREQS[2]);
    SAME_PARAM_VALUES(p1.amplitudeAt(t), 0);
    SAME_PARAM_VALUES(p1.bandwidthAt(t), P1_BWS[2]);
    // no phase change, exactly eleven periods
    SAME_PHASE_VALUES(p1.phaseAt(t), P1_PHS[2]);
    SAME_PARAM_VALUES(p1.parametersAt(t).frequency(), P1_FREQS[2]);
    SAME_PARAM_VALUES(p1.parametersAt(t).amplitude(), 0);
    SAME_PARAM_VALUES(p1.parametersAt(t).bandwidth(), P1_BWS[2]);
    SAME_PHASE_VALUES(p1.parametersAt(t).phase(), P1_PHS[2]);
}

// ----------- test_absorb -----------
//
static void
test_absorb(void)
{
    std::cout << "\t--- testing Partial::absorb... ---\n\n";

    //	Fabricate two Partials, and the correct result of aborbing
    //	one into the other, verify that abosrb works:
    Partial p1, p2;
    const int NUM_BPTS = 3;
    const double P1_TIMES[] = {0, .8, 1};
    const double P1_FREQS[] = {180, 180, 180};
    const double P1_AMPS[] = {.2, .2, .4};
    const double P1_BWS[] = {0, 0, .2};
    const double P1_PHS[] = {.8, .8, -1.2};

    for (int i = 0; i < NUM_BPTS; ++i)
        p1.insert(P1_TIMES[i],
                  Breakpoint(P1_FREQS[i], P1_AMPS[i], P1_BWS[i], P1_PHS[i]));

    const double P2_TIMES[] = {.2, .5, 1};
    const double P2_FREQS[] = {200, 200, 200};
    const double P2_AMPS[] = {.1, .6, .2};
    const double P2_BWS[] = {.9, .1, .1};
    const double P2_PHS[] = {0, 0, 0};

    for (int i = 0; i < NUM_BPTS; ++i)
        p2.insert(P2_TIMES[i],
                  Breakpoint(P2_FREQS[i], P2_AMPS[i], P2_BWS[i], P2_PHS[i]));

    //	the fused Partial should have Breakpoints at the same times
    //	and frequencies as the absorbing Partial (p1):
    Partial fuse_by_hand;
    Partial::iterator it = p1.begin();
    while (it != p1.end())
    {
        double t = it.time();
        double f = p1.frequencyAt(t);

        double e1 = p1.amplitudeAt(t) * p1.amplitudeAt(t);
        double e2 = p2.amplitudeAt(t) * p2.amplitudeAt(t);

        // the fused amplitude is the square root of the
        // total energy:
        double a = sqrt(e1 + e2);

        // the fused bandwidth is the ratio of the noise energy
        // to total energy, the noise energy is the noise energy
        // in p1 added to the energy in p2:
        double bw = ((e1 * p1.bandwidthAt(t)) + e2) / (e1 + e2);

        double ph = p1.phaseAt(t);
        fuse_by_hand.insert(t, Breakpoint(f, a, bw, ph));

        ++it;
    }

    // now absorb p2 into a copy of p1:
    Partial fused = p1;
    fused.absorb(p2);

    //	check:
    TEST(fused.numBreakpoints() == fuse_by_hand.numBreakpoints());

    SAME_PARAM_VALUES(fused.startTime(), fuse_by_hand.startTime());
    SAME_PARAM_VALUES(fused.endTime(), fuse_by_hand.endTime());
    SAME_PARAM_VALUES(fused.duration(), fuse_by_hand.duration());

    SAME_PARAM_VALUES(fused.frequencyAt(0), fuse_by_hand.frequencyAt(0));
    SAME_PARAM_VALUES(fused.amplitudeAt(0), fuse_by_hand.amplitudeAt(0));
    SAME_PARAM_VALUES(fused.bandwidthAt(0), fuse_by_hand.bandwidthAt(0));
    SAME_PARAM_VALUES(fused.phaseAt(0), fuse_by_hand.phaseAt(0));

    SAME_PARAM_VALUES(fused.frequencyAt(0.1), fuse_by_hand.frequencyAt(0.1));
    SAME_PARAM_VALUES(fused.amplitudeAt(0.1), fuse_by_hand.amplitudeAt(0.1));
    SAME_PARAM_VALUES(fused.bandwidthAt(0.1), fuse_by_hand.bandwidthAt(0.1));
    SAME_PARAM_VALUES(fused.phaseAt(0.1), fuse_by_hand.phaseAt(0.1));

    SAME_PARAM_VALUES(fused.frequencyAt(0.3), fuse_by_hand.frequencyAt(0.3));
    SAME_PARAM_VALUES(fused.amplitudeAt(0.3), fuse_by_hand.amplitudeAt(0.3));
    SAME_PARAM_VALUES(fused.bandwidthAt(0.3), fuse_by_hand.bandwidthAt(0.3));
    SAME_PARAM_VALUES(fused.phaseAt(0.3), fuse_by_hand.phaseAt(0.3));

    SAME_PARAM_VALUES(fused.frequencyAt(0.6), fuse_by_hand.frequencyAt(0.6));
    SAME_PARAM_VALUES(fused.amplitudeAt(0.6), fuse_by_hand.amplitudeAt(0.6));
    SAME_PARAM_VALUES(fused.bandwidthAt(0.6), fuse_by_hand.bandwidthAt(0.6));
    SAME_PARAM_VALUES(fused.phaseAt(0.6), fuse_by_hand.phaseAt(0.6));

    SAME_PARAM_VALUES(fused.frequencyAt(0.85), fuse_by_hand.frequencyAt(0.85));
    SAME_PARAM_VALUES(fused.amplitudeAt(0.85), fuse_by_hand.amplitudeAt(0.85));
    SAME_PARAM_VALUES(fused.bandwidthAt(0.85), fuse_by_hand.bandwidthAt(0.85));
    SAME_PARAM_VALUES(fused.phaseAt(0.85), fuse_by_hand.phaseAt(0.85));

    SAME_PARAM_VALUES(fused.frequencyAt(1), fuse_by_hand.frequencyAt(1));
    SAME_PARAM_VALUES(fused.amplitudeAt(1), fuse_by_hand.amplitudeAt(1));
    SAME_PARAM_VALUES(fused.bandwidthAt(1), fuse_by_hand.bandwidthAt(1));
    SAME_PARAM_VALUES(fused.phaseAt(1), fuse_by_hand.phaseAt(1));
}

// ----------- test_split -----------
//
static void
test_split(void)
{
    std::cout << "\t--- testing Partial::split... ---\n\n";

    //	Fabricate a Partial, split it, and verify that
    //	the two resulting Partials do not overlap, and
    //	that they have the same Breakpoints as the original,
    //	divided between them.
    Partial original;
    const int NUM_BPTS = 4;
    const double P1_TIMES[] = {.2, .4, .7, .9};
    const double P1_FREQS[] = {180, 150, 180, 170};
    const double P1_AMPS[] = {.2, .25, .4, .3};
    const double P1_BWS[] = {0, .1, .2, .3};
    const double P1_PHS[] = {-.8, .8, -1.2, .8};

    for (int i = 0; i < NUM_BPTS; ++i)
        original.insert(P1_TIMES[i], Breakpoint(P1_FREQS[i], P1_AMPS[i],
                                                P1_BWS[i], P1_PHS[i]));

    Partial p1 = original;
    // split into two Partials, two Breakpoints each:
    Partial p2 = p1.split(p1.findNearest(0.6));

    // verify the number of Breakpoints
    TEST(p1.numBreakpoints() == 2);
    TEST(p2.numBreakpoints() == 2);
    TEST(p1.numBreakpoints() + p2.numBreakpoints() ==
         original.numBreakpoints());

    // verify that the two do not overlap:
    TEST(p1.endTime() < p2.startTime());

    // verify that the Breakpoints are the same:
    Partial::iterator it = p1.begin();
    while (it != p1.end())
    {
        Breakpoint &p1Breakpoint = *it;
        Partial::iterator origit = original.findNearest(it.time());
        Breakpoint &origBreakpoint = *origit;

        SAME_PARAM_VALUES(it.time(), origit.time());
        SAME_PARAM_VALUES(p1Breakpoint.frequency(), origBreakpoint.frequency());
        SAME_PARAM_VALUES(p1Breakpoint.amplitude(), origBreakpoint.amplitude());
        SAME_PARAM_VALUES(p1Breakpoint.bandwidth(), origBreakpoint.bandwidth());
        SAME_PARAM_VALUES(p1Breakpoint.phase(), origBreakpoint.phase());

        ++it;
    }

    it = p2.begin();
    while (it != p2.end())
    {
        Breakpoint &p2Breakpoint = *it;
        Partial::iterator origit = original.findNearest(it.time());
        Breakpoint &origBreakpoint = *origit;

        SAME_PARAM_VALUES(it.time(), origit.time());
        SAME_PARAM_VALUES(p2Breakpoint.frequency(), origBreakpoint.frequency());
        SAME_PARAM_VALUES(p2Breakpoint.amplitude(), origBreakpoint.amplitude());
        SAME_PARAM_VALUES(p2Breakpoint.bandwidth(), origBreakpoint.bandwidth());
        SAME_PARAM_VALUES(p2Breakpoint.phase(), origBreakpoint.phase());

        ++it;
    }
}

// ----------- helpers for the move and sink tests -----------
//
//	Two different things can be asked about a pair of Partials, and they
//	are not the same question:
//
//	  equivalence -- the two have the same label, and Breakpoints at the
//	                 same times carrying the same parameters;
//	  identity    -- the two share the very same Breakpoint objects, one
//	                 having taken over the other's envelope storage.
//
//	Equivalence is the question to ask of a Partial that is supposed not
//	to have changed. Identity is the question to ask of a move: a move
//	and a copy leave equivalent results behind, and only the transfer of
//	the storage tells them apart, so equivalence alone cannot show that a
//	move is a move.

static const int MOVE_NUM_BPTS = 4;
static const double MOVE_TIMES[] = {.1, .25, .4, .55};
static const double MOVE_FREQS[] = {310, 305, 312, 308};
static const double MOVE_AMPS[] = {.2, .35, .3, .15};
static const double MOVE_BWS[] = {0, .1, .25, .05};
static const double MOVE_PHS[] = {-.8, .4, 1.1, -.3};
static const Partial::label_type MOVE_LABEL = 7;

static Partial
make_test_partial(void)
{
    Partial p;
    for (int i = 0; i < MOVE_NUM_BPTS; ++i)
    {
        p.insert(MOVE_TIMES[i], Breakpoint(MOVE_FREQS[i], MOVE_AMPS[i],
                                           MOVE_BWS[i], MOVE_PHS[i]));
    }
    p.setLabel(MOVE_LABEL);
    return p;
}

//	Return a token identifying a Partial's Breakpoint storage: the address
//	of its first Breakpoint.
//
//	Moving a Partial has to be a constant time operation, so it cannot
//	rebuild the Breakpoint map; it transfers the nodes, and the
//	Breakpoints keep their addresses. Copying allocates new ones. So
//	comparing this token before and after distinguishes a move from a
//	copy, which comparing parameters cannot do.
//
//	\pre	p has at least one Breakpoint.
static const Breakpoint *
envelope_storage(const Partial &p)
{
    return &(p.begin().breakpoint());
}

//	Verify that a Partial has exactly the Breakpoints and label that
//	make_test_partial() builds.
static void
verify_test_partial(const Partial &p)
{
    TEST(p.numBreakpoints() == Partial::size_type(MOVE_NUM_BPTS));
    TEST(p.label() == MOVE_LABEL);

    int i = 0;
    for (Partial::const_iterator it = p.begin(); it != p.end(); ++it, ++i)
    {
        SAME_PARAM_VALUES(it.time(), MOVE_TIMES[i]);
        SAME_PARAM_VALUES(it.breakpoint().frequency(), MOVE_FREQS[i]);
        SAME_PARAM_VALUES(it.breakpoint().amplitude(), MOVE_AMPS[i]);
        SAME_PARAM_VALUES(it.breakpoint().bandwidth(), MOVE_BWS[i]);
        SAME_PARAM_VALUES(it.breakpoint().phase(), MOVE_PHS[i]);
    }
    TEST(i == MOVE_NUM_BPTS);
}

//	Verify that two Partials are equivalent: same label, same Breakpoints
//	at the same times. This says nothing about whether they share storage,
//	and is the right question to ask of a Partial that must not have been
//	modified.
static void
verify_equivalent_partials(const Partial &p, const Partial &reference)
{
    TEST(p.numBreakpoints() == reference.numBreakpoints());
    TEST(p.label() == reference.label());

    Partial::const_iterator a = p.begin();
    Partial::const_iterator b = reference.begin();
    while (a != p.end() && b != reference.end())
    {
        SAME_PARAM_VALUES(a.time(), b.time());
        SAME_PARAM_VALUES(a.breakpoint().frequency(),
                          b.breakpoint().frequency());
        SAME_PARAM_VALUES(a.breakpoint().amplitude(),
                          b.breakpoint().amplitude());
        SAME_PARAM_VALUES(a.breakpoint().bandwidth(),
                          b.breakpoint().bandwidth());
        SAME_PARAM_VALUES(a.breakpoint().phase(), b.breakpoint().phase());
        ++a;
        ++b;
    }
    TEST(a == p.end());
    TEST(b == reference.end());
}

// ----------- test_move_semantics -----------
//
static void
test_move_semantics(void)
{
    std::cout << "\t--- testing Partial move construction and assignment... "
                 "---\n\n";

    //	Partial must have move operations, and they must not throw,
    //	otherwise every "move" is a deep copy of the Breakpoint map.
    TEST(std::is_move_constructible<Partial>::value);
    TEST(std::is_move_assignable<Partial>::value);
    TEST(std::is_nothrow_move_constructible<Partial>::value);
    TEST(std::is_nothrow_move_assignable<Partial>::value);

    //	Copy operations must survive the addition of the move operations.
    TEST(std::is_copy_constructible<Partial>::value);
    TEST(std::is_copy_assignable<Partial>::value);

    //	Move construction takes over the envelope: the new Partial has the
    //	old one's Breakpoints, and they are the same Breakpoints, not
    //	copies of them.
    Partial p = make_test_partial();
    const Breakpoint *storage = envelope_storage(p);

    Partial moved(std::move(p));
    verify_test_partial(moved);
    TEST(envelope_storage(moved) == storage);

    //	The moved-from Partial is left valid and empty, and can be used
    //	again.
    TEST(p.numBreakpoints() == 0);
    p.insert(1.5, Breakpoint(440, .5, 0, 0));
    TEST(p.numBreakpoints() == 1);

    //	Move assignment takes over the envelope too, and releases whatever
    //	the target was holding.
    Partial assigned;
    assigned.insert(9.0, Breakpoint(100, .1, 0, 0));
    assigned.setLabel(99);

    storage = envelope_storage(moved);
    assigned = std::move(moved);
    verify_test_partial(assigned);
    TEST(envelope_storage(assigned) == storage);
    TEST(moved.numBreakpoints() == 0);

    //	Copying still copies: the result is equivalent to its source but
    //	has its own storage, and mutating it leaves the source alone.
    Partial source = make_test_partial();
    Partial copy(source);
    verify_equivalent_partials(copy, source);
    TEST(envelope_storage(copy) != envelope_storage(source));

    copy.insert(0.9, Breakpoint(200, .1, 0, 0));
    TEST(copy.numBreakpoints() == Partial::size_type(MOVE_NUM_BPTS + 1));
    verify_test_partial(source);
}

// ----------- test_synthesize_sink_parameter -----------
//
//	Synthesizer::synthesize takes its Partial by value, as a sink
//	parameter, because it quantizes and phase-corrects a working copy.
//	Two things follow, and they are different questions:
//
//	  an lvalue argument is copied, so the caller's Partial must come back
//	  equivalent to what it was, still owning its own envelope;
//
//	  an rvalue argument is moved, so the caller's envelope is taken over
//	  rather than duplicated.
//
static void
test_synthesize_sink_parameter(void)
{
    std::cout << "\t--- testing that synthesis consumes a copy, not the "
                 "caller's Partial... ---\n\n";

    const double srate = 44100;

    //	through synthesize( Partial ), with an lvalue
    {
        Partial p = make_test_partial();
        Partial reference = p;
        const Breakpoint *storage = envelope_storage(p);

        std::vector<double> buf;
        Synthesizer synth(srate, buf);
        synth.synthesize(p);

        TEST(buf.size() > 0);
        verify_equivalent_partials(p, reference);

        //	the caller still owns the envelope it started with, so nothing
        //	was moved out from under it
        TEST(envelope_storage(p) == storage);
    }

    //	through operator()( const Partial & )
    {
        Partial p = make_test_partial();
        Partial reference = p;
        const Breakpoint *storage = envelope_storage(p);

        std::vector<double> buf;
        Synthesizer synth(srate, buf);
        synth(p);

        TEST(buf.size() > 0);
        verify_equivalent_partials(p, reference);
        TEST(envelope_storage(p) == storage);
    }

    //	through the range overload, which dereferences its iterators to
    //	lvalues, so every Partial in the list is copied and left alone
    {
        PartialList plist;
        plist.push_back(make_test_partial());
        plist.push_back(make_test_partial());
        const Partial reference = make_test_partial();

        std::vector<double> buf;
        Synthesizer synth(srate, buf);
        synth.synthesize(plist.begin(), plist.end());

        TEST(buf.size() > 0);
        TEST(plist.size() == 2);
        for (PartialList::const_iterator it = plist.begin(); it != plist.end();
             ++it)
        {
            verify_equivalent_partials(*it, reference);
        }
    }

    //	an rvalue argument is moved into the parameter: the caller's
    //	envelope is taken over, not duplicated. Checking only that samples
    //	came out would not show this.
    {
        Partial p = make_test_partial();

        std::vector<double> buf;
        Synthesizer synth(srate, buf);
        synth.synthesize(std::move(p));

        TEST(buf.size() > 0);
        TEST(p.numBreakpoints() == 0);
    }
}

// ----------- main -----------
//
int
main()
{
    std::cout << "Unit test for Partial class." << endl;
    std::cout << "Relies on Breakpoint, Partial::iterator, and Synthesizer."
              << endl
              << endl;
    std::cout << "Built: " << __DATE__ << endl << endl;

    try
    {
        test_parametersAt();
        test_absorb();
        test_split();
        test_move_semantics();
        test_synthesize_sink_parameter();
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

    //	return successfully
    cout << "Partial passed all tests." << endl;
    return 0;
}
