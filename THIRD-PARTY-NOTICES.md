# Third-Party Notices

MPQ Viewer by X08 (https://github.com/X08/MPQ-Viewer-Editor-for-Linux) includes or links against the
third-party software listed below. This file reproduces the notices those projects ask for. It covers
third-party code only; it says nothing about the license of MPQ Viewer's own source.

"MPQ", "Warcraft", "StarCraft", "Diablo", "World of Warcraft" and related names are trademarks of
Blizzard Entertainment, Inc. This project is not affiliated with or endorsed by Blizzard Entertainment.

Contents

1. [StormLib and its bundled components](#1-stormlib-and-its-bundled-components) (in the source tree, statically linked)
2. [Qt 6](#2-qt-6) (bundled in the AppImage, dynamically linked)
3. [Other libraries bundled in the AppImage](#3-other-libraries-bundled-in-the-appimage)
4. [AppImage tooling](#4-appimage-tooling)

---

## 1. StormLib and its bundled components

Location: `third_party/StormLib/` (version 9.41, upstream commit `d60b7e6`, 2026-11-18)
Upstream: https://github.com/ladislav-zezula/StormLib
Linked statically into the `mpq-viewer` executable.

### StormLib — MIT License

```
The MIT License (MIT)

Copyright (c) 1999-2013 Ladislav Zezula

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
```

The same text is in `third_party/StormLib/LICENSE`. StormLib's own source files (including `adpcm/`,
`huffman/` and `pklib/`) carry Ladislav Zezula's copyright headers; `huffman/` also credits ShadowFlare,
and `adpcm/` thanks Tom Amigo for releasing his sources.

StormLib in turn contains the following third-party components under `third_party/StormLib/src/`:

### zlib (`zlib/`)

```
  Copyright (C) 1995-2010 Jean-loup Gailly and Mark Adler

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  Jean-loup Gailly        Mark Adler
  jloup@gzip.org          madler@alumni.caltech.edu
```

### bzip2 / libbzip2 (`bzip2/`)

Copyright (C) 1996-2007 Julian Seward <jseward@bzip.org>

```
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

2. The origin of this software must not be misrepresented; you must
   not claim that you wrote the original software.  If you use this
   software in a product, an acknowledgment in the product
   documentation would be appreciated but is not required.

3. Altered source versions must be plainly marked as such, and must
   not be misrepresented as being the original software.

4. The name of the author may not be used to endorse or promote
   products derived from this software without specific prior written
   permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

### LibTomMath and LibTomCrypt (`libtommath/`, `libtomcrypt/`)

By Tom St Denis (tomstdenis@gmail.com). The LibTomMath headers state that the library "is free for all
purposes without any express guarantee it works." These libraries are distributed by their author as
public domain; StormLib uses them for hashing and cryptographic primitives (AES, MD5, SHA-1, SHA-256,
big-number arithmetic) in archive handling.

### LZMA SDK 9.11 (`lzma/`)

By Igor Pavlov. Files carry the notice "Public domain".

### lookup3 (`jenkins/`)

`lookup3.c` by Bob Jenkins, May 2006. Public domain.

---

## 2. Qt 6

Bundled in the AppImage (`usr/lib/libQt6*.so.6` and `usr/plugins/`), dynamically linked.
Version: Qt 6.4.2, as packaged by Ubuntu 24.04 (`6.4.2+dfsg-21.1build5`).
Modules in use: Core, Gui, Widgets, DBus, OpenGL, Svg, WaylandClient (+ the EGL client and wl-shell
integration plugins).

Qt is © The Qt Company Ltd. and other contributors. MPQ Viewer uses Qt under the
**GNU Lesser General Public License, version 3 (LGPL-3.0)**. Qt is also available under GPL-2.0/GPL-3.0
and commercial licenses; those are not used here.

- License text: https://www.gnu.org/licenses/lgpl-3.0.html (and, on a Debian/Ubuntu system,
  `/usr/share/common-licenses/LGPL-3`). The LGPL builds on the GPL-3.0, https://www.gnu.org/licenses/gpl-3.0.html.
- Qt source code for the exact bundled version: https://packages.ubuntu.com/source/noble/qt6-base
  and upstream at https://code.qt.io/cgit/qt/qtbase.git/ (tag `v6.4.2`). Qt SVG and Qt Wayland are
  separate Ubuntu source packages (`qt6-svg`, `qt6-wayland`).
- Replacing the Qt libraries: Qt is linked dynamically and the AppImage is not obfuscated. Run
  `./MPQ_Viewer-*-x86_64.AppImage --appimage-extract`, replace the libraries in `squashfs-root/usr/lib/`
  with a compatible build, and start `squashfs-root/AppRun`. MPQ Viewer's full source is in this repository.
- Qt's own third-party components (for example the bundled copies of libraries inside Qt Base) carry
  their own notices; see https://doc.qt.io/qt-6/licenses-used-in-qt.html.

---

## 3. Other libraries bundled in the AppImage

These are unmodified Ubuntu 24.04 (noble) system libraries copied into `usr/lib/` by `linuxdeploy` so
the AppImage runs without them being installed. Licenses are those declared by the respective packages;
the authoritative, full text for each is in `/usr/share/doc/<package>/copyright` on Ubuntu 24.04, and
the corresponding source is available from https://packages.ubuntu.com or `apt source <package>`.

| Library (package) | License |
|---|---|
| GLib (`libglib2.0-0t64`) | LGPL-2.1-or-later |
| systemd library (`libsystemd0`) | LGPL-2.1-or-later |
| libgcrypt (`libgcrypt20`) | LGPL-2.1-or-later |
| Graphite2 (`libgraphite2-3`) | LGPL-2.1-or-later, or MPL-1.1, or GPL-2.0-or-later |
| libgomp (`libgomp1`) | GPL-3.0-or-later with the GCC Runtime Library Exception |
| D-Bus (`libdbus-1-3`) | AFL-2.1 or GPL-2.0-or-later |
| ICU (`libicu74`: libicuuc, libicui18n, libicudata) | ICU / Unicode license |
| libpng (`libpng16-16t64`) | libpng license |
| libjpeg-turbo (`libjpeg-turbo8`) | BSD-3-Clause, IJG license, zlib license |
| bzip2 (`libbz2-1.0`) | bzip2 license (BSD-style; text in section 1) |
| Zstandard (`libzstd1`) | BSD-3-Clause or GPL-2.0 |
| LZ4 (`liblz4-1`) | BSD-2-Clause |
| XZ Utils (`liblzma5`) | public domain / 0BSD |
| PCRE2 (`libpcre2-8-0`, `libpcre2-16-0`) | BSD-3-Clause (with the PCRE2 binary-library exception) |
| Brotli (`libbrotli1`) | MIT |
| libffi (`libffi8`) | MIT |
| libcap (`libcap2`) | BSD-3-Clause or GPL-2.0 |
| libbsd, libmd (`libbsd0`, `libmd0`) | BSD-style licenses (2-/3-Clause, ISC and others; see package copyright file) |
| BLAKE2 (`libb2-1`) | CC0-1.0 (public domain dedication) |
| double-conversion (`libdouble-conversion3`) | BSD-3-Clause |
| MD4C (`libmd4c0`) | MIT |
| xkbcommon (`libxkbcommon0`, `libxkbcommon-x11-0`) | MIT / X11 |
| Wayland client libraries (`libwayland-cursor0`, `libwayland-egl1`) | MIT / X11 |
| libxcb and extensions (`libxcb-glx0`, `-icccm4`, `-image0`, `-keysyms1`, `-randr0`, `-render0`, `-render-util0`, `-shape0`, `-shm0`, `-sync1`, `-util1`, `-xfixes0`, `-xkb1`) | MIT / X11 |
| libXau, libXdmcp (`libxau6`, `libxdmcp6`) | MIT / X11 |

Not bundled: the C library (glibc) and the graphics/display stack (`libX11`, `libEGL`, `libGL*`,
fontconfig, FreeType, HarfBuzz) are taken from the user's system.

---

## 4. AppImage tooling

Used at build time only; none of their code is part of the MPQ Viewer executable.

- `linuxdeploy` and `linuxdeploy-plugin-qt`: https://github.com/linuxdeploy (MIT)
- `appimagetool`: https://github.com/AppImage/appimagetool (MIT)

The finished `.AppImage` file embeds the AppImage runtime written by `appimagetool`; see
https://github.com/AppImage/type2-runtime for that component's license terms.

---

*Generated for MPQ Viewer 0.1.0 from the build inputs of 2026-11-22. This is an engineering summary of the
licenses declared by each project, not legal advice. If you rebuild on a different distribution or
update a dependency, regenerate the table in section 3 and the version numbers above.*
