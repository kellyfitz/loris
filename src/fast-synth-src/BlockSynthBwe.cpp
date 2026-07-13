/*
 *  BlockSynthBwe.cpp
 *  Loris
 *
 *  Created by Pop on 10/3/11.
 *  Copyright 2011 __MyCompanyName__. All rights reserved.
 *
 */

#include "BlockSynthBwe.h"

#include "Breakpoint.h"
#include "PartialUtils.h"
#include "Resampler.h"

#include "Filter.h"

#include "LorisExceptions.h"

#include <cmath>

extern "C" {
#include "oscil.h"
#include "multiplyAdd.h"
}

#include "r250.h"


const Fastsynth_Float_Type TwoPi = 2 * 3.14159265358979324;



//  prototype, defined at bottom of this file

static 
void generate_randi( unsigned int decimation, Fastsynth_Float_Type * output, int howmany );

/*
 make_cos_table
 
 Function to allocate and fill a cosine wavetable. Make it one 
 extra sample long, so that interpolating and rounding index
 are safe.
 */
static Fastsynth_Float_Type * make_cos_table( int N )
{
    using std::cos;
    
	int i;
	Fastsynth_Float_Type * table = (Fastsynth_Float_Type *)malloc( (N+1)*sizeof(Fastsynth_Float_Type) );
	
	/* fill it with a cosine wave */
	for ( i = 0; i < N+1; ++i )
    {
		table[i] = cos( Fastsynth_Float_Type(i) * TwoPi / N );
    }
	return table;
}

/*
 make_carrier_amp_table
 
 Function to allocate and fill a lookup table for Partial amplitude
 scale as a function of bandwidth. The table is filled with samples of
 
 F(bw) = sqrt( 1. - bw )
 
 for 0.0 <= bw <= 1.0. Carrier amplitude is F(bw) * amp.
 
 Make it one extra sample long, so that interpolating and rounding index 
 are safe. Lookup as F(bw) = tab[ round(bw*N) ].
 */
static Fastsynth_Float_Type * make_carrier_amp_table( int N )
{
    using std::sqrt;
    
	int i;
	Fastsynth_Float_Type * table = (Fastsynth_Float_Type *)malloc( (N+1)*sizeof(Fastsynth_Float_Type) );
	
	/* fill it with a cosine wave */
	for ( i = 0; i < N+1; ++i )
    {
		table[i] = sqrt( 1.0 - (Fastsynth_Float_Type(i) / N) );
    }
	return table;
}

/*
 make_mod_index_table
 
 Function to allocate and fill a lookup table for Partial modulation index
 scale as a function of bandwidth. The table is filled with samples of
 
 F(bw) = sqrt( 2. * bw )
 
 for 0.0 <= bw <= 1.0. Stochastic modulator is F(bw) * amp * noise.
 
 Make it one extra sample long, so that interpolating and rounding index 
 are safe. Lookup as F(bw) = tab[ round(bw*N) ].
 */
static Fastsynth_Float_Type * make_mod_index_table( int N )
{
    using std::sqrt;
    
	int i;
	Fastsynth_Float_Type * table = (Fastsynth_Float_Type *)malloc( (N+1)*sizeof(Fastsynth_Float_Type) );
	
	/* fill it with a cosine wave */
	for ( i = 0; i < N+1; ++i )
    {
		table[i] = sqrt( 2.0 * (Fastsynth_Float_Type(i) / N) );
    }
	return table;
}



//	these are temporary

static Fastsynth_Float_Type NoiseBuffer[ BlockSynthBwe::TabSize ];


/*
 generate_env_segment
 
 Generate samples of an arbitrary envelope segment, from initial and target values.
 The target value is the value that would be achieved by the envelope one sample after
 the last one generated.
 
 Passing dTime allows a little extra efficiency, == one over howmany.
 
 
 *** Make this a generator. 
 
 */
static void generate_env_segment( Fastsynth_Float_Type ival, Fastsynth_Float_Type tval, Fastsynth_Float_Type dTime, 
								 Fastsynth_Float_Type * output, int howmany, int stride )

{
    const Fastsynth_Float_Type dVal = ( tval - ival )  * dTime;
    Fastsynth_Float_Type val = ival;
    while ( howmany-- > 0 )
    {
        *output = val;
        val += dVal;
		output += stride;        
    }
    
}


/*
 generate_random
 
 *** Make this fast! For now just call random.
 
 Need to add a filter too. And maybe interpolation.
 
 */

static void generate_random( Fastsynth_Float_Type * output, int howmany, int stride )
{
	using std::rand;
	
	static const Fastsynth_Float_Type OneOverRandMax = 1. / RAND_MAX;
	
	while ( howmany-- > 0 )
    {
		Fastsynth_Float_Type zero_to_one = rand() * OneOverRandMax;
        *output = 2.*( 0.5 + zero_to_one );
		output += stride;        
    }
	
}

/*
 generate_table_lookup_01
 
 Generate samples that are a function of the input, from samples of the
 function stored in a wavetable. The input is assumed to be bounded 0 to 1.
 
 
 */

static void generate_table_lookup_01( Fastsynth_Float_Type * in, int in_stride,
									 Fastsynth_Float_Type const * const Table, unsigned int TableMaxIdx,
									 Fastsynth_Float_Type * output, int howmany, int stride )
{
	while ( howmany-- > 0 )
    {
		unsigned int idx = (unsigned int)( (*in * TableMaxIdx) + 0.5 ); // cheap rounding
		
        *output = Table[ idx ];
		
		in += in_stride;        
		output += stride;        
    }
	
	
}


BlockSynthBwe::BlockSynthBwe( unsigned int blockLenSamples, 
							  Fastsynth_Float_Type sample_rate,
                              unsigned int numOscils ) :
	mBlockLenSamples( blockLenSamples ),
    mNoiseBufferIndex( 0 ),
	mOneOverBlockLen( 1.0 / blockLenSamples ),
	mOneOverSR( 1.0 / sample_rate ),
	mRadians2WavetablePhase( TabSize / TwoPi )
{
	allocateOscils( numOscils, sample_rate );
    
    //  initialize the random number generator
    r250_init(1);
	
	//  pre-fill the noise buffer
    generate_randi( 50, NoiseBuffer, TabSize );
	
}


//	\pre mBpFrames must already be constructed, initial oscillator state is set to 
void
BlockSynthBwe::allocateOscils( unsigned int howMany, Fastsynth_Float_Type sample_rate )
{
	BlockOscillator proto = BlockOscillator( mBlockLenSamples, sample_rate );
	
	// mOscils is a std::vector< oscil_info >	
	mOscils.resize( howMany, proto );
}


void 
BlockSynthBwe::render( std::vector< Loris::Breakpoint > & thisFrame, Fastsynth_Float_Type * putEmHere )
{
    generate_randi( 50, NoiseBuffer+mNoiseBufferIndex, mBlockLenSamples );
    mNoiseBufferIndex += mBlockLenSamples;
    if ( TabSize <= (mNoiseBufferIndex + mBlockLenSamples) )
    {
        mNoiseBufferIndex = 0;
    }
        //  need a decorrelating delay here! NOT YET IMPLEMENTED
    
    for ( unsigned int partialNum = 0; partialNum < mOscils.size(); ++partialNum )
    {
		BlockOscillator & osc = mOscils[partialNum];
		
		Loris::Breakpoint & nxtBp = thisFrame[partialNum];
		
		//  skip over all of this if all samples will be zero
		if ( 0 < nxtBp.amplitude() || 0 < osc.amplitude() )
		{    						        
			osc.oscillate( nxtBp, putEmHere, NoiseBuffer );			
		}
		
		//  reset oscillator phase if current amp is zero
		//
		//	HEY this order of operations assumes that the first frame is always filled
		//	with zero-amplitude Breakpoints (or at more precisely that all Partials
        //  fade in).
		if ( 0 == osc.amplitude() )
		{
			// osc.setPhase( nxtBp.phase() );
			osc.set( nxtBp );
		}
	}
	
}


// ------------------------------------------------
// temporary hacks to make a noise generator
// ------------------------------------------------

inline double uniform( void ) { return dr250(); }

// ---------------------------------------------------------------------------
//	gaussian_normal
// ---------------------------------------------------------------------------
//	Approximate the normal distribution using the polar form of the
//  Box-Muller transformation.
//	This is a better approximation and faster algorithm than the  
//  central limit theorem method.
//
double gaussian_normal( void )
{
	static bool use_saved = false;	//	boolean really, now member variables
	static double saved_val;
    
	double r = 1., fac, v1, v2;
	
	if ( use_saved )
	{
		use_saved = false;
		return saved_val;
	}    
    else            
	{
		v1 = 2. * uniform() - 1.;
		v2 = 2. * uniform() - 1.;
		r = v1*v1 + v2*v2;
		while( r >= 1. )
		{
			// v1 = 2. * uniform() - 1.;
            // actually may only need one new uniform sample
			v1 = v2;
			v2 = 2. * uniform() - 1.;
			r = v1*v1 + v2*v2;
		}
        
		fac = std::sqrt( -2. * std::log(r) / r );
		saved_val = v1 * fac;
		use_saved = true;
		return v2 * fac;
	}
}



// ---------------------------------------------------------------------------
//  apply protoype filter
// ---------------------------------------------------------------------------
//  Static local function for obtaining a prototype Filter
//  to use in Oscillator construction. Eventually, allow
//  external (client) specification of the Filter prototype.
//
static inline double apply_filter( double sample )
{
    //  Chebychev order 3, cutoff 500, ripple -1.
    //
    //  Coefficients obtained from http://www.cs.york.ac.uk/~fisher/mkfilter/
    //  Digital filter designed by mkfilter/mkshape/gencode   A.J. Fisher
    //
    static const double Gain = 4.663939184e+04;
    static const double ExtraScaling = 6.;
    static const double MaCoefs[] = { 1., 3., 3., 1. }; 
    static const double ArCoefs[] = { 1., -2.9258684252, 2.8580608586, -0.9320209046 };
    
    static Loris::Filter proto( MaCoefs, MaCoefs + 4, ArCoefs, ArCoefs + 4, ExtraScaling/Gain );


    
    return proto.apply( sample );
}

static 
void generate_randi( unsigned int decimation, Fastsynth_Float_Type * output, int howmany )
{
    static int step = decimation;
    static Fastsynth_Float_Type value = gaussian_normal();
    static Fastsynth_Float_Type dvalue = (gaussian_normal() - value) / step;
	
    while ( howmany-- > 0 ) *output++ = 0;
//    {
//		/* compute the output sample */
//		*output++ = apply_filter( value );
//		
//		/* update the noise sample */
//		if ( --step <= 0 )
//		{
//            step = decimation;	
//            dvalue = (gaussian_normal() - value) / step;
//		}
//		value += dvalue;
//	}	
}
