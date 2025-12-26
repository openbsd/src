use strict;
use warnings;

BEGIN {
    chdir 't' if -d 't';
}

# This is a wrapper for a generated file.  Assumes being run from 't'
# directory.

if (! $ENV{PERL_DEBUG_FULL_TEST}) {
    print "1..0 # SKIP Lengthy Tests Disabled; to enable set environment"
        . " variable PERL_DEBUG_FULL_TEST to a true value\n";
    exit;
}

my $file = '../lib/unicore/TestNorm.pl';
if (! -e $file) {
    print "1..0 # SKIP $file not built (perhaps build options don't"
        . " build it)\n";
    exit;
}

do $file;
