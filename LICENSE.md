
CyberCache Cluster License
==========================

Copyright 2016-2019 CyberHULL. All rights reserved.

Written by Vadim Sytnikov.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

The complete text of the GNU General Public License can be found in the
`GPL_v2.txt` file. Should you prefer version 3 or a later version, please see
[GNU web site](http://www.gnu.org/licenses/).

On Debian systems, the full text of the GNU General Public License version 2 can
be found in the file `/usr/share/common-licenses/GPL-2`.

Component Licenses
------------------

CyberCache Cluster uses several third-party libraries for the implementation of
hash generators, compressors, and regular expressions. Licenses of all those
components are less strict than GPL v2 (e.g. MIT, BSD, even public domain), and
can be found in the following files:

* Brotli compressor by the Brotli Authors:

  - source code: `lib/compression/brotli/LICENSE`
  - binary distribution: `/usr/share/doc/cybercache-<ce/ee>/license/brotli/LICENSE`

* LZ4 compressor by Yann Collet:

  - source code: `lib/compression/lz4/LICENSE`
  - binary distribution: `/usr/share/doc/cybercache-<ce/ee>/license/lz4/LICENSE`

* LZF compressor by Marc Alexander Lehmann:

  - source code: `lib/compression/lzf/LICENSE`
  - binary distribution: `/usr/share/doc/cybercache-<ce/ee>/license/lzf/LICENSE`

* LZHAM compressor by Richard Geldreich, Jr:

  - source code: `lib/compression/lzham/LICENSE`
  - binary distribution: `/usr/share/doc/cybercache-<ce/ee>/license/lzham/LICENSE`

* Snappy compressor by Google, Inc.:

  - source code: `lib/compression/snappy/COPYING`
  - binary distribution: `/usr/share/doc/cybercache-<ce/ee>/license/snappy/COPYING`

* Zlib compressor by Jean-loup Gailly and Mark Adler:

  - source code: `lib/compression/zlib/LICENSE`
  - binary distribution: `/usr/share/doc/cybercache-<ce/ee>/license/zlib/LICENSE`

* Zstd compressor by Facebook, Inc:

  - source code: `lib/compression/zstd/LICENSE`
  - binary distribution: `/usr/share/doc/cybercache-<ce/ee>/license/zstd/LICENSE`

* Farmhash hash generator by Google, Inc:

  - source code: `lib/hashes/farmhash/COPYING`
  - binary distribution: `/usr/share/doc/cybercache-<ce/ee>/license/farmhash/COPYING`

* Murmurhash2 and Murmurhash3 hash generators by Austin Appleby:

  - source code: `lib/hashes/murmurhash/LICENSE`
  - binary distribution: `/usr/share/doc/cybercache-<ce/ee>/license/murmurhash/LICENSE`

* Spookyhash hash generator by Bob Jenkins:

  - source code: `lib/hashes/spookyhash/LICENSE`
  - binary distribution: `/usr/share/doc/cybercache-<ce/ee>/license/spookyhash/LICENSE`

* xxHash hash generator by Yann Collet:

  - source code: `lib/hashes/xxhash/LICENSE`
  - binary distribution: `/usr/share/doc/cybercache-<ce/ee>/license/xxhash/LICENSE`

* PCRE2 regular expression library by Philip Hazel:

  - source code: `lib/regex/pcre2/LICENCE`
  - binary distribution: `/usr/share/doc/cybercache-<ce/ee>/license/pcre2/LICENCE`
