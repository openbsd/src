#!/usr/bin/perl -w

# Test that MakeMaker honors user's PM override.

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
use Test::More tests => 6;

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

ok( my $stdout = tie *STDOUT, 'TieOut' );

{
    my $mm = WriteMakefile(
        NAME            => 'Big::Dummy',
        VERSION_FROM    => 'lib/Big/Dummy.pm',
        PM              => { 'wibble' => 'woof' }
    );

    is_deeply( $mm->{PM},  { wibble => 'woof' } );
}
