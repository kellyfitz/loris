/*
 * loris_fastsynth_main.cpp
 *
 * main() function for a utility program to render Partials
 * stored in a SDIF file, optionally with dilation and amplitude
 * and frequency scaling by a constant factor.
 *
 * Kelly Fitz on 9/23/11.
 * loris@cerlsoundgroup.org
 *
 * http://www.cerlsoundgroup.org/Loris/
 *
 */
#include <algorithm>
using std::for_each;

#include <cmath>
using std::log;

#include <cstdlib>
using std::strtod;

#include <iostream>
using std::cout;
using std::endl;

#include <stdexcept>
using std::domain_error;

#include <string>
using std::string;

#include <vector>
using std::vector;

#include "BlockSynthBwe.h"
#include "BlockSynthReader.h"

#include <AiffFile.h>
#include <Collator.h>
#include <Dilator.h>
#include <Marker.h>
#include <PartialList.h>
#include <PartialUtils.h>
#include <SdifFile.h>
#include <SpcFile.h>
const unsigned int Fastsynth_BlockSize_samples = 128;

using namespace Loris;

//  function prototypes
void parseArguments(int nargs, char *args[]);
void printUsage(const char *programName);

//  global state
//  (Geez, this is lousy programming. I'd flunk my students for doing this.)
double Rate = 44100;
double FreqScale = 1.;
double AmpScale = 1.;
double BwScale = 1.;
string Outname = "synth.aiff";
vector<double> marker_times, cmdline_times;

int
main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printUsage(argv[0]);
        return 1;
    }

    //  get the filename and its suffix
    string filename(argv[1]);
    string suffix = filename.substr(filename.rfind('.') + 1);

    // ----------- read Partials and Markers ---------------

    PartialList partials;
    std::vector<Marker> markers;
    double midiNN = 0;

    if (suffix == "sdif")
    {
        try
        {
            SdifFile f(filename);
            cout << "SDIF partials file \"" << filename << "\":" << endl;
            std::pair<double, double> span = PartialUtils::timeSpan(
                f.partials().begin(), f.partials().end());
            cout << f.partials().size() << " partials spanning " << span.first;
            cout << " to " << span.second << " seconds.\n";
            partials.insert(partials.begin(), f.partials().begin(),
                            f.partials().end());
            markers.insert(markers.begin(), f.markers().begin(),
                           f.markers().end());
        }
        catch (Exception &ex)
        {
            cout << "Error reading markers from file: " << filename << "\n";
            cout << ex.what() << "\n";
            return 1;
        }
    }
    else if (suffix == "spc")
    {
        try
        {
            SpcFile f(filename);
            cout << "Spc partials file \"" << filename << "\":" << endl;
            std::pair<double, double> span = PartialUtils::timeSpan(
                f.partials().begin(), f.partials().end());
            cout << f.partials().size() << " partials spanning " << span.first;
            cout << " to " << span.second << " seconds.\n";
            cout << "MIDI note number " << f.midiNoteNumber() << endl;
            midiNN = f.midiNoteNumber();
            partials.insert(partials.begin(), f.partials().begin(),
                            f.partials().end());
            markers.insert(markers.begin(), f.markers().begin(),
                           f.markers().end());
        }
        catch (Exception &ex)
        {
            cout << "Error reading markers from file: " << filename << "\n";
            cout << ex.what() << "\n";
            return 1;
        }
    }
    else
    {
        cout << "Error -- unrecognized suffix: " << suffix << "\n";
        return 1;
    }

    //  parse the other arguments
    parseArguments(argc - 2, argv + 2);

    // ----------- dilate ---------------

    for (int i = 0; i < markers.size(); ++i)
    {
        marker_times.push_back(markers[i].time());
    }

    if (0 < cmdline_times.size())
    {
        if (!markers.empty())
        {
            cout << "Features marked in " << filename << " before dilating:\n";
            //  print out the marker_times:
            std::vector<Marker>::iterator it;
            for (it = markers.begin(); it != markers.end(); ++it)
            {
                cout << it->time() << "\t\"" << it->name() << "\"\n";
            }
        }

        if (cmdline_times.size() == marker_times.size())
        {
            cout << "Dilating partials using " << cmdline_times.size()
                 << " marked features." << endl;
            Dilator dilator(marker_times.begin(), marker_times.end(),
                            cmdline_times.begin());
            dilator.dilate(partials.begin(), partials.end());
            dilator.dilate(markers.begin(), markers.end());
        }
        else if (1 == cmdline_times.size())
        {
            double dur =
                PartialUtils::timeSpan(partials.begin(), partials.end()).second;
            cout << "Scaling duration from " << dur << " to "
                 << cmdline_times.front() << " seconds" << endl;
            Dilator dilator(&dur, (&dur) + 1, cmdline_times.begin());
            dilator.dilate(partials.begin(), partials.end());
            dilator.dilate(markers.begin(), markers.end());
        }
        else
        {
            cout << "Specified time points need to correspond to Markers "
                 << "in " << filename << ", ignoring." << endl;
            printUsage(argv[0]);
            return 1;
        }

        if (!markers.empty())
        {
            cout << "Features marked in " << filename << " after dilating:\n";
            //  print out the marker_times:
            std::vector<Marker>::iterator it;
            for (it = markers.begin(); it != markers.end(); ++it)
            {
                cout << it->time() << "\t\"" << it->name() << "\"\n";
            }
        }
    }

    if (1. != FreqScale)
    {
        cout << "Scaling partial frequencies by " << FreqScale << endl;
        PartialUtils::scaleFrequency(partials.begin(), partials.end(),
                                     FreqScale);

        if (0 != midiNN)
        {
            double newNN = midiNN + 12 * (log(FreqScale) / log(2.0));
            cout << "Adjusting Midi Note Number from " << midiNN << " to "
                 << newNN << endl;
            midiNN = newNN;
        }
    }

    if (1. != AmpScale)
    {
        cout << "Scaling partial amplitudes by " << AmpScale << endl;
        PartialUtils::scaleAmplitude(partials.begin(), partials.end(),
                                     AmpScale);
    }

    if (1. != BwScale)
    {
        cout << "Scaling partial bandwidths by " << BwScale << endl;
        PartialUtils::scaleBandwidth(partials.begin(), partials.end(), BwScale);
    }

    double TimeScale = 1;

    //  crop to remove space at the front
    std::pair<double, double> span =
        Loris::PartialUtils::timeSpan(partials.begin(), partials.end());
    //        span.first = 1;
    //        span.second = 2;
    Loris::PartialUtils::crop(partials.begin(), partials.end(), span.first,
                              span.second);
    Loris::PartialUtils::shiftTime(partials.begin(), partials.end(),
                                   -span.first);

    //  prepare a sample buffer
    const double dur_seconds =
        PartialUtils::timeSpan(partials.begin(), partials.end()).second;
    const double fade_time_seconds = Fastsynth_BlockSize_samples / Rate;
    std::vector<Fastsynth_Float_Type> samps_out(
        std::ceil(dur_seconds * TimeScale * Rate) +
        Fastsynth_BlockSize_samples + Fastsynth_BlockSize_samples);
    std::fill(samps_out.begin(), samps_out.end(), 0);

    //  prepare Partials
    Collator collator(fade_time_seconds, fade_time_seconds * 0.1);
    collator.collate(partials);

    const double Fastsynth_BlockInterval_seconds =
        Fastsynth_BlockSize_samples / Rate;
    BlockSynthReader reader(partials, Fastsynth_BlockInterval_seconds);
    BlockSynthBwe synth(Fastsynth_BlockSize_samples, Rate,
                        reader.numPartials());

    //  render the Partials
    cout << "Rendering " << partials.size() << " partials in "
         << reader.numFrames() << " frames at " << Rate << " Hz." << endl;

    Fastsynth_Float_Type *putEmHere = &(samps_out.front());
    //	for ( unsigned int blocknum = 0; blocknum < reader.numFrames();
    //++blocknum )

    double frametime = 0;
    double tinc = Fastsynth_BlockInterval_seconds / TimeScale;
    unsigned int blocknum = 0;
    while (frametime < dur_seconds)
    {

        //        BlockSynthReader::FrameType & frame = reader.getFrameAtTime(
        //        frametime );
        BlockSynthReader::FrameType &frame = reader.getFrame(blocknum++);
        frametime += tinc;

        synth.render(frame, putEmHere);

        //        if (2 == blocknum)
        //        {
        //            for ( unsigned int k = 0; k < Fastsynth_BlockSize_samples;
        //            ++k )
        //            {
        //                cout << k << ": " << putEmHere[k] << endl;;
        //            }
        //        }

        putEmHere += Fastsynth_BlockSize_samples;
    }

    cout << "Done rendering." << endl;

    AiffFile fout(samps_out, Rate);
    fout.markers() = markers;
    if (0 != midiNN)
    {
        fout.setMidiNoteNumber(midiNN);
    }

    //  export the samples
    cout << "Exporting to " << Outname << endl;
    fout.write(Outname);

    cout << "* Done." << endl;
    return 0;
}

static double
getFloatArg(const char *arg)
{
    char *endptr;
    double x = strtod(arg, &endptr);
    if (endptr == arg)
    {
        cout << "Error processing argument: " << arg << endl;
        throw domain_error("bad argument");
    }
    return x;
}

void
parseArguments(int nargs, char *args[])
{
    while (nargs > 0)
    {
        string arg = *args;
        if (arg[0] == '-')
        {
            ++args;
            --nargs;
            //  process an option
            if (arg == "-rate")
            {
                Rate = getFloatArg(*args);
                ++args;
                --nargs;
            }
            else if (arg == "-freq")
            {
                FreqScale = getFloatArg(*args);
                ++args;
                --nargs;
            }
            else if (arg == "-amp")
            {
                AmpScale = getFloatArg(*args);
                ++args;
                --nargs;
            }
            else if (arg == "-bw")
            {
                BwScale = getFloatArg(*args);
                ++args;
                --nargs;
            }
            else if (arg == "-o")
            {
                Outname = *args;
                ++args;
                --nargs;
            }
            else
            {
                cout << "Unrecognized argument: " << arg << endl;
                cout << "Ignoring the rest." << endl;
                return;
            }
        }
        else
        {
            //  all the remaining command line args
            //  should be cmdline_times
            while (nargs > 0)
            {
                cmdline_times.push_back(getFloatArg(*args));
                ++args;
                --nargs;
            }
        }
    }
}

void
printUsage(const char *programName)
{
    cout << "usage: " << programName
         << " filename.sdif [options] [cmdline_times]" << endl;
    cout << "options:" << endl;
    cout << "-rate <sample rate in Hz>" << endl;
    cout << "-freq <frequency scale factor>" << endl;
    cout << "-amp <amplitude scale factor>" << endl;
    cout << "-bw <bandwidth scale factor>" << endl;
    cout << "-o <output AIFF file name, default is synth.aiff>" << endl;
    cout << "\nOptional cmdline_times (any number) are used for dilation."
         << endl;
    cout << "If cmdline_times are specified, they must all correspond to "
         << endl;
    cout << "Markers in the SDIF file. If only a single time is" << endl;
    cout << "specified, and the SDIF file has no Markers or more" << endl;
    cout << "than one, the specified time is used as the overall duration"
         << endl;
    cout << "of the uniformly-dilated synthesis." << endl;
}
