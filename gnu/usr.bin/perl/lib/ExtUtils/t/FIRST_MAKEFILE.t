#!/usr/bin/perl -w

BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't' if -d 't';
        @INC = ('../lib', 'lib');
    }
    else {
        unshift @INC, 't/lib';
    }
}
chdir 't';

use strict;
use Test::More tests => 7;

use MakeMaker::Test::Setup::BFD;
use MakeMaker::Test::Utils;

my $perl = which_perl();
my $make = make_run();
perl_lib();


ok( setup_recurs(), 'setup' );
END {
    ok( chdir File::Spec->updir );
    ok( teardown_recurs(), 'teardown' );
}

ok( chdir('Big-Dummy'), "chdir'd to Big-Dummy" ) ||
  diag("chdir failed: $!");

my @mpl_out = run(qq{$perl Makefile.PL FIRST_MAKEFILE=jakefile});
cmp_ok( $?, '==', 0, 'Makefile.PL exited with zero' ) || diag @mpl_out;

ok( -e 'jakefile', 'FIRST_MAKEFILE honored' );

ok( grep(/^Writing jakefile for Big::Dummy/, @mpl_out) == 1,
					'Makefile.PL output looks right' );
