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
 * PartialList.C
 *
 * Definition of Loris::PartialList class members.
 *
 * Kelly Fitz, 15 Feb 2011
 * loris@cerlsoundgroup.org
 *
 * http://www.cerlsoundgroup.org/Loris/
 *
 */

#include "PartialList.h"

//	begin namespace
namespace Loris
{

//	The default constructor, copy and move constructors, copy and move
//	assignment operators, and destructor are all defaulted in PartialList.h;
//	std::list provides the correct behavior for each.

// ---------------------------------------------------------------------------
//	extract
// ---------------------------------------------------------------------------
//! Remove a range of Partials from this List and return a new List containing
//! those Partials.
//!
//! \param  b beginning of a range of Partials in this PartialList
//! \param  e end of a range of Partials in this PartialList
//! \return a new PartialList containing the Partials in the half-open range
//! [b,e)
//! \post   Partials in the range [b,e) are removed from this List
//! \pre    [b,e) must describe a valid range of Partials in this List
//
PartialList
PartialList::extract(iterator b, iterator e)
{
    PartialList ret;
    ret.mList.splice(ret.mList.begin(), mList, b, e);
    return ret;
}

} // namespace Loris
