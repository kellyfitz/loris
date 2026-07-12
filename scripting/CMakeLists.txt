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

include(UseSWIG)

# loris.i %includes the auxiliary lorisXXX.i files; USE_SWIG_DEPENDENCIES
# makes the wrapper regenerate when any of them changes
set_property(SOURCE loris.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE loris.i PROPERTY SWIG_MODULE_NAME loris)
set_property(SOURCE loris.i PROPERTY USE_SWIG_DEPENDENCIES TRUE)

# the target cannot be named "loris" (that is the C++ library), so name
# it pyloris and set OUTPUT_NAME so the built module is _loris + loris.py
swig_add_library(pyloris LANGUAGE python SOURCES loris.i)
# when installed to <prefix>/lib/pythonX.Y/site-packages, libloris lives
# two levels up in <prefix>/lib; @loader_path covers macOS, $ORIGIN Linux
set_target_properties(pyloris PROPERTIES
    OUTPUT_NAME loris
    SWIG_USE_TARGET_INCLUDE_DIRECTORIES TRUE
    INSTALL_RPATH "@loader_path/../..;$ORIGIN/../.."
)

target_include_directories(pyloris PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pyloris PRIVATE loris Python3::Module)
target_compile_options(pyloris PRIVATE -Wno-comment)

# install the extension module and its Python shim into the interpreter's
# site-packages (the project .venv, when configured with the venv on PATH)
install(TARGETS pyloris LIBRARY DESTINATION ${Python3_SITEARCH})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/loris.py DESTINATION ${Python3_SITEARCH})
