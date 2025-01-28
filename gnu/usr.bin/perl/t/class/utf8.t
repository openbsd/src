#!./perl

BEGIN {
    chdir 't' if -d 't';
    require './test.pl';
    set_up_inc('../lib');
    require Config;
}

use v5.36;
use utf8;
use feature 'class';
no warnings 'experimental::class';

# A bunch of test cases with non-ASCII, non-Latin1. Esperanto is good for that
# as the accented characters are not in Latin1.

STDOUT->binmode( ":encoding(UTF-8)" );

my $manĝis;

class Sandviĉon {
   method manĝu { $manĝis++ }

   field $tranĉaĵoj :param :reader = undef;
}

# class name
{
   my $s = Sandviĉon->new;
   isa_ok( $s, "Sandviĉon", '$s' );
}

# methods
{
   my $s = Sandviĉon->new;
   $s->manĝu;
   ok( $manĝis, 'UTF-8 method name works' );
}

# field params + accessors default names
{
   my $s = Sandviĉon->new( tranĉaĵoj => 3 );
   is( $s->tranĉaĵoj, 3, 'Can obtain value from field via accessor' );
}

class Sandwich {
   field $slices :param(tranĉaĵoj) :reader(tranĉaĵoj) = undef;
}

{
   my $s = Sandwich->new( tranĉaĵoj => 5 );
   is( $s->tranĉaĵoj, 5, 'Can obtain value from field via accessor with overridden name' );
}

done_testing;
