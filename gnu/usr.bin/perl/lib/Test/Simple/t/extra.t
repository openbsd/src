#!perl -w

BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = '../lib';
    }
}

# Can't use Test.pm, that's a 5.005 thing.
package My::Test;

print "1..2\n";

my $test_num = 1;
# Utility testing functions.
sub ok ($;$) {
    my($test, $name) = @_;
    my $ok = '';
    $ok .= "not " unless $test;
    $ok .= "ok $test_num";
    $ok .= " - $name" if defined $name;
    $ok .= "\n";
    print $ok;
    $test_num++;
}


package main;

require Test::Simple;

chdir 't';
push @INC, '../t/lib/';
require Test::Simple::Catch;
my($out, $err) = Test::Simple::Catch::caught();

Test::Simple->import(tests => 3);

#line 30
ok(1, 'Foo');
ok(0, 'Bar');
ok(1, 'Yar');
ok(1, 'Car');
ok(0, 'Sar');

END {
    My::Test::ok($$out eq <<OUT);
1..3
ok 1 - Foo
not ok 2 - Bar
ok 3 - Yar
ok 4 - Car
not ok 5 - Sar
OUT

    My::Test::ok($$err eq <<ERR);
#     Failed test ($0 at line 31)
#     Failed test ($0 at line 34)
# Looks like you planned 3 tests but ran 2 extra.
ERR

    exit 0;
}
