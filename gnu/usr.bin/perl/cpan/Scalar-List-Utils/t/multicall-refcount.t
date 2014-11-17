use Test::More tests => 1;

use List::Util 'first';

our $comparison;

sub foo {
   if( $comparison ) {
      return 1;
   }
   else {
      local $comparison = 1;
      first \&foo, 1,2,3;
   }
}

for(1,2){
   foo();
}

ok( "Didn't crash calling recursively" );
