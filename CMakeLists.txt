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

cmake_minimum_required(VERSION 3.24)

project(Loris VERSION 1.9 LANGUAGES C CXX)

#----------------------------------------------------------------
# Version components, substituted into src/loris.h.in
# (keep consistent with the project() version above)
#----------------------------------------------------------------
set(LORIS_MAJOR_VERSION ${PROJECT_VERSION_MAJOR})
set(LORIS_MINOR_VERSION ${PROJECT_VERSION_MINOR})
set(LORIS_SUBMINOR_VERSION "")
set(LORIS_PRERELEASE_STR "development")
set(LORIS_VERSION_STR "Loris ${PROJECT_VERSION}${LORIS_PRERELEASE_STR}")

#----------------------------------------------------------------
# Options
#----------------------------------------------------------------
option(LORIS_FFTW "Use the FFTW library if available (otherwise use the bundled FFT)" ON)
option(LORIS_UTILS "Build the command line utilities" ON)
option(LORIS_PYTHON "Build the SWIG Python module" ON)
option(LORIS_DEBUG "Enable internal Loris debugging code (not recommended)" OFF)

#----------------------------------------------------------------
# Look for FFTW3. Optional: without it, Loris uses the bundled
# FFT (fftsg.c), and infrequent non-power-of-two DFTs are slower.
#----------------------------------------------------------------
if(LORIS_FFTW)
    find_path(FFTW3_INCLUDE_DIR fftw3.h HINTS /opt/homebrew/include /usr/local/include)
    find_library(FFTW3_LIBRARY fftw3 HINTS /opt/homebrew/lib /usr/local/lib)
    if(FFTW3_INCLUDE_DIR AND FFTW3_LIBRARY)
        set(LORIS_HAVE_FFTW3 TRUE)
    else()
        message(WARNING "Not using the FFTW library. Infrequent non-power-of-two DFTs will be slow.")
    endif()
endif()

add_subdirectory(src)

if(LORIS_UTILS)
    add_subdirectory(utils)
endif()

#----------------------------------------------------------------
# Python module (SWIG). Python and SWIG are located here at the top
# level so their imported targets are visible in scripting/ and test/.
# With the project .venv first on PATH, both are found there.
#----------------------------------------------------------------
if(LORIS_PYTHON)
    # honor an activated virtual environment (VIRTUAL_ENV set): the module
    # is then built for and installed into that venv's site-packages
    if(DEFINED ENV{VIRTUAL_ENV})
        set(Python3_FIND_VIRTUALENV ONLY)
    endif()
    find_package(Python3 COMPONENTS Interpreter Development.Module)
    find_package(SWIG 4.0)
    if(Python3_FOUND AND SWIG_FOUND)
        add_subdirectory(scripting)
        message(STATUS "Python module: ${Python3_EXECUTABLE} -> ${Python3_SITEARCH}")
    else()
        message(WARNING "Cannot find Python and/or SWIG. Python module cannot be built.")
    endif()
endif()

# include(CTest) defines BUILD_TESTING (default ON)
include(CTest)
if(BUILD_TESTING)
    add_subdirectory(test)
endif()

# optional `docs` target, when doxygen is available
add_subdirectory(doc)

#----------------------------------------------------------------
# Configuration summary
#----------------------------------------------------------------
if(LORIS_HAVE_FFTW3)
    message(STATUS "FFTW support: Enabled (${FFTW3_LIBRARY})")
else()
    message(STATUS "FFTW support: Disabled, using the bundled FFT")
endif()
message(STATUS "Command line utilities: ${LORIS_UTILS}")
