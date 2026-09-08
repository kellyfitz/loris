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
 *	test_SpcFile.cpp
 *
 *	Unit tests for the Spc capacity limits, and for import/export
 *	round tripping at the largest size each mode allows.
 *
 *	An Spc file holds 256 envelope streams. A bandwidth-enhanced Partial
 *	occupies two of them (sine magnitude with frequency, noise magnitude
 *	with phase) and a sinusoidal Partial occupies one, so the ceilings are
 *	128 and 256 Partials respectively. Partial counts are padded up to a
 *	power of two, so those ceilings are exact rather than approximate.
 *
 *	Exceeding the enhanced ceiling used to be accepted by SpcFile::write
 *	and then overrun a stack array in writeSosEnvelopesChunk, corrupting
 *	the caller's frame; see doc/spc-format.md. These tests pin the limits
 *	so that failure stays an exception.
 *
 *
 * Kelly Fitz, 7 September 2026
 * loris@cerlsoundgroup.org
 *
 * http://www.cerlsoundgroup.org/Loris/
 *
 */

#include "Breakpoint.h"
#include "LorisExceptions.h"
#include "Partial.h"
#include "SpcFile.h"

#include <iostream>

using namespace Loris;
using namespace std;

// --- macros ---

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

//	The stream ceiling, from SpcFile.cpp. Not exported in a header,
//	so it is restated here; if it ever changes, this test should fail
//	loudly rather than quietly test the wrong thing.
static const int MaxStreams = 256;
static const int MaxEnhancedPartials = MaxStreams / 2;
static const int MaxSinusoidalPartials = MaxStreams;

// ----------- helpers -----------

//	Build a Partial with a handful of Breakpoints, at a frequency
//	appropriate to its harmonic number.
static Partial
makePartial(int label)
{
    Partial p;
    p.setLabel(label);

    const double f = 100.0 * label;
    for (int i = 0; i < 6; ++i)
    {
        const double t = 0.01 * (i + 1);
        p.insert(t, Breakpoint(f, 0.3, 0.1, 0.0));
    }
    return p;
}

//	Fill an SpcFile with Partials labeled 1..n.
static void
addPartials(SpcFile &f, int n)
{
    for (int label = 1; label <= n; ++label)
    {
        f.addPartial(makePartial(label));
    }
}

//	Attempt an export, and report whether it threw. Uses the two-argument
//	write and writeSinusoidal, not the deprecated enhanced-flag overload.
static bool
exportThrows(int nPartials, bool enhanced, const std::string &path)
{
    try
    {
        SpcFile f(60);
        addPartials(f, nPartials);
        if (enhanced)
        {
            f.write(path);
        }
        else
        {
            f.writeSinusoidal(path);
        }
        return false;
    }
    catch (Exception &)
    {
        return true;
    }
}

// ----------- test_labelCeiling -----------
//
//	A label above the ceiling is refused by addPartial itself, before
//	anything is written.
//
static void
test_labelCeiling(void)
{
    std::cout << "\t--- testing that oversized labels are refused... ---\n\n";

    SpcFile f(60);

    //	the largest legal label is accepted
    f.addPartial(makePartial(MaxSinusoidalPartials));

    //	one past it is not
    bool threw = false;
    try
    {
        f.addPartial(makePartial(MaxSinusoidalPartials + 1));
    }
    catch (InvalidArgument &)
    {
        threw = true;
    }
    TEST(threw);

    //	unlabeled Partials are refused too
    threw = false;
    try
    {
        f.addPartial(makePartial(0));
    }
    catch (InvalidArgument &)
    {
        threw = true;
    }
    TEST(threw);
}

// ----------- test_streamCeiling -----------
//
//	The capacity is on streams, so the enhanced ceiling is half the
//	sinusoidal one. Crossing it must raise, not corrupt memory.
//
static void
test_streamCeiling(void)
{
    std::cout
        << "\t--- testing the enhanced and sinusoidal ceilings... ---\n\n";

    //	at the ceiling, both modes write
    TEST(!exportThrows(MaxEnhancedPartials, true, "spc_enh_at_limit.spc"));
    TEST(!exportThrows(MaxSinusoidalPartials, false, "spc_sine_at_limit.spc"));

    //	one Partial past the enhanced ceiling is over the stream budget,
    //	because the count is padded to the next power of two
    TEST(exportThrows(MaxEnhancedPartials + 1, true, "spc_enh_over.spc"));

    //	and well past it, in the range that used to smash the stack
    TEST(exportThrows(MaxSinusoidalPartials, true, "spc_enh_way_over.spc"));

    //	the same counts are fine without bandwidth enhancement
    TEST(!exportThrows(MaxEnhancedPartials + 1, false, "spc_sine_ok.spc"));
}

// ----------- test_roundTrip -----------
//
//	Export and re-import at the largest size each mode allows.
//
static void
test_roundTrip(void)
{
    std::cout << "\t--- testing import/export round trip at the limits... "
                 "---\n\n";

    {
        const char *path = "spc_enh_roundtrip.spc";
        SpcFile out(60);
        addPartials(out, MaxEnhancedPartials);
        out.write(path);

        SpcFile in(path);
        TEST_VALUE(int(in.partials().size()), MaxEnhancedPartials);
        TEST_VALUE(in.midiNoteNumber(), 60);
    }

    {
        const char *path = "spc_sine_roundtrip.spc";
        SpcFile out(60);
        addPartials(out, MaxSinusoidalPartials);
        out.writeSinusoidal(path);

        SpcFile in(path);
        TEST_VALUE(int(in.partials().size()), MaxSinusoidalPartials);
    }
}

// ----------- main -----------
//
int
main()
{
    std::cout << "Unit test for SpcFile capacity limits." << endl;
    std::cout << "Relies on Breakpoint, Partial, and SpcFile." << endl << endl;
    std::cout << "Built: " << __DATE__ << endl << endl;

    try
    {
        test_labelCeiling();
        test_streamCeiling();
        test_roundTrip();
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
    cout << "SpcFile passed all tests." << endl;
    return 0;
}
