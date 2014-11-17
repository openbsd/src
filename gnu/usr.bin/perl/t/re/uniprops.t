use strict;
use warnings;
no warnings 'once';

# This is a wrapper for a generated file.  Assumes being run from 't'
# directory.

# It is skipped by default under PERL_DEBUG_READONLY_COW, but you can run
# it directly via:  cd t; ./perl ../lib/unicore/TestProp.pl

require Config;
if ($Config::Config{ccflags} =~ /(?:^|\s)-DPERL_DEBUG_READONLY_COW\b/) {
    print "1..0 # Skip PERL_DEBUG_READONLY_COW\n";
    exit;
}

do '../lib/unicore/TestProp.pl';

0
