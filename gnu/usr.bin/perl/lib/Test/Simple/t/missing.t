BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = ('../lib', 'lib');
    }
    else {
        unshift @INC, 't/lib';
    }
}

# Can't use Test.pm, that's a 5.005 thing.
package My::Test;

# This has to be a require or else the END block below runs before
# Test::Builder's own and the ending diagnostics don't come out right.
require Test::Builder;
my $TB = Test::Builder->create;
$TB->plan(tests => 2);

sub is { $TB->is_eq(@_) }


package main;

require Test::Simple;

require Test::Simple::Catch;
my($out, $err) = Test::Simple::Catch::caught();
local $ENV{HARNESS_ACTIVE} = 0;

Test::Simple->import(tests => 5);

#line 30
ok(1, 'Foo');
ok(0, 'Bar');

END {
    My::Test::is($$out, <<OUT);
1..5
ok 1 - Foo
not ok 2 - Bar
OUT

    My::Test::is($$err, <<ERR);
#   Failed test 'Bar'
#   in $0 at line 31.
# Looks like you planned 5 tests but only ran 2.
# Looks like you failed 1 test of 2 run.
ERR

    exit 0;
}
