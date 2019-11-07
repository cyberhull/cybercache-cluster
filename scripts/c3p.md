
CyberCache Text Preprocessor
============================

`c3p` is build/configuration utility that can make edition-specific changes and
other transformations to various texts such as documentation and configuration
files. Usage:

    c3p [ <option> [ <option> [ ... ]]

where valid options are:

    -e {ce|community|ee|enterprise}  specifies target edition,
    -f <filename>                    input file name (stdin if omitted),
    -o <filename>                    output file name (stdout if omitted),
    -t                               output to temp file and print its name,
    -c <filename>                    load specified configuration file,
    -v <name>=<value>                set configuration variable to a value,
    -h, or --help                    display help message and exit.

The `-t` option will create temporary file, write transformed text to it, and
print to standard output stream the name of that file (it would then be
caller's responsibility to delete the file when it's no longer needed). This
option overrides `-o` (explicit output filename will be ignored).

The `c3p` utility exits to the shell with one of the following statuses:

    0 if processing was completed successfully,
    1 if help message was printed upon user's request,
    3 if an error occurred.

`c3p` can do several types of changes to the input text:

Edition-specific Changes
------------------------

When processing source file, `c3p` looks for special constructs within that
file that provide alternative texts for Community and Enterprise editions of
the file, and retain (in the destination file that it generates) only those
that correspond to the edition specified on the command line. The constructs
are very simple:

### Inline Usage:

    C3P[<community-edition-text>|<enterptise-edition-text>]

If 'community' edition was specified, first portion of the text will be
retained; if 'enterprise' was specified, then the second portion.

### Block/paragraph Usage

If it is necessary to include/exclude one or more paragraphs based on
specified edition, multiline version of the selector can be used:

    C3P[
    <community-edition-text>
    |
    <enterptise-edition-text>
    ]

It is important that 'C3P[', '|' (or custom delimiter), and ']' are all
located at the **very beginning** of their lines, and that they are followed
by the newline characters; the terminating ']' can also be the very last
character in the input file/stream.

### Custom Separator Characters

If default delimiter ('|') is used in replacement text, it is possible to
specify a different delimiter between `C3P` and opening square bracket; for
instance:

    C3P![comunity edition uses '|' for foo!enterprise edition uses '|' for bar]

or, in case of multiline selector:

    C3P%[
    Comunity edition uses '|' for baz
    %
    Enterprise edition uses '|' for FUBAR
    ]

Custom separator can be any **punctuation** character except ']'.

Meta-variable Replacements
--------------------------

Second type of replacements performed by `c3p` utility is that of various meta-
variables found in the text. The following replacements are made when using
default configuration file:

* `C3P[VERSION]` - current CyberCache version number, such as `1.2.0`,
* `C3P[ED]` - short name of specified edition (`CE` or `EE`),
* `C3P[EDITION]` - full name of the edition (`Community` or `Enterprise`),
* `C3P[DATE]` - current date; for instance `Aug 11, 2019`,
* `C3P[TIMESTAMP]` - current timestamp (e.g. `2019-08-11 20:12:15`).

Having version number in configuration file allows other tools to have one,
"centralized" source of it; for instance, a shell script can easily get it
using the following code snippet (provided that `c3p` is in `PATH`):

    $(echo 'C3P[VERSION]' | c3p)

Performed replacements are not constrained by the above list; in fact, it is
possible to define arbitrary replacements using configuration file and/or a
combination of `-v` options.

### Configuration File

Configuration file is a series of `<name> = <value>` assignments, where
`value` **must** be a PHP expression that evaluates to a string. Each variable
definition must be on a single line, line continuations are not supported.
Empty lines are ignored. Lines with first non-whitespace character being hash
mark (`#`) are treated as comments and are also ignored. 

Default configuration file contains the following definitions:

    VERSION = '1.0.0'
    ED ='CE'
    EDITION = 'Community'
    DATE = date("M j, Y")
    TIMESTAMP = date("Y.m.d H:i:s")

The `ED` and `EDITION` variables are set automatically if option `-e` is
specified on the command line. `DATE` and `TIMESTAMP` can be set to a specific
date string -- that is, without a `date()` call (if it's necessary to, say,
mark all release texts with a single date).

> *NOTE*: setting `ED`, or `EDITION`, or both does not affect replacements
> done using `C3E[<ce-text>|<ee-text>]` macros, it only sets edition names
> that will be inserted instead of `C3E[ED]` and `C3E[EDITION]` macros,
> respectively; only option `-e` affects which parts of the substitution text
> will be selected.

The `-v` option can be used to not only overwrite a variable (e.g. to override
version using something like `-v VERSION='1.5.3'`), but also to introduce
*new* variables and their replacement values.
