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
# Configuration summary
#----------------------------------------------------------------
if(LORIS_HAVE_FFTW3)
    message(STATUS "FFTW support: Enabled (${FFTW3_LIBRARY})")
else()
    message(STATUS "FFTW support: Disabled, using the bundled FFT")
endif()
message(STATUS "Command line utilities: ${LORIS_UTILS}")
