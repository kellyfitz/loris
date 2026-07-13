/*
 *  fastsynth.h
 *  Loris
 *
 *  Fast (?) rendering of Loris Partials using streamlined unit generators.
 *
 *  Created by Kelly Fitz on 9/23/11.
 *  loris@cerlsoundgroup.org
 *
 *  http://www.cerlsoundgroup.org/Loris/
 *
 */

#include "PartialList.h"
#include <vector>

/*
 Definition of floating point type
 */
#if defined(FASTSYNTH_FLOAT_TYPE) 
typedef FASTSYNTH_FLOAT_TYPE Fastsynth_Float_Type;
#else
typedef float Fastsynth_Float_Type;
#define FASTSYNTH_FLOAT_TYPE
#endif

//const double Fastsynth_fadetime_seconds = 0.001;
const unsigned int Fastsynth_BlockSize_samples = 100;

void fastsynth( Loris::PartialList & partials, double sample_rate, 
				std::vector< Fastsynth_Float_Type > & samps_out );

/*
 generate_env_segment
 
 Generate samples of an arbitrary envelope segment, from initial and target values.
 The target value is the value that would be achieved by the envelope one sample after
 the last one generated.
 */
void generate_env_segment( Fastsynth_Float_Type ival, Fastsynth_Float_Type tval, Fastsynth_Float_Type dTime, 
                           Fastsynth_Float_Type * output, int howmany, int stride );


/*
 generate_random
 
 *** Make this fast! For now just call random.
 
 */

void generate_random( Fastsynth_Float_Type * output, int howmany, int stride );

/*
 generate_table_lookup_01
 
 Generate samples that are a function of the input, from samples of the
 function stored in a wavetable. The input is assumed to be bounded 0 to 1.
 
 
 */

void generate_table_lookup_01( Fastsynth_Float_Type * in, int in_stride,
							  Fastsynth_Float_Type const * const Table, unsigned int TableMaxIdx,
							  Fastsynth_Float_Type * output, int howmany, int stride );


// ------------------------------------
//	class BlockOscilBwe
// ------------------------------------

class BlockOscilBwe
{
private:
	unsigned int mBlockLenSamples;
	
	//	multipliers
	Fastsynth_Float_Type mOneOverBlockLen;
	Fastsynth_Float_Type mOneOverSR;
	
	//	oscillator state variables
	Fastsynth_Float_Type mPhaseRadians;
	Fastsynth_Float_Type mAmp;	
	Fastsynth_Float_Type mFreqRadiansPerSamp;
	Fastsynth_Float_Type mBw;
	
	
public:
	
	//	lifecycle
	
	//	construct from block length and sample rate, which must then be
	//	fixed for the lifetime of this oscillator.
	BlockOscilBwe( unsigned int blockLenSamples, Fastsynth_Float_Type sampleRate );
	
	
	//	state access and mutation
	
	//	set oscillator state from a Breakpoint
	void set( const Loris::Breakpoint & bp );
	
	
};	//	end of class BlockOscilBwe
