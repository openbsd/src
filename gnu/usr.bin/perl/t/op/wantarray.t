#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

use strict;

plan 13;

sub context {
  local $::Level = $::Level + 1;
  my ( $cona, $testnum ) = @_;
  my $conb = (defined wantarray) ? ( wantarray ? 'A' : 'S' ) : 'V';
  is $cona, $conb;
}

context('V');
my $a = context('S');
my @a = context('A');
scalar context('S');
$a = scalar context('S');
($a) = context('A');
($a) = scalar context('S');

{
  # [ID 20020626.011] incorrect wantarray optimisation
  sub simple { wantarray ? 1 : 2 }
  sub inline {
    my $a = wantarray ? simple() : simple();
    $a;
  }
  my @b = inline();
  my $c = inline();
  is @b, 1;
  is "@b", "2";
  is $c, 2;
}

my $q;

my $qcontext = q{
  $q = (defined wantarray) ? ( wantarray ? 'A' : 'S' ) : 'V';
};
eval $qcontext;
is $q, 'V';
$a = eval $qcontext;
is $q, 'S';
@a = eval $qcontext;
is $q, 'A';

1;
