#!perl
use strict;
BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't' if -d 't';
        chdir '../lib/parent';
        @INC = '..';
    }
}

use strict;
use Test::More tests => 4;
use lib 't/lib';

our @ISA;

use Data::Dumper;
is_deeply \@ISA, [], '@ISA is empty at start';

# These contain another test each
eval q{ use parent qw(localize_A localize_B) };

is_deeply \@ISA, [qw[localize_A localize_B]], '@ISA contains localize_A localize_B after use line';

done_testing();
