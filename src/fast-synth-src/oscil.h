#ifndef INCLUDE_OSCIL_H
#define INCLUDE_OSCIL_H
/* oscil.h
 *
 * MODIFIED from oscil.h of TUGs: Teaching Unit Generators 
 * http://sourceforge.net/projects/tugs
 * 
 * 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 * Struct definition and sample generation
 * function for wavetable oscillator.
 */

/*
 Definition of floating point type
 */
#if defined(FASTSYNTH_FLOAT_TYPE) 
typedef FASTSYNTH_FLOAT_TYPE Fastsynth_Float_Type;
#else
typedef float Fastsynth_Float_Type;
#define FASTSYNTH_FLOAT_TYPE
#endif


typedef struct
{
    int N;							  /* wavetable length */
    Fastsynth_Float_Type * table;     /* wavetable */
    Fastsynth_Float_Type phase;       /* wavetable phase stored in samples */
    
    Fastsynth_Float_Type OneOverSR;   /* inverse of sample rate (1/Hz) */
    
} oscil_info;

/*
    create_oscil_info

    Allocate and return a new oscil_info 
    structure using the specified wavetable.
    init_phase specifies the starting phase
    in radians.
*/
oscil_info * create_oscil_info( Fastsynth_Float_Type * wavetable, int N, 
								Fastsynth_Float_Type init_phase, Fastsynth_Float_Type sample_rate );

/*
    delete_oscil_info

    Free the memory associated with a oscil_info
    structure.
*/
void delete_oscil_info( oscil_info * info );

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
                      Fastsynth_Float_Type * output, int howmany, int stride );

#endif
