# qr// was introduced in 5.004-devel.  Skip this test if we're not
# of high enough version.
BEGIN { 
    if( $] < 5.005 ) {
        print "1..0 # Skipped Test requires qr//\n";
        exit(0);
    }
}

BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = ('../lib', 'lib');
    }
    else {
        unshift @INC, 't/lib';
    }
}

# There was a bug with like() involving a qr// not failing properly.
# This tests against that.

use strict;


# Can't use Test.pm, that's a 5.005 thing.
package My::Test;

# This has to be a require or else the END block below runs before
# Test::Builder's own and the ending diagnostics don't come out right.
require Test::Builder;
my $TB = Test::Builder->create;
$TB->plan(tests => 2);


require Test::Simple::Catch;
my($out, $err) = Test::Simple::Catch::caught();
local $ENV{HARNESS_ACTIVE} = 0;


package main;

require Test::More;
Test::More->import(tests => 1);

eval q{ like( "foo", qr/that/, 'is foo like that' ); };


END {
    $TB->is_eq($$out, <<OUT, 'failing output');
1..1
not ok 1 - is foo like that
OUT

    my $err_re = <<ERR;
#   Failed test 'is foo like that'
#   in .* at line 1\.
#                   'foo'
#     doesn't match '\\(\\?-xism:that\\)'
# Looks like you failed 1 test of 1\\.
ERR


    $TB->like($$err, qr/^$err_re$/, 'failing errors');

    exit(0);
}
