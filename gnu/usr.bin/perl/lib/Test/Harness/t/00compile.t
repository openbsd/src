#!/usr/bin/perl -Tw

BEGIN {
    if($ENV{PERL_CORE}) {
        chdir 't';
        @INC = '../lib';
    }
    else {
        unshift @INC, 't/lib';
    }
}

use Test::More tests => 5;

BEGIN { use_ok 'Test::Harness' }

BEGIN { use_ok 'Test::Harness::Straps' }

BEGIN { use_ok 'Test::Harness::Iterator' }

BEGIN { use_ok 'Test::Harness::Assert' }

# If the $VERSION is set improperly, this will spew big warnings.
use_ok 'Test::Harness', 1.1601;
