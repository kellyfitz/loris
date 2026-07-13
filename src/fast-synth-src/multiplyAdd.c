/* multiplyAdd.c
 *
 * Part of TUGs: Teaching Unit Generators 
 * http://sourceforge.net/projects/tugs
 * 
 * This file is copyright (C) 2007-2010 by Kelly Fitz <kfitz@cerlsoundgroup.org>.
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
 * Define signal multiply and add functions that can be used 
 * with other unit generators.
 */

#include "multiplyAdd.h"

/*
	generate_add

	Generate samples that are the sum of two input signals.
*/
void generate_add( Fastsynth_Float_Type * in1, int in1_stride,
				   Fastsynth_Float_Type * in2, int in2_stride,
				   Fastsynth_Float_Type * output, int howmany, int stride )
{
	while ( howmany-- > 0 )
	{
		*output = *in1 + *in2;

		/* advance buffer pointers */
		in1 += in1_stride;
		in2 += in2_stride;
		output += stride;
	}	
}
	
/*
	generate_sub

	Generate samples that are the difference of two input signals.
*/
void generate_sub( Fastsynth_Float_Type * in1, int in1_stride,
				   Fastsynth_Float_Type * in2, int in2_stride,
				   Fastsynth_Float_Type * output, int howmany, int stride )
{
	while ( howmany-- > 0 )
	{
		*output = *in1 - *in2;

		/* advance buffer pointers */
		in1 += in1_stride;
		in2 += in2_stride;
		output += stride;
	}	
}
	
/*
	generate_mult

	Generate samples that are the product of two input signals.
*/
void generate_mult( Fastsynth_Float_Type * in1, int in1_stride,
					Fastsynth_Float_Type * in2, int in2_stride,
					Fastsynth_Float_Type * output, int howmany, int stride )
{
	while ( howmany-- > 0 )
	{
		*output = *in1 * *in2;

		/* advance buffer pointers */
		in1 += in1_stride;
		in2 += in2_stride;
		output += stride;
	}	
}
