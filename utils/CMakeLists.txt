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

# sources are utils/loris_*.C; the built programs are loris-*
set(LORIS_UTILS_PROGRAMS
    analyze
    dilate
    mark
    spewmarkers
    synthesize
    unmark
)

include(GNUInstallDirs)

foreach(util IN LISTS LORIS_UTILS_PROGRAMS)
    add_executable(loris-${util} loris_${util}.C)
    target_link_libraries(loris-${util} PRIVATE loris)
    target_compile_options(loris-${util} PRIVATE -Wno-comment)
    # find libloris in <prefix>/lib next to <prefix>/bin after install
    set_target_properties(loris-${util} PROPERTIES
        INSTALL_RPATH "@loader_path/../lib;$ORIGIN/../lib")
    install(TARGETS loris-${util} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endforeach()

# loris-fastsynth: the fast block synthesizer (work in progress; built
# but not installed). Renders in double precision, as the original
# Xcode "Loris fast synthesizer" target did.
set(FASTSYNTH_DIR ${CMAKE_SOURCE_DIR}/src/fast-synth-src)
add_executable(loris-fastsynth
    loris_fastsynth_main.cpp
    ${FASTSYNTH_DIR}/BlockOscillator.cpp
    ${FASTSYNTH_DIR}/BlockSynthBwe.cpp
    ${FASTSYNTH_DIR}/BlockSynthReader.cpp
    ${FASTSYNTH_DIR}/fastsynth.cpp
    ${FASTSYNTH_DIR}/multiplyAdd.c
    ${FASTSYNTH_DIR}/oscil.c
    ${FASTSYNTH_DIR}/r250.c
    ${FASTSYNTH_DIR}/randlcg.c
)
target_include_directories(loris-fastsynth PRIVATE ${FASTSYNTH_DIR})
target_link_libraries(loris-fastsynth PRIVATE loris)
target_compile_options(loris-fastsynth PRIVATE -Wno-comment)
target_compile_definitions(loris-fastsynth PRIVATE FASTSYNTH_FLOAT_TYPE=double)
