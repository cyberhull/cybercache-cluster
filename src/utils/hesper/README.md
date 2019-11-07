
Hesper Compressor
=================

This is a test application for the `hesper` compression algorithm (designed to
compress [very] short ASCII strings, such as Magento tag names), which will be
ultimately included into CyberCache cluster's compression library. See comments
before the `Hesper` class in `main.cc`.

he `hesper_*` scripts are used to test Hesper compression algorithm developer
by VyberCache author ; its support has not been included into CyberCache components yet.


Usage:

    hesper {encode|decode} <source-file> <destination-file>

Depending on result, `hesper` will exit with one of the following codes:

* `0` : compression was successful,

* `1` : invalid command line arguments,

* `2` : compression has failed: either `hesper` encountered unsupported
  character (non-ASCII, or ASCII control code other than `\n`), or it could not
  produce result smaller than the original file,

* `3` : some error occurred (`hesper` could not open/read source file, or
  create/write destination file, or uncompress source file because it was not
  compressed using `hesper` or got corrupt, or an internal error occurred; this
  code is also returned if source file size is zero or greater than 4G).

If compression was successful, `<destination-file>` will contain source file
size in first four bytes (hence `<source-file>` cannot be bigger than 4G),
followed by compressed stream of bytes.
