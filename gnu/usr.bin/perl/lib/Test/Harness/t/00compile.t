#!/usr/bin/perl -w

BEGIN {
    if($ENV{PERL_CORE}) {
        chdir 't';
        @INC = '../lib';
    }
    else {
        unshift @INC, 't/lib';
    }
}

use Test::More tests => 6;

BEGIN { use_ok 'Test::Harness' }
BEGIN { diag( "Testing Test::Harness $Test::Harness::VERSION under Perl $] and Test::More $Test::More::VERSION" ) unless $ENV{PERL_CORE}}

BEGIN { use_ok 'Test::Harness::Straps' }

BEGIN { use_ok 'Test::Harness::Iterator' }

BEGIN { use_ok 'Test::Harness::Assert' }

BEGIN { use_ok 'Test::Harness::Point' }

# If the $VERSION is set improperly, this will spew big warnings.
BEGIN { use_ok 'Test::Harness', 1.1601 }

