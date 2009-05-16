#!perl -w
# $Id: eq_set.t,v 1.2 2009/05/16 21:42:57 simon Exp $

BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = ('../lib', 'lib');
    }
    else {
        unshift @INC, 't/lib';
    }
}
chdir 't';

use strict;
use Test::More;

plan tests => 4;

# RT 3747
ok( eq_set([1, 2, [3]], [[3], 1, 2]) );
ok( eq_set([1,2,[3]], [1,[3],2]) );

# bugs.perl.org 36354
my $ref = \2;
ok( eq_set( [$ref, "$ref", "$ref", $ref],
            ["$ref", $ref, $ref, "$ref"] 
          ) );

TODO: {
    local $TODO = q[eq_set() doesn't really handle references];

    ok( eq_set( [\1, \2, \3], [\2, \3, \1] ) );
}

