
PCRE2 Test
==========

Test application: makes sure that `regex_pcre2` library had been built and is 
working according to specs. Expects two arguments:

1. regular expression pattern conforming to the PCRE specs,

2. string that is to be searched for a match or matches.

If there is a match, prints it out; if pattern contained groups (in round 
braces), outputs parts of the match that correspond to the groups.
