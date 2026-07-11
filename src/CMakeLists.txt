# Loris is Copyright (c) 1999-2026 by Kelly Fitz and Lippold Haken
# <loris@cerlsoundgroup.org>
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

# loris.h is generated from loris.h.in, substituting the version macros
configure_file(loris.h.in ${CMAKE_CURRENT_BINARY_DIR}/loris.h @ONLY)

# source code for Loris classes
set(CPP_SRC
    AiffData.C
    AiffFile.C
    Analyzer.C
    AssociateBandwidth.C
    BigEndian.C
    Breakpoint.C
    BreakpointUtils.C
    Channelizer.C
    Collator.C
    Dilator.C
    Distiller.C
    Envelope.C
    F0Estimate.C
    LorisExceptions.C
    Filter.C
    FourierTransform.C
    FrequencyReference.C
    Fundamental.C
    Harmonifier.C
    ImportLemur.C
    KaiserWindow.C
    LinearEnvelope.C
    Marker.C
    Morpher.C
    NoiseGenerator.C
    Notifier.C
    Oscillator.C
    Partial.C
    PartialBuilder.C
    PartialList.C
    PartialUtils.C
    phasefix.C
    ReassignedSpectrum.C
    Resampler.C
    SdifFile.C
    Sieve.C
    SpcFile.C
    SpectralPeakSelector.C
    SpectralSurface.C
    Synthesizer.C
    fftsg.c
)

# source code for the procedural (C) interface
set(PI_SRC
    lorisAnalyzer_pi.C
    lorisBpEnvelope_pi.C
    lorisException_pi.C
    lorisNonObj_pi.C
    lorisPartialList_pi.C
    lorisUtilities_pi.C
)

add_library(loris SHARED ${CPP_SRC} ${PI_SRC})

# continue the libtool -version-info 13:0:0 lineage
set_target_properties(loris PROPERTIES VERSION 13.0.0 SOVERSION 13)

# the binary dir (generated loris.h) must precede the source dir, so that
# a stale in-tree loris.h left by an autotools build cannot shadow it
target_include_directories(loris PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_options(loris PRIVATE -Wno-comment)

if(LORIS_HAVE_FFTW3)
    target_compile_definitions(loris PRIVATE HAVE_FFTW3_H=1)
    target_include_directories(loris PRIVATE ${FFTW3_INCLUDE_DIR})
    target_link_libraries(loris PRIVATE ${FFTW3_LIBRARY})
endif()

if(LORIS_DEBUG)
    target_compile_definitions(loris PRIVATE Debug_Loris)
endif()

if(NOT APPLE)
    target_link_libraries(loris PRIVATE m)
endif()

# installed Loris header files
set(LORIS_HEADERS
    AiffFile.h
    Analyzer.h
    BreakpointEnvelope.h
    Breakpoint.h
    BreakpointUtils.h
    Channelizer.h
    Collator.h
    Dilator.h
    Distiller.h
    Envelope.h
    Exception.h
    F0Estimate.h
    Filter.h
    FourierTransform.h
    FrequencyReference.h
    Fundamental.h
    Harmonifier.h
    ImportLemur.h
    KaiserWindow.h
    LinearEnvelope.h
    LorisExceptions.h
    Marker.h
    Morpher.h
    NoiseGenerator.h
    Notifier.h
    Oscillator.h
    Partial.h
    PartialList.h
    PartialPtrs.h
    PartialUtils.h
    PtrCopyOnWrite.h
    ReassignedSpectrum.h
    Resampler.h
    SdifFile.h
    Sieve.h
    SpcFile.h
    SpectralSurface.h
    Synthesizer.h
)

include(GNUInstallDirs)
install(TARGETS loris LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/loris.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES ${LORIS_HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/loris)
