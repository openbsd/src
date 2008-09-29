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

plan tests => 8;

my $blib = File::Spec->catfile( File::Spec->curdir, "blib" );
my $blib_lib = File::Spec->catfile( $blib, "lib" );
my $blib_arch = File::Spec->catfile( $blib, "arch" );
my $prove = File::Spec->catfile( $blib, "script", "prove" );
$prove = "$^X $prove";

CAPITAL_TAINT: {
    local $ENV{PROVE_SWITCHES};

    my @actual = qx/$prove -Ifirst -D -I second -Ithird -Tvdb/;
    my @expected = ( "# \$Test::Harness::Switches: -T -I$blib_arch -I$blib_lib -Ifirst -Isecond -Ithird\n" );
    is_deeply( \@actual, \@expected, "Capital taint flags OK" );
}

LOWERCASE_TAINT: {
    local $ENV{PROVE_SWITCHES};

    my @actual = qx/$prove -dD -Ifirst -I second -t -Ithird -vb/;
    my @expected = ( "# \$Test::Harness::Switches: -t -I$blib_arch -I$blib_lib -Ifirst -Isecond -Ithird\n" );
    is_deeply( \@actual, \@expected, "Lowercase taint OK" );
}

PROVE_SWITCHES: {
    local $ENV{PROVE_SWITCHES} = "-dvb -I fark";

    my @actual = qx/$prove -Ibork -Dd/;
    my @expected = ( "# \$Test::Harness::Switches: -I$blib_arch -I$blib_lib -Ifark -Ibork\n" );
    is_deeply( \@actual, \@expected, "PROVE_SWITCHES OK" );
}

PROVE_SWITCHES_L: {
    my @actual = qx/$prove -l -Ibongo -Dd/;
    my @expected = ( "# \$Test::Harness::Switches: -Ilib -Ibongo\n" );
    is_deeply( \@actual, \@expected, "PROVE_SWITCHES OK" );
}

PROVE_SWITCHES_LB: {
    my @actual = qx/$prove -lb -Dd/;
    my @expected = ( "# \$Test::Harness::Switches: -Ilib -I$blib_arch -I$blib_lib\n" );
    is_deeply( \@actual, \@expected, "PROVE_SWITCHES OK" );
}

PROVE_VERSION: {
    # This also checks that the prove $VERSION is in sync with Test::Harness's $VERSION
    local $/ = undef;

    use_ok( 'Test::Harness' );

    my $thv = $Test::Harness::VERSION;
    my @actual = qx/$prove --version/;
    is( scalar @actual, 1, 'Only 1 line returned' );
    like( $actual[0], qq{/^\Qprove v$thv, using Test::Harness v$thv and Perl v5\E/} );
}
