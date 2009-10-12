#!perl -w

BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = '../lib';
    }
}

# Can't use Test.pm, that's a 5.005 thing.
package My::Test;

# This has to be a require or else the END block below runs before
# Test::Builder's own and the ending diagnostics don't come out right.
require Test::Builder;
my $TB = Test::Builder->create;
$TB->plan(tests => 2);


package main;

require Test::Simple;

chdir 't';
push @INC, '../t/lib/';
require Test::Simple::Catch;
my($out, $err) = Test::Simple::Catch::caught();
local $ENV{HARNESS_ACTIVE} = 0;

Test::Simple->import(tests => 3);

#line 30
ok(1, 'Foo');
ok(0, 'Bar');
ok(1, 'Yar');
ok(1, 'Car');
ok(0, 'Sar');

END {
    $TB->is_eq($$out, <<OUT);
1..3
ok 1 - Foo
not ok 2 - Bar
ok 3 - Yar
ok 4 - Car
not ok 5 - Sar
OUT

    $TB->is_eq($$err, <<ERR);
#   Failed test 'Bar'
#   at $0 line 31.
#   Failed test 'Sar'
#   at $0 line 34.
# Looks like you planned 3 tests but ran 5.
# Looks like you failed 2 tests of 5 run.
ERR

    exit 0;
}
