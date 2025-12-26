#!./perl

use strict;
use warnings;

use Test::More tests => 8;
use List::Util qw(zip zip_longest zip_shortest);

is_deeply( [zip ()], [],
  'zip empty returns empty');

is_deeply( [zip ['a'..'c']], [ ['a'], ['b'], ['c'] ],
  'zip of one list returns a list of singleton lists' );

is_deeply( [zip ['one', 'two'], [1, 2]], [ [one => 1], [two => 2] ],
  'zip of two lists returns a list of pair lists' );

# Unequal length arrays

is_deeply( [zip_longest ['x', 'y', 'z'], ['X', 'Y']], [ ['x', 'X'], ['y', 'Y'], ['z', undef] ],
  'zip_longest extends short lists with undef' );

is_deeply( [zip_shortest ['x', 'y', 'z'], ['X', 'Y']], [ ['x', 'X'], ['y', 'Y'] ],
  'zip_shortest stops after shortest list' );

# Non arrayref arguments throw exception
ok( !defined eval { zip 1, 2, 3 },
  'non-reference argument throws exception' );

ok( !defined eval { zip +{ one => 1 } },
  'reference to non array throws exception' );

# RT156183
{
  my @inp = ( [1,2,3], [4,5,6] );
  foreach my $pair ( zip @inp ) {
    $pair->[0]++;
    $pair->[1]++;
  }
  is_deeply( \@inp, [ [1,2,3], [4,5,6] ],
    'original values unchanged by modification of zip() output' );
}
