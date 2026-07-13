/*
 *  fastsynth.cpp
 *  Loris
 *
 *  Created by Kelly Fitz on 9/23/11.
 *  loris@cerlsoundgroup.org
 *
 *  http://www.cerlsoundgroup.org/Loris/
 *
 */

#include "fastsynth.h"

#include "Breakpoint.h"
#include "BreakpointUtils.h"
#include "Partial.h"
#include "PartialList.h"
#include "PartialUtils.h"
#include "Resampler.h"

#include "BlockSynthBwe.h"
#include "BlockSynthReader.h"

// #include <iostream>

extern "C" {
    #include "oscil.h"
	#include "multiplyAdd.h"
}

#include <vector>

#include <cstring> /* for bzero */
#include <cmath>    /* for cos */

#include <cstdlib>

const Fastsynth_Float_Type TwoPi = 2 * 3.14159265358979324;


using namespace Loris;
using std::vector;

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
	Fastsynth_Float_Type * table = (Fastsynth_Float_Type *)malloc( (N*1)*sizeof(Fastsynth_Float_Type) );
	
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
	Fastsynth_Float_Type * table = (Fastsynth_Float_Type *)malloc( (N*1)*sizeof(Fastsynth_Float_Type) );
	
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
	Fastsynth_Float_Type * table = (Fastsynth_Float_Type *)malloc( (N*1)*sizeof(Fastsynth_Float_Type) );
	
	/* fill it with a cosine wave */
	for ( i = 0; i < N+1; ++i )
    {
		table[i] = sqrt( 2.0 * (Fastsynth_Float_Type(i) / N) );
    }
	return table;
}

static const unsigned int TabSize = 1024;
static Fastsynth_Float_Type * CosineTab = make_cos_table( TabSize );
static Fastsynth_Float_Type * CarrierAmpTab = make_carrier_amp_table( TabSize );
static Fastsynth_Float_Type * ModIndexTab = make_mod_index_table( TabSize );

static const Fastsynth_Float_Type Radians_to_wavetable_phase = TabSize / TwoPi;

static const unsigned int BufSize = 1024;
//static Fastsynth_Float_Type AmpBuffer[ BufSize ];
//static Fastsynth_Float_Type FreqBuffer[ BufSize ];
//static Fastsynth_Float_Type BwBuffer[ BufSize ];
//static Fastsynth_Float_Type NoiseBuffer[ BufSize ];
//static Fastsynth_Float_Type ModBuffer[ BufSize ];


// ------------------------------------------------------------------------------
// fastsynth
// ------------------------------------------------------------------------------
void fastsynth( PartialList & partials, Fastsynth_Float_Type sample_rate, vector< Fastsynth_Float_Type > & samps_out )
{
    //  prepare Partials
	const double Fastsynth_BlockInterval_seconds = Fastsynth_BlockSize_samples / sample_rate;
	
    BlockSynthReader reader( partials, Fastsynth_BlockInterval_seconds );
    
	BlockSynthBwe synth( Fastsynth_BlockSize_samples, sample_rate, reader.numPartials() );
	
	
	
         

//    Resampler quantizer( Fastsynth_BlockInterval_seconds );
//    quantizer.setPhaseCorrect( true );		
//	
//	double tmax = 0;															  
//    for ( PartialList::iterator it = partials.begin(); it != partials.end(); ++it )
//    {
//        Partial & p = *it;
//
//        //  use a Resampler to quantize the Breakpoint times and 
//        //  correct the phases:
//        quantizer.quantize( p );
//		
//		p.fadeIn( Fastsynth_BlockInterval_seconds );
//		p.fadeOut( Fastsynth_BlockInterval_seconds );
//		
//		tmax = std::max( tmax, p.endTime() );
//	}


	//const double OneOverInterval = 1.0 / Fastsynth_BlockInterval_seconds;
	const Fastsynth_Float_Type dur_seconds = Loris::PartialUtils::timeSpan( partials.begin(), partials.end() ).second;    
	
	const unsigned int NumBlocks = 1 + (unsigned int)((dur_seconds / Fastsynth_BlockInterval_seconds) + 0.5 /* round */);
	
	Fastsynth_Float_Type * putEmHere = &( samps_out.front() );
	for ( unsigned int blocknum = 0; blocknum < NumBlocks; ++blocknum )
	{
        BlockSynthReader::FrameType & frame = reader.getFrame( blocknum );
        
		synth.render( frame, putEmHere );
		putEmHere += Fastsynth_BlockSize_samples;
	}	
	
//	const unsigned int bpMatrixNumPartials = partials.size();
//	const unsigned int bpMatrixNumBlocks = (unsigned int)((tmax * OneOverInterval) + 0.5 /* round */);
//
//	Breakpoint bpMatrix[ bpMatrixNumPartials ][ bpMatrixNumBlocks ];
//	
//	oscil_info * oscArray[ bpMatrixNumPartials ];
//
//	unsigned int bpMatrixRowNum = 0;
//	
//	//	Fill in the Breakpoint matrix and allocate oscillators													
//	for ( PartialList::iterator it = partials.begin(); it != partials.end(); ++it )
//	{		
//		Partial::const_iterator pos =it->begin();
//		unsigned int bpMatrixColNum = std::floor( (pos.time() * OneOverInterval) + 0.5 /* round */ );
//		while( pos != it->end() )
//		{
//			bpMatrix[ bpMatrixRowNum ][ bpMatrixColNum++ ] = pos.breakpoint();
//			
//			++pos; 
//		}
//		
//		//  prepare generators
//		oscArray[ bpMatrixRowNum ] = create_oscil_info( CosineTab, TabSize, 0, sample_rate );
//		
//		++bpMatrixRowNum;
//    }
//        

    //  render Partials block by block (column by column)
	

	
//    Fastsynth_Float_Type * bufferBegin = &( samps_out.front() );
//	const unsigned int nsamps = Fastsynth_BlockSize_samples;
//	const double dTime = 1. / nsamps;
//
//	for ( unsigned int col = 0; col < bpMatrixNumBlocks; ++col )
//	{
//		unsigned int currentSamp = col * Fastsynth_BlockSize_samples;
//		Fastsynth_Float_Type * putEmHere = bufferBegin+currentSamp;
//		
//		
//		for ( unsigned int row = 0; row < bpMatrixNumPartials; ++row )
//		{
//			oscil_info * osc = oscArray[row];
//			
//			Breakpoint & curBp = bpMatrix[row][col];
//			Breakpoint & nxtBp = bpMatrix[row][col+1];
//			
//            //  skip over all of this if all samples will be zero
//            if ( 0 < curBp.amplitude() || 0 < nxtBp.amplitude() )
//            {    
//				
//#if defined(OSC_CLASS)
//				
//				if ( 0 == curBp.amplitude() )
//				{
//					osc.set( curBp );
//				}
//				
//				osc.oscillate( nxtBp, putEmHere, nsamps, 1 );
//#endif
//		
//				
//				//  make amp and freq envelope segment
//                generate_env_segment( curBp.amplitude(), nxtBp.amplitude(), dTime, AmpBuffer, nsamps, 1 );
//                generate_env_segment( curBp.frequency(), nxtBp.frequency(), dTime, FreqBuffer, nsamps, 1 );	
//				
//#undef BandwidthEnhanced				
//#if defined(BandwidthEnhanced)
//                generate_env_segment( curBp.bandwidth(), nxtBp.bandwidth(), dTime, BwBuffer, nsamps, 1 );				
//				generate_random( NoiseBuffer, nsamps, 1 );
//				
//				//	compose the modulator
//				generate_table_lookup_01( BwBuffer, 1, ModIndexTab, TabSize, ModBuffer, nsamps, 1 );
//				generate_mult( NoiseBuffer, 1, ModBuffer, 1, NoiseBuffer, nsamps, 1 );
//				//	NoiseBuffer stores the stochastic modulator, noise times mod index
//				
//				generate_table_lookup_01( BwBuffer, 1, CarrierAmpTab, TabSize, ModBuffer, nsamps, 1 );
//				//	ModBuffer stores carrier amp
//				
//				generate_add( NoiseBuffer, 1, ModBuffer, 1, ModBuffer, nsamps, 1 );
//				//	ModBuffer stores the AM modulation signal, carrier plus noise times mod index
//				
//				generate_mult( AmpBuffer, 1, ModBuffer, 1, AmpBuffer, nsamps, 1 );
//				//	AmpBuffer stores the modulated amplitude signal
//				
//#endif /* defined(BandwidthEnhanced) */
//				
//				
//                //  reset oscillator phase if current amp is zero
//                if ( 0 == curBp.amplitude() )
//                {
//                    osc->phase = curBp.phase() * Radians_to_wavetable_phase;
//                    
//                    // phase has to be non-negative
//                    while ( 0 > osc->phase )
//                    {
//                        osc->phase += TwoPi;
//                    }                    
//                }
//				
//                // oscillate
//                // *** modified to accumulate samples, instead of overwriting the buffer contents ***
//                generate_oscil( osc, AmpBuffer, 1, FreqBuffer, 1, putEmHere, nsamps, 1 );
//				
//		
//				
//				
//			}
//			
//        }
//		
//    } 
//		

		
//	//	clean up
//	for ( unsigned int row = 0; row < bpMatrixNumPartials; ++row )
//	{
//		delete_oscil_info( oscArray[row] );
//	}
    
}


/*
 generate_env_segment
 
 Generate samples of an arbitrary envelope segment, from initial and target values.
 The target value is the value that would be achieved by the envelope one sample after
 the last one generated.
 
 Passing dTime allows a little extra efficiency, == one over howmany.
 
 
 *** Make this a generator. 
 
 */
void generate_env_segment( Fastsynth_Float_Type ival, Fastsynth_Float_Type tval, Fastsynth_Float_Type dTime, 
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

void generate_random( Fastsynth_Float_Type * output, int howmany, int stride )
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

void generate_table_lookup_01( Fastsynth_Float_Type * in, int in_stride,
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
// -----------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------








// -----------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------






#if 0		
for ( PartialList::iterator it = partials.begin(); it != partials.end(); ++it )
{                
	// std::cout << it->label() << std::endl;
	
	Partial::iterator nextBpPos = it->begin();
	unsigned int currentSamp( (nextBpPos.time() * sample_rate) + 0.5 );   //  cheap rounding
	
	
	Partial::iterator curBpPos = nextBpPos++;
	while( nextBpPos != it->end() )
	{
		//  determine number of samples to synthesize in this segment
		const unsigned int tgtSamp( (nextBpPos.time() * sample_rate) + 0.5 );   //  cheap rounding
		
		//  skip over all of this if all samples will be zero
		if ( 0 < nextBpPos.breakpoint().amplitude() || 0 < curBpPos.breakpoint().amplitude() )
		{            
			const unsigned int nsamps = tgtSamp - currentSamp;
			const double dTime = 1. / nsamps;
			
			//  make amp and freq envelope segment
			generate_env_segment( curBpPos.breakpoint().amplitude(), nextBpPos.breakpoint().amplitude(), dTime, AmpBuffer, nsamps, 1 );
			generate_env_segment( curBpPos.breakpoint().frequency(), nextBpPos.breakpoint().frequency(), dTime, FreqBuffer, nsamps, 1 );
			
#undef BandwidthEnhanced				
#if defined(BandwidthEnhanced)
			generate_env_segment( curBpPos.breakpoint().bandwidth(), nextBpPos.breakpoint().bandwidth(), dTime, BwBuffer, nsamps, 1 );				
			generate_random( NoiseBuffer, nsamps, 1 );
			
			//	compose the modulator
			generate_table_lookup_01( BwBuffer, 1, ModIndexTab, TabSize, ModBuffer, nsamps, 1 );
			generate_mult( NoiseBuffer, 1, ModBuffer, 1, NoiseBuffer, nsamps, 1 );
			//	NoiseBuffer stores the stochastic modulator, noise times mod index
			
			generate_table_lookup_01( BwBuffer, 1, CarrierAmpTab, TabSize, ModBuffer, nsamps, 1 );
			//	ModBuffer stores carrier amp
			
			generate_add( NoiseBuffer, 1, ModBuffer, 1, ModBuffer, nsamps, 1 );
			//	ModBuffer stores the AM modulation signal, carrier plus noise times mod index
			
			generate_mult( AmpBuffer, 1, ModBuffer, 1, AmpBuffer, nsamps, 1 );
			//	AmpBuffer stores the modulated amplitude signal
			
#endif /* defined(BandwidthEnhanced) */
			
			
			//  reset oscillator phase if current amp is zero
			if ( 0 == curBpPos.breakpoint().amplitude() )
			{
				osc->phase = curBpPos.breakpoint().phase() * Radians_to_wavetable_phase;
				
				// phase has to be non-negative
				while ( 0 > osc->phase )
				{
					osc->phase += 2*Pi;
				}                    
			}
			
			// oscillate
			// *** modified to accumulate samples, instead of overwriting the buffer contents ***
			generate_oscil( osc, AmpBuffer, 1, FreqBuffer, 1, bufferBegin+currentSamp, nsamps, 1 );
		}
		
		//  advance
		curBpPos = nextBpPos++;            
		currentSamp = tgtSamp;
	}
	
}



// -----------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------



for ( unsigned int partialNum = 0; partialNum < mOscils.size(); ++partialNum )
{
    BlockOscillator & osc = mOscils[partialNum];
    
    Loris::Breakpoint & nxtBp = mBpFrames[frameNum][partialNum];
    
    //  skip over all of this if all samples will be zero
    if ( 0 < nxtBp.amplitude() || 0 < osc.amplitude() )
    {    
        
        
        //  make amp and freq envelope segment
        //generate_env_segment( curBp.amplitude(), nxtBp.amplitude(), mOneOverBlockLen, AmpBuffer, mBlockLenSamples, 1 );
        //generate_env_segment( curBp.frequency(), nxtBp.frequency(), mOneOverBlockLen, FreqBuffer, mBlockLenSamples, 1 );	
        
#undef BandwidthEnhanced				
#if defined(BandwidthEnhanced)
        generate_env_segment( curBp.bandwidth(), nxtBp.bandwidth(), mOneOverBlockLen, BwBuffer, mBlockLenSamples, 1 );				
        generate_random( NoiseBuffer, nsamps, 1 );
        
        //	compose the modulator
        generate_table_lookup_01( BwBuffer, 1, ModIndexTab, TabSize, ModBuffer, mBlockLenSamples, 1 );
        generate_mult( NoiseBuffer, 1, ModBuffer, 1, NoiseBuffer, mBlockLenSamples, 1 );
        //	NoiseBuffer stores the stochastic modulator, noise times mod index
        
        generate_table_lookup_01( BwBuffer, 1, CarrierAmpTab, TabSize, ModBuffer, mBlockLenSamples, 1 );
        //	ModBuffer stores carrier amp
        
        generate_add( NoiseBuffer, 1, ModBuffer, 1, ModBuffer, mBlockLenSamples, 1 );
        //	ModBuffer stores the AM modulation signal, carrier plus noise times mod index
        
        generate_mult( AmpBuffer, 1, ModBuffer, 1, AmpBuffer, mBlockLenSamples, 1 );
        //	AmpBuffer stores the modulated amplitude signal
        
#endif /* defined(BandwidthEnhanced) */
        
        
        /*  reset oscillator phase if current amp is zero
         if ( 0 == osc.amplitude() )
         {
         osc.phase = curBp.phase() * mRadians2WavetablePhase;
         
         // phase has to be non-negative
         while ( 0 > osc.phase )
         {
         osc.phase += TwoPi;
         }                    
         }
         */
        
        // oscillate
        // *** modified to accumulate samples, instead of overwriting the buffer contents ***
        //generate_oscil( &osc, AmpBuffer, 1, FreqBuffer, 1, putEmHere, mBlockLenSamples, 1 );
        
        
        osc.oscillate( nxtBp, putEmHere );
        
    }





#endif