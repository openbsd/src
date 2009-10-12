#!perl

use strict;
use warnings;

use Test::More tests => 3;

{
  package A;
}

@B::ISA = 'A';
@C::ISA = 'A';
@D::ISA = qw(B C);

eval {mro::set_mro('D', 'c3')};

like $@, qr/Invalid mro name: 'c3'/;

require mro;

is_deeply(mro::get_linear_isa('D'), [qw(D B A C)], 'still dfs MRO');

mro::set_mro('D', 'c3');

is_deeply(mro::get_linear_isa('D'), [qw(D B C A)], 'c3 MRO');
