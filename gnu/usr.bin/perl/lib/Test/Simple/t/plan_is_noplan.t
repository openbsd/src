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

BEGIN {
    if( !$ENV{HARNESS_ACTIVE} && $ENV{PERL_CORE} ) {
        print "1..0 # Skipped: Won't work with t/TEST\n";
        exit 0;
    }

    # This feature requires a fairly new version of Test::Harness
    require Test::Harness;
    if( $Test::Harness::VERSION < 1.20 ) {
        print "1..0 # Skipped: Need Test::Harness 1.20 or up\n";
        exit(0);
    }
}

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

require Test::Simple::Catch;
my($out, $err) = Test::Simple::Catch::caught();


Test::Simple->import('no_plan');

ok(1, 'foo');


END {
    My::Test::ok($$out eq <<OUT);
ok 1 - foo
1..1
OUT

    My::Test::ok($$err eq <<ERR);
ERR

    # Prevent Test::Simple from exiting with non zero
    exit 0;
}
