#!/usr/bin/perl -w

BEGIN {
    if ($^O eq 'VMS') {
        print '1..0 # Child test output confuses parent test counter';
        exit;
    }
}

BEGIN {
    if ( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = ('../lib', 'lib');
    }
    else {
        unshift @INC, 't/lib';
    }
}

use strict;

use Test::More tests => 6;
use File::Spec;

BEGIN {
    use_ok( 'Test::Harness' );
}

my $died;
sub prepare_for_death { $died = 0; }
sub signal_death { $died = 1; }

my $Curdir = File::Spec->curdir;
my $SAMPLE_TESTS = $ENV{PERL_CORE}
	? File::Spec->catdir($Curdir, 'lib', 'sample-tests')
	: File::Spec->catdir($Curdir, 't',   'sample-tests');

PASSING: {
    local $SIG{__DIE__} = \&signal_death;
    prepare_for_death();
    eval { runtests( File::Spec->catfile( $SAMPLE_TESTS, "simple" ) ) };
    ok( !$@, "simple lives" );
    is( $died, 0, "Death never happened" );
}

FAILING: {
    local $SIG{__DIE__} = \&signal_death;
    prepare_for_death();
    eval { runtests( File::Spec->catfile( $SAMPLE_TESTS, "too_many" ) ) };
    ok( $@, "$@" );
    ok( $@ =~ m[Failed 1/1], "too_many dies" );
    is( $died, 1, "Death happened" );
}
