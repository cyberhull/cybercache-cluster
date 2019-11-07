
Standard Regex Test
===================

Test application: uses standard C++11 regular expressions to search input text
for matches against provided pattern. Used to compare size of the produced
executable against that of the PCRE2-based one (`pcre2.exe`: 122,880 bytes,
`regex.exe`: 91,648 bytes, both built with full optimizations; the latter does
*not* output meaningful error descriptions, however, printing "regex_error"
instead; the results are for Cygwin build). Expects two arguments:

1. regular expression pattern conforming to the PCRE specs,

2. string that is to be searched for a match or matches.

If there is a match, prints it out; if pattern contained groups (in round 
braces), outputs parts of the match that correspond to the groups.

The regular exception constructor has an option of ignoring group matches and 
not storing them in the `cmatch` instance, which should be used in production 
code if "knowing" substrings is not needed (that option, `std::regex::nosubs`, 
is commented out in the test code).
