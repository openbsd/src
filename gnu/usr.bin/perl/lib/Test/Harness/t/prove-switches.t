BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = ('../lib', 'lib');
    }
    else {
        unshift @INC, 't/lib';
    }
}

use strict;
use File::Spec;
use Test::More;
plan skip_all => "Not adapted to perl core" if $ENV{PERL_CORE};
plan skip_all => "Not installing prove" if -e "t/SKIP-PROVE";

plan tests => 5;

my $blib = File::Spec->catfile( File::Spec->curdir, "blib" );
my $blib_lib = File::Spec->catfile( $blib, "lib" );
my $blib_arch = File::Spec->catfile( $blib, "arch" );
my $prove = File::Spec->catfile( $blib, "script", "prove" );

CAPITAL_TAINT: {
    local $ENV{PROVE_SWITCHES};
    local $/ = undef;

    my @actual = qx/$prove -Ifirst -D -I second -Ithird -Tvdb/;
    my @expected = ( "# \$Test::Harness::Switches: -T -I$blib_arch -I$blib_lib -Ifirst -Isecond -Ithird\n" );
    array_match_ok( \@actual, \@expected, "Capital taint flags OK" );
}

LOWERCASE_TAINT: {
    local $ENV{PROVE_SWITCHES};
    local $/ = undef;

    my @actual = qx/$prove -dD -Ifirst -I second -t -Ithird -vb/;
    my @expected = ( "# \$Test::Harness::Switches: -t -I$blib_arch -I$blib_lib -Ifirst -Isecond -Ithird\n" );
    array_match_ok( \@actual, \@expected, "Lowercase taint OK" );
}

PROVE_SWITCHES: {
    local $ENV{PROVE_SWITCHES} = "-dvb -I fark";
    local $/ = undef;

    my @actual = qx/$prove -Ibork -Dd/;
    my @expected = ( "# \$Test::Harness::Switches: -I$blib_arch -I$blib_lib -Ifark -Ibork\n" );
    array_match_ok( \@actual, \@expected, "PROVE_SWITCHES OK" );
}

PROVE_SWITCHES_L: {
    local $/ = undef;

    my @actual = qx/$prove -l -Ibongo -Dd/;
    my @expected = ( "# \$Test::Harness::Switches: -Ilib -Ibongo\n" );
    array_match_ok( \@actual, \@expected, "PROVE_SWITCHES OK" );
}

PROVE_SWITCHES_LB: {
    local $/ = undef;

    my @actual = qx/$prove -lb -Dd/;
    my @expected = ( "# \$Test::Harness::Switches: -Ilib -I$blib_arch -I$blib_lib\n" );
    array_match_ok( \@actual, \@expected, "PROVE_SWITCHES OK" );
}


sub array_match_ok {
    my $actual = shift;
    my $expected = shift;
    my $message = shift;
    my $n = 0;

    my @actual = @$actual;
    my @expected = @$expected;

    while ( @actual && @expected ) {
	return ok( 0, "Differs at element $n: $message" ) if shift @actual ne shift @expected;
	++$n;
    }
    return ok( 0, "Too many actual: $message" ) if @actual;
    return ok( 0, "Too many expected: $message" ) if @expected;

    return ok( 1, $message );
}
