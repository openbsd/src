#!./perl -Tw

BEGIN {
    chdir 't';
    @INC = '../lib';
}

use Test::More tests => 1;

use_ok('B::Bblock', qw(find_leaders));

# Someone who understands what this module does, please fill this out.
