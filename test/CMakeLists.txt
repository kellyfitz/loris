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

# Each test runs in its own scratch directory under work/, so test
# output never lands in the source tree, and tests can run in parallel
# (no test reads a file written by another). The tests locate their
# input data (clarinet.aiff etc.) via the srcdir environment variable,
# same as under the automake test harness.

function(loris_add_test name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE loris)
    target_compile_options(${name} PRIVATE -Wno-comment)
    set(_workdir ${CMAKE_CURRENT_BINARY_DIR}/work/${name})
    file(MAKE_DIRECTORY ${_workdir})
    add_test(NAME ${name} COMMAND ${name} WORKING_DIRECTORY ${_workdir})
    set_tests_properties(${name} PROPERTIES
        ENVIRONMENT "srcdir=${CMAKE_CURRENT_SOURCE_DIR}")
endfunction()

# simple morphing test
loris_add_test(test_cpp morphtest.C)

# procedural interface test (C source, links the C++ library)
loris_add_test(test_pi pitest.c)
set_target_properties(test_pi PROPERTIES LINKER_LANGUAGE CXX)

# unit tests
loris_add_test(test_aiff test_Aiff.C)
loris_add_test(test_crop test_Cropper.C)
loris_add_test(test_distiller test_Distiller.C)
loris_add_test(test_filter test_Filter.C)
loris_add_test(test_fundamental test_Fundamental.C)
loris_add_test(test_identity test_Identity.C)
loris_add_test(test_morpher test_Morpher.C)
loris_add_test(test_partial test_Partial.C)
loris_add_test(test_resample test_Resampler.C)
loris_add_test(test_sdiffile test_SdifFile.C)
loris_add_test(test_synthesizer test_Synthesizer.C)

# Python module test: morphtest.py against the just-built module in
# build/scripting (PYTHONPATH holds both _loris and loris.py there)
if(TARGET pyloris)
    set(_workdir ${CMAKE_CURRENT_BINARY_DIR}/work/run_pytest)
    file(MAKE_DIRECTORY ${_workdir})
    add_test(NAME run_pytest
        COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/morphtest.py
        WORKING_DIRECTORY ${_workdir})
    set_tests_properties(run_pytest PROPERTIES
        ENVIRONMENT "srcdir=${CMAKE_CURRENT_SOURCE_DIR};PYTHONPATH=$<TARGET_FILE_DIR:pyloris>")
endif()
