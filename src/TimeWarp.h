#ifndef INCLUDE_TIMEWARP_H
#define INCLUDE_TIMEWARP_H
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
 * TimeWarp.h
 *
 * Definition of class TimeWarp.
 *
 * Kelly Fitz, 19 Sep 2026
 * loris@cerlsoundgroup.org
 *
 * http://www.cerlsoundgroup.org/Loris/
 *
 */

#include "LinearEnvelope.h"
#include "Partial.h"

//	begin namespace
namespace Loris
{

// ---------------------------------------------------------------------------
//	class TimeWarp
//
//!	Class TimeWarp represents an arbitrary, not necessarily monotonic,
//!	trajectory through the time axis of a Partial.
//!
//!	Unlike Dilator, which recomputes the time of each Breakpoint and can
//!	therefore only stretch and compress (the warped times must remain in
//!	increasing order), a TimeWarp renders a Partial along any trajectory,
//!	including one that runs backwards, stands still, or reverses direction
//!	repeatedly. Reversal, freezing, and scrubbing back and forth are all
//!	expressed as a single timing envelope.
//!
//!	The timing envelope maps <em>synthesis time</em>, the time axis of the
//!	rendered samples, onto <em>envelope time</em>, the time axis of the
//!	Partial's Breakpoints. Its breakpoints are called <em>knots</em> here,
//!	to distinguish them from the Breakpoints of a Partial: a knot's time is
//!	a synthesis time and its value is the envelope time to be rendered at
//!	that instant. Both must be non-negative. The interior knots, where the
//!	slope of the trajectory changes, are called <em>knees</em>.
//!
//!	Warping is pitch-preserving: the frequency rendered at synthesis time t
//!	is the Partial's frequency at envelope time tau(t), whatever the speed
//!	or direction of the trajectory. A reversed Partial sounds at its
//!	original pitch, and a stationary trajectory sustains a tone.
//!
//!	The warp is exact, not a resampling. Within a segment (the span between
//!	two consecutive knots) the trajectory is affine, and Partial parameters
//!	are interpolated linearly between Breakpoints, so a warped Partial is
//!	represented exactly by Breakpoints at three kinds of instant: the image
//!	of every source Breakpoint the trajectory crosses, a Breakpoint
//!	computed with Partial::parametersAt at every knee, and null
//!	(zero-amplitude) Breakpoints where the trajectory leaves the Partial
//!	and later returns. The number of Breakpoints is therefore proportional
//!	to the work of rendering, and no sampling interval need be chosen.
//!
//!	A warped Partial is an ordinary Partial: the unmodified Synthesizer
//!	renders it, and it can be exported, morphed, or distilled like any
//!	other. Warping is not performed in place; the source Partial is not
//!	modified.
//!
//!	The timing envelope must be a LinearEnvelope, because the warp needs
//!	to know where the trajectory changes direction, and a generic Envelope
//!	cannot report its knees. To warp along some other function of time,
//!	sample it into a LinearEnvelope first.
//!
//!	\sa Dilator, Resampler
//
class TimeWarp
{
    //	-- instance variables --

    LinearEnvelope _timing; //!	maps synthesis time onto envelope time
    double _fadeTime;       //!	fade time for the nulls bracketing a gap

    //	-- public interface --
  public:
    //	-- construction --

    //!	Construct a new TimeWarp from a timing envelope, using the fade time
    //!	stored in the Synthesizer DefaultParameters.
    //!
    //!	\param	timing is the timing envelope, mapping synthesis time (knot
    //!			times) onto envelope time (knot values).
    //!	\throw	InvalidArgument if timing has fewer than two knots, if its
    //!			earliest knot time is negative, or if any of its values is
    //!			negative.
    explicit TimeWarp(const LinearEnvelope &timing);

    //!	Construct a new TimeWarp from a timing envelope and a fade time.
    //!
    //!	\param	timing is the timing envelope, mapping synthesis time (knot
    //!			times) onto envelope time (knot values).
    //!	\param	fadeTime is the time (in seconds) over which a warped Partial
    //!			fades out and back in when the trajectory leaves the Partial
    //!			and later returns.
    //!	\throw	InvalidArgument if timing has fewer than two knots, if its
    //!			earliest knot time is negative, if any of its values is
    //!			negative, or if fadeTime is negative.
    TimeWarp(const LinearEnvelope &timing, double fadeTime);

    //	Use compiler-generated copy, assign, and destroy.

    //	-- static construction --

    //!	Return a TimeWarp that renders the envelope time span
    //!	[envStart, envEnd] backwards, at unit speed, beginning at the
    //!	specified synthesis time.
    //!
    //!	\param	envStart is the earliest envelope time to render.
    //!	\param	envEnd is the latest envelope time to render.
    //!	\param	synthStart is the synthesis time at which the reversed
    //!			rendering begins, 0 by default.
    //!	\throw	InvalidArgument if any argument is negative, or if envEnd
    //!			is not later than envStart.
    static TimeWarp reverse(double envStart, double envEnd,
                            double synthStart = 0.);

    //!	Return a TimeWarp that renders envelope time [0, duration] forwards
    //!	at unit speed, that is, that changes nothing.
    //!
    //!	\param	duration is the length (in seconds) of the trajectory.
    //!	\throw	InvalidArgument if duration is not positive.
    static TimeWarp identity(double duration);

    //	-- access --

    //!	Return the envelope time rendered at the specified synthesis time.
    //!
    //!	\param	t is a synthesis time in seconds.
    //!	\return	the envelope time corresponding to t.
    double warpedTime(double t) const;

    //!	Return the earliest synthesis time in this TimeWarp's domain, the
    //!	time of the earliest knot in the timing envelope.
    double startTime(void) const;

    //!	Return the latest synthesis time in this TimeWarp's domain, the time
    //!	of the latest knot in the timing envelope.
    double endTime(void) const;

    //!	Return the length (in seconds) of this TimeWarp's domain.
    double duration(void) const;

    //!	Return the fade time (in seconds) used for the nulls that bracket a
    //!	gap in a warped Partial.
    double fadeTime(void) const;

    //!	Return a reference to this TimeWarp's timing envelope.
    const LinearEnvelope &timing(void) const;

    //	-- warping --

    //!	Return a new Partial that is the specified Partial rendered along
    //!	this TimeWarp's trajectory. The new Partial has the same label as
    //!	the source, and Breakpoints at synthesis times spanning (at most)
    //!	this TimeWarp's domain. The source Partial is not modified.
    //!
    //!	If the trajectory never enters the source Partial, the returned
    //!	Partial has no Breakpoints.
    //!
    //!	\param	p is the Partial to warp.
    //!	\return	a new, warped Partial.
    Partial warp(const Partial &p) const;

    //!	Function call operator: same as warp( p ).
    Partial
    operator()(const Partial &p) const
    {
        return warp(p);
    }

}; //	end of class TimeWarp

} // namespace Loris

#endif /* ndef INCLUDE_TIMEWARP_H */
