#!./perl

use strict;
use Test::More tests => 20;
use List::Util qw(pairgrep pairfirst pairmap pairs pairkeys pairvalues);

no warnings 'misc'; # avoid "Odd number of elements" warnings most of the time

is_deeply( [ pairgrep { $b % 2 } one => 1, two => 2, three => 3 ],
           [ one => 1, three => 3 ],
           'pairgrep list' );

is( scalar( pairgrep { $b & 2 } one => 1, two => 2, three => 3 ),
    2,
    'pairgrep scalar' );

is_deeply( [ pairgrep { $a } 0 => "zero", 1 => "one", 2 ],
           [ 1 => "one", 2 => undef ],
           'pairgrep pads with undef' );

{
  use warnings 'misc';
  my $warnings = "";
  local $SIG{__WARN__} = sub { $warnings .= $_[0] };

  pairgrep { } one => 1, two => 2;
  is( $warnings, "", 'even-sized list yields no warnings from pairgrep' );

  pairgrep { } one => 1, two =>;
  like( $warnings, qr/^Odd number of elements in pairgrep at /,
        'odd-sized list yields warning from pairgrep' );
}

{
  my @kvlist = ( one => 1, two => 2 );
  pairgrep { $b++ } @kvlist;
  is_deeply( \@kvlist, [ one => 2, two => 3 ], 'pairgrep aliases elements' );
}

is_deeply( [ pairfirst { length $a == 5 } one => 1, two => 2, three => 3 ],
           [ three => 3 ],
           'pairfirst list' );

is_deeply( [ pairfirst { length $a == 4 } one => 1, two => 2, three => 3 ],
           [],
           'pairfirst list empty' );

is( scalar( pairfirst { length $a == 5 } one => 1, two => 2, three => 3 ),
    1,
    'pairfirst scalar true' );

ok( !scalar( pairfirst { length $a == 4 } one => 1, two => 2, three => 3 ),
    'pairfirst scalar false' );

is_deeply( [ pairmap { uc $a => $b } one => 1, two => 2, three => 3 ],
           [ ONE => 1, TWO => 2, THREE => 3 ],
           'pairmap list' );

is( scalar( pairmap { qw( a b c ) } one => 1, two => 2 ),
    6,
    'pairmap scalar' );

is_deeply( [ pairmap { $a => @$b } one => [1,1,1], two => [2,2,2], three => [3,3,3] ],
           [ one => 1, 1, 1, two => 2, 2, 2, three => 3, 3, 3 ],
           'pairmap list returning >2 items' );

is_deeply( [ pairmap { $b } one => 1, two => 2, three => ],
           [ 1, 2, undef ],
           'pairmap pads with undef' );

{
  my @kvlist = ( one => 1, two => 2 );
  pairmap { $b++ } @kvlist;
  is_deeply( \@kvlist, [ one => 2, two => 3 ], 'pairmap aliases elements' );
}

# Calculating a 1000-element list should hopefully cause the stack to move
# underneath pairmap
is_deeply( [ pairmap { my @l = (1) x 1000; "$a=$b" } one => 1, two => 2, three => 3 ],
           [ "one=1", "two=2", "three=3" ],
           'pairmap copes with stack movement' );

is_deeply( [ pairs one => 1, two => 2, three => 3 ],
           [ [ one => 1 ], [ two => 2 ], [ three => 3 ] ],
           'pairs' );

is_deeply( [ pairs one => 1, two => ],
           [ [ one => 1 ], [ two => undef ] ],
           'pairs pads with undef' );

is_deeply( [ pairkeys one => 1, two => 2 ],
           [qw( one two )],
           'pairkeys' );

is_deeply( [ pairvalues one => 1, two => 2 ],
           [ 1, 2 ],
           'pairvalues' );
