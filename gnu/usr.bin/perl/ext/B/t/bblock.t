#!./perl -Tw

BEGIN {
    chdir 't';
    @INC = '../lib';
    require Config;
    if (($Config::Config{'extensions'} !~ /\bB\b/) ){
        print "1..0 # Skip -- Perl configured without B module\n";
        exit 0;
    }
}

use Test::More tests => 1;

use_ok('B::Bblock', qw(find_leaders));

# Someone who understands what this module does, please fill this out.
