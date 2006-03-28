#!/usr/bin/perl -w

# This is a test of the verification of the arguments to
# WriteMakefile.

BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't' if -d 't';
        @INC = ('../lib', 'lib');
    }
    else {
        unshift @INC, 't/lib';
    }
}

use strict;
use Test::More tests => 16;

use TieOut;
use MakeMaker::Test::Utils;
use MakeMaker::Test::Setup::BFD;

use ExtUtils::MakeMaker;

chdir 't';

perl_lib();

ok( setup_recurs(), 'setup' );
END {
    ok( chdir File::Spec->updir );
    ok( teardown_recurs(), 'teardown' );
}

ok( chdir 'Big-Dummy', "chdir'd to Big-Dummy" ) ||
  diag("chdir failed: $!");

{
    ok( my $stdout = tie *STDOUT, 'TieOut' );
    my $warnings = '';
    local $SIG{__WARN__} = sub {
        $warnings .= join '', @_;
    };

    my $mm;

    eval {
        $mm = WriteMakefile(
            NAME            => 'Big::Dummy',
            VERSION_FROM    => 'lib/Big/Dummy.pm',
            MAN3PODS        => ' ', # common mistake
        );
    };

    is( $warnings, <<VERIFY );
WARNING: MAN3PODS takes a hash reference not a string/number.
         Please inform the author.
VERIFY

    $warnings = '';
    eval {
        $mm = WriteMakefile(
            NAME            => 'Big::Dummy',
            VERSION_FROM    => 'lib/Big/Dummy.pm',
            AUTHOR          => sub {},
        );
    };

    is( $warnings, <<VERIFY );
WARNING: AUTHOR takes a string/number not a code reference.
         Please inform the author.
VERIFY

    # LIBS accepts *both* a string or an array ref.  The first cut of
    # our verification did not take this into account.
    $warnings = '';
    $mm = WriteMakefile(
        NAME            => 'Big::Dummy',
        VERSION_FROM    => 'lib/Big/Dummy.pm',
        LIBS            => '-lwibble -lwobble',
    );

    # We'll get warnings about the bogus libs, that's ok.
    unlike( $warnings, qr/WARNING: .* takes/ );
    is_deeply( $mm->{LIBS}, ['-lwibble -lwobble'] );

    $warnings = '';
    $mm = WriteMakefile(
        NAME            => 'Big::Dummy',
        VERSION_FROM    => 'lib/Big/Dummy.pm',
        LIBS            => ['-lwibble', '-lwobble'],
    );

    # We'll get warnings about the bogus libs, that's ok.
    unlike( $warnings, qr/WARNING: .* takes/ );
    is_deeply( $mm->{LIBS}, ['-lwibble', '-lwobble'] );

    $warnings = '';
    eval {
        $mm = WriteMakefile(
            NAME            => 'Big::Dummy',
            VERSION_FROM    => 'lib/Big/Dummy.pm',
            LIBS            => { wibble => "wobble" },
        );
    };

    # We'll get warnings about the bogus libs, that's ok.
    like( $warnings, qr{^WARNING: LIBS takes a array reference or string/number not a hash reference}m );


    $warnings = '';
    $mm = WriteMakefile(
        NAME            => 'Big::Dummy',
        WIBBLE          => 'something',
        wump            => { foo => 42 },
    );

    like( $warnings, qr{^WARNING: WIBBLE is not a known parameter.\n}m );
    like( $warnings, qr{^WARNING: wump is not a known parameter.\n}m );

    is( $mm->{WIBBLE}, 'something' );
    is_deeply( $mm->{wump}, { foo => 42 } );
}
