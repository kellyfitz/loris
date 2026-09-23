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
 * TimeWarp.cpp
 *
 * Implementation of class TimeWarp.
 *
 * Kelly Fitz, 19 Sep 2026
 * loris@cerlsoundgroup.org
 *
 * http://www.cerlsoundgroup.org/Loris/
 *
 */

#include "TimeWarp.h"

#include "Breakpoint.h"
#include "BreakpointUtils.h"
#include "LorisExceptions.h"
#include "Synthesizer.h"

//	begin namespace
namespace Loris
{

// ---------------------------------------------------------------------------
//	local helpers
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
//	validateTiming		(STATIC)
// ---------------------------------------------------------------------------
//	Raise InvalidArgument unless the timing envelope can be used as a
//	trajectory: it must span an interval of synthesis time, and neither
//	its times (synthesis times) nor its values (envelope times) may be
//	negative.
//
static void
validateTiming(const LinearEnvelope &timing, double fadeTime)
{
    if (2 > timing.size())
    {
        Throw(InvalidArgument, "A TimeWarp timing envelope must have at least "
                               "two breakpoints, spanning an interval of "
                               "synthesis time.");
    }

    if (0. > timing.begin()->first)
    {
        Throw(InvalidArgument,
              "A TimeWarp timing envelope cannot have negative times, "
              "synthesis time is non-negative.");
    }

    for (LinearEnvelope::const_iterator it = timing.begin(); it != timing.end();
         ++it)
    {
        if (0. > it->second)
        {
            Throw(InvalidArgument,
                  "A TimeWarp timing envelope cannot have negative values, "
                  "envelope time is non-negative.");
        }
    }

    if (0. > fadeTime)
    {
        Throw(InvalidArgument, "A TimeWarp fade time must be non-negative.");
    }
}

// ---------------------------------------------------------------------------
//	class PartialWalker
// ---------------------------------------------------------------------------
//	Accumulates the Breakpoints of a warped Partial, in increasing
//	synthesis time, tracking the trajectory's excursions outside the source
//	Partial so that gaps can be bracketed with nulls. Private to this file,
//	and deliberately absent from TimeWarp.h: it is how the warp is computed,
//	not part of the interface.
//
//	Three kinds of Breakpoint are emitted: a Breakpoint computed with
//	parametersAt at each knee of the trajectory (knee), a copy of each
//	source Breakpoint the trajectory crosses (crossing), and a null on
//	either side of each gap (see emitInside).
//
class PartialWalker
{
    Partial &_out;       //	the warped Partial under construction
    const Partial &_src; //	the Partial being warped
    const double _fade;  //	fade time for the nulls bracketing a gap

    const double _tStart; //	extent of _src, in envelope time
    const double _tEnd;

    bool _everInside;   //	has any Breakpoint been emitted yet?
    bool _inside;       //	was the last observed position inside _src?
    double _exitTime;   //	synthesis time of the latest emission
    Breakpoint _exitBp; //	the Breakpoint emitted there

  public:
    PartialWalker(Partial &out, const Partial &src, double fade) :
        _out(out),
        _src(src),
        _fade(fade),
        _tStart(src.startTime()),
        _tEnd(src.endTime()),
        _everInside(false),
        _inside(false),
        _exitTime(0.),
        _exitBp(0., 0., 0., 0.)
    {
    }

    //	Is the specified envelope time within the source Partial?
    bool
    isInside(double tau) const
    {
        return (tau >= _tStart) && (tau <= _tEnd);
    }

    //	Emit the Breakpoint at a knee of the trajectory, if the trajectory
    //	is within the source Partial there. Otherwise just note that the
    //	trajectory has left the Partial.
    void
    knee(double t, double tau)
    {
        if (isInside(tau))
        {
            emitInside(t, _src.parametersAt(tau));
        }
        else
        {
            _inside = false;
        }
    }

    //	Emit the source Breakpoints crossed by one segment of the
    //	trajectory, in increasing synthesis time. The knots at either end
    //	of the segment are emitted by knee(), so Breakpoints at exactly
    //	tauA or tauB are skipped here.
    void
    segment(double tA, double tauA, double tB, double tauB)
    {
        if (tauB == tauA)
        {
            //	the trajectory is stationary, the Partial is frozen at tauA
            //	and the two knees bracketing this segment say everything
            //	there is to say about it:
            return;
        }

        //	map an envelope time within this segment onto synthesis time:
        const double dtdtau = (tB - tA) / (tauB - tauA);

        if (tauB > tauA)
        {
            //	forward: ascend through the Breakpoints in (tauA, tauB)
            Partial::const_iterator it = _src.findAfter(tauA);
            while (it != _src.end() && it.time() <= tauA)
            {
                ++it;
            }
            while (it != _src.end() && it.time() < tauB)
            {
                emitInside(tA + ((it.time() - tauA) * dtdtau), it.breakpoint());
                ++it;
            }
        }
        else
        {
            //	reverse: descend through the Breakpoints in (tauB, tauA),
            //	which still visits them in increasing synthesis time
            Partial::const_iterator it = _src.findAfter(tauA);
            if (it == _src.begin())
            {
                //	no Breakpoint earlier than tauA
                return;
            }
            --it; //	the latest Breakpoint earlier than tauA

            while (it.time() > tauB)
            {
                emitInside(tA + ((it.time() - tauA) * dtdtau), it.breakpoint());
                if (it == _src.begin())
                {
                    break;
                }
                --it;
            }
        }
    }

  private:
    //	Emit a Breakpoint known to lie within the source Partial, first
    //	closing any gap that preceded it.
    void
    emitInside(double t, const Breakpoint &bp)
    {
        if (_everInside && !_inside)
        {
            bracketGap(t, bp);
        }

        _out.insert(t, bp);

        _everInside = true;
        _inside = true;
        _exitTime = t;
        _exitBp = bp;
    }

    //	The trajectory left the source Partial after _exitTime and has
    //	returned at t. Insert nulls so that the warped Partial is silent
    //	across the gap, and so that the Synthesizer resets the oscillator
    //	phase on re-entry, as it does for any Partial onset.
    //
    //	Nothing is inserted at the ends of the trajectory: a warped Partial
    //	that begins or ends part way through the domain is faded in and out
    //	by the Synthesizer itself, exactly as any other Partial is.
    void
    bracketGap(double t, const Breakpoint &bp)
    {
        const double gap = t - _exitTime;

        //	leave no room for a null, and inserting one would displace the
        //	Breakpoints it is meant to separate (Partial::insert erases a
        //	neighbor within a nanosecond):
        if (gap <= 2. * Partial::ShortestSafeFadeTime)
        {
            return;
        }

        //	Two nulls need three separations wider than the merge window: the
        //	fade out from the exit, the span between the nulls, and the fade in
        //	to the entry. The first and last are _fade; the middle one is
        //	gap - 2*_fade, so `gap > 2*_fade` alone is not enough -- a gap a
        //	half-nanosecond wider than the two fades would put the second null
        //	on top of the first and erase it.
        if ((_fade > Partial::ShortestSafeFadeTime) &&
            ((gap - (2. * _fade)) > Partial::ShortestSafeFadeTime))
        {
            //	room for a fade out and a fade in:
            _out.insert(_exitTime + _fade,
                        BreakpointUtils::makeNullAfter(_exitBp, _fade));
            _out.insert(t - _fade, BreakpointUtils::makeNullBefore(bp, _fade));
        }
        else
        {
            //	a narrow gap: one null between the two, so that the Partial
            //	still reaches zero amplitude and still resets its phase
            const double tmid = _exitTime + (0.5 * gap);
            _out.insert(tmid, BreakpointUtils::makeNullBefore(bp, t - tmid));
        }
    }

}; //	end of class PartialWalker

// ---------------------------------------------------------------------------
//	TimeWarp constructor
// ---------------------------------------------------------------------------
//!	Construct a new TimeWarp from a timing envelope, using the fade time
//!	stored in the Synthesizer DefaultParameters.
//
TimeWarp::TimeWarp(const LinearEnvelope &timing) :
    _timing(timing),
    _fadeTime(Synthesizer::DefaultParameters().fadeTime)
{
    validateTiming(_timing, _fadeTime);
}

// ---------------------------------------------------------------------------
//	TimeWarp constructor
// ---------------------------------------------------------------------------
//!	Construct a new TimeWarp from a timing envelope and a fade time.
//
TimeWarp::TimeWarp(const LinearEnvelope &timing, double fadeTime) :
    _timing(timing),
    _fadeTime(fadeTime)
{
    validateTiming(_timing, _fadeTime);
}

// ---------------------------------------------------------------------------
//	reverse (static)
// ---------------------------------------------------------------------------
//!	Return a TimeWarp that renders the envelope time span
//!	[envStart, envEnd] backwards, at unit speed.
//
TimeWarp
TimeWarp::reverse(double envStart, double envEnd, double synthStart)
{
    if ((0. > envStart) || (0. > envEnd) || (0. > synthStart))
    {
        Throw(InvalidArgument, "TimeWarp::reverse times must be non-negative.");
    }

    if (envEnd <= envStart)
    {
        Throw(InvalidArgument,
              "TimeWarp::reverse end time must be later than its start time.");
    }

    LinearEnvelope timing;
    timing.insert(synthStart, envEnd);
    timing.insert(synthStart + (envEnd - envStart), envStart);

    return TimeWarp(timing);
}

// ---------------------------------------------------------------------------
//	identity (static)
// ---------------------------------------------------------------------------
//!	Return a TimeWarp that renders envelope time [0, duration] forwards
//!	at unit speed, that is, that changes nothing.
//
TimeWarp
TimeWarp::identity(double duration)
{
    if (0. >= duration)
    {
        Throw(InvalidArgument, "TimeWarp::identity duration must be positive.");
    }

    LinearEnvelope timing;
    timing.insert(0., 0.);
    timing.insert(duration, duration);

    return TimeWarp(timing);
}

// ---------------------------------------------------------------------------
//	warpedTime
// ---------------------------------------------------------------------------
//!	Return the envelope time rendered at the specified synthesis time.
//
double
TimeWarp::warpedTime(double t) const
{
    return _timing.valueAt(t);
}

// ---------------------------------------------------------------------------
//	startTime
// ---------------------------------------------------------------------------
//!	Return the earliest synthesis time in this TimeWarp's domain.
//
double
TimeWarp::startTime(void) const
{
    return _timing.begin()->first;
}

// ---------------------------------------------------------------------------
//	endTime
// ---------------------------------------------------------------------------
//!	Return the latest synthesis time in this TimeWarp's domain.
//
double
TimeWarp::endTime(void) const
{
    LinearEnvelope::const_iterator it = _timing.end();
    return (--it)->first;
}

// ---------------------------------------------------------------------------
//	duration
// ---------------------------------------------------------------------------
//!	Return the length (in seconds) of this TimeWarp's domain.
//
double
TimeWarp::duration(void) const
{
    return endTime() - startTime();
}

// ---------------------------------------------------------------------------
//	fadeTime
// ---------------------------------------------------------------------------
//!	Return the fade time (in seconds) used for the nulls that bracket a
//!	gap in a warped Partial.
//
double
TimeWarp::fadeTime(void) const
{
    return _fadeTime;
}

// ---------------------------------------------------------------------------
//	timing
// ---------------------------------------------------------------------------
//!	Return a reference to this TimeWarp's timing envelope.
//
const LinearEnvelope &
TimeWarp::timing(void) const
{
    return _timing;
}

// ---------------------------------------------------------------------------
//	warp (Partial)
// ---------------------------------------------------------------------------
//!	Return a new Partial that is the specified Partial rendered along
//!	this TimeWarp's trajectory.
//
Partial
TimeWarp::warp(const Partial &p) const
{
    Partial result;
    result.setLabel(p.label());

    if (0 == p.numBreakpoints())
    {
        return result;
    }

    PartialWalker walker(result, p, _fadeTime);

    LinearEnvelope::const_iterator knot = _timing.begin();
    double tPrev = knot->first;
    double tauPrev = knot->second;

    walker.knee(tPrev, tauPrev);

    for (++knot; knot != _timing.end(); ++knot)
    {
        const double tNext = knot->first;
        const double tauNext = knot->second;

        walker.segment(tPrev, tauPrev, tNext, tauNext);
        walker.knee(tNext, tauNext);

        tPrev = tNext;
        tauPrev = tauNext;
    }

    return result;
}

} // namespace Loris
