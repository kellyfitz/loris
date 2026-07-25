# Loris

[![CI](https://github.com/kellyfitz/loris/actions/workflows/ci.yml/badge.svg)](https://github.com/kellyfitz/loris/actions/workflows/ci.yml)

Loris is an open source C++ class library implementing analysis, manipulation,
and synthesis of digitized sounds using the Reassigned Bandwidth-Enhanced
Additive Sound Model. It includes a C++ class library, a Python module, a
C-linkable interface, and command line utilities.

For more information about Loris and the Reassigned Bandwidth-Enhanced Additive
Model, contact the developers at <loris@cerlsoundgroup.org>, or visit them at
<http://www.cerlsoundgroup.org/Loris/>.

## Installation

Loris builds with CMake (3.24 or newer). For detailed configuration and
installation instructions, see [INSTALL](INSTALL). Briefly:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build
cmake --install build
```

### Requirements

- A modern, reasonably standard-compliant C++ compiler. Loris requires C++17.
- [SWIG](https://www.swig.org), needed only to build the scripting extensions
  such as the Python module. Also available via `pip install swig`.
- [FFTW](https://www.fftw.org), optional but recommended for best performance.
  FFTW is covered by its own license and copyright, and is entirely separate
  from Loris. Loris builds and runs without it, using a bundled FFT, at the
  cost of slower infrequent non-power-of-two transforms.

Loris is built and tested on Linux and macOS on every push; see
[.github/workflows/ci.yml](.github/workflows/ci.yml) for the exact packages
each platform needs.

## Documentation

For documentation, please see the files in the [doc](doc) subdirectory. With
Doxygen installed, `cmake --build build --target docs` generates the API
documentation.

For a list of major changes to Loris, organized by release number, please see
[NEWS](NEWS).

Maintainers: for the steps to cut a tagged release, see
[RELEASING.md](RELEASING.md).

## Copyright and license

Loris is Copyright (c) 1999-2026 by Kelly Fitz and Lippold Haken.

Loris is free software; you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY, without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the file [COPYING](COPYING) or the GNU General Public
License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA.

<loris@cerlsoundgroup.org>
<http://www.cerlsoundgroup.org/Loris/>
