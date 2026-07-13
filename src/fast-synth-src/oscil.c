/* oscil.c
 *
 * MODIFIED from oscil.c of TUGs: Teaching Unit Generators 
 * http://sourceforge.net/projects/tugs
 * 
 *
 * Functions for synthesizing a waveform using a wavetable
 * oscillator described by a oscil_info structure.
 */

#include "oscil.h"
#include <math.h>
#include <stdlib.h>


/*
 * wrap
 * 
 * Helper function to wrap phase to a valid index range.
 */
static Fastsynth_Float_Type wrap( Fastsynth_Float_Type ph, int N )
{
	const Fastsynth_Float_Type Nfloat = N;
	
 	while ( ph >= Nfloat )
    {
 		ph -= Nfloat;
    }
 	while ( ph < 0 )
    {
 		ph += Nfloat;
    }
 	return ph;	
}

/*
	create_oscil_info

	Allocate and return a new oscil_info 
	structure using the specified wavetable.
	init_phase specifies the starting phase
	in radians.
*/
oscil_info * create_oscil_info( Fastsynth_Float_Type * wavetable, int N, Fastsynth_Float_Type init_phase, Fastsynth_Float_Type sample_rate )
{
	oscil_info * ret = (oscil_info *)malloc( sizeof(oscil_info) );
	ret->table = wavetable;
	ret->N = N;
	
	// phase is stored in samples
	ret->phase = wrap( init_phase * (N / (2*M_PI)), N );
		
    ret->OneOverSR = 1. / sample_rate;
	return ret;
}

/*
	delete_oscil_info

	Free the memory associated with a oscil_info
	structure.
*/
void delete_oscil_info( oscil_info * info )
{
	free( info );
}

/*
	generate_oscil

	Generate samples for a wavetable oscillator, described
	by a oscil_info structure. Amplitude and frequency
	are inputs. Samples are synthesized
	and stored in a specified buffer. stride indicates
	the number of buffer positions to advance after each
	synthesized sample (i.e. stride=1 means fill every
	sample, stride=2 means every other samples, etc.)
*/
void generate_oscil(  oscil_info * info, 	
					  Fastsynth_Float_Type * amp, int amp_stride,
					  Fastsynth_Float_Type * freq, int freq_stride,
					  Fastsynth_Float_Type * output, int howmany, int stride )
{
	const Fastsynth_Float_Type phaseIncOverF = info->OneOverSR * info->N;
	Fastsynth_Float_Type f;
	
	while ( howmany-- > 0 )
	{
		/* save f, it might be stored in output */
		f = *freq;
		
		/* compute the output sample */
		*output += *amp * info->table[(int)info->phase];
		
		/* update the phase */
		info->phase = wrap( info->phase + (f * phaseIncOverF), info->N );
		
		/* advance buffer pointers */
		amp += amp_stride;
		freq += freq_stride;
		output += stride;
	}	
}
