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

print "1..12\n";

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

eval {
    Test::Simple->import;
};

My::Test::ok($$out eq '');
My::Test::ok($$err eq '');
My::Test::ok($@    eq '');

eval {
    Test::Simple->import(tests => undef);
};

My::Test::ok($$out eq '');
My::Test::ok($$err eq '');
My::Test::ok($@ =~ /Got an undefined number of tests/);

eval {
    Test::Simple->import(tests => 0);
};

My::Test::ok($$out eq '');
My::Test::ok($$err eq '');
My::Test::ok($@ =~ /You said to run 0 tests!/);

eval {
    Test::Simple::ok(1);
};
My::Test::ok( $@ =~ /You tried to run a test without a plan!/);


END {
    My::Test::ok($$out eq '');
    My::Test::ok($$err eq "");

    # Prevent Test::Simple from exiting with non zero.
    exit 0;
}
