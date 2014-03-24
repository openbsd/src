#!/usr/bin/perl -w

BEGIN {
    unshift @INC, 't/lib/';
}
chdir 't';

use strict;

use Test::More;
use MakeMaker::Test::Utils;
use MakeMaker::Test::Setup::XS;
use File::Find;
use File::Spec;
use File::Path;

my $Skipped = 0;
if( have_compiler() ) {
    plan tests => 5;
}
else {
    $Skipped = 1;
    plan skip_all => "ExtUtils::CBuilder not installed or couldn't find a compiler";
}

my $Is_VMS = $^O eq 'VMS';
my $perl = which_perl();

chdir 't';

perl_lib;

$| = 1;

ok( setup_xs(), 'setup' );
END {
    unless( $Skipped ) {
        chdir File::Spec->updir or die;
        teardown_xs(), 'teardown' or die;
    }
}

ok( chdir('XS-Test'), "chdir'd to XS-Test" ) ||
  diag("chdir failed: $!");

my @mpl_out = run(qq{$perl Makefile.PL});

cmp_ok( $?, '==', 0, 'Makefile.PL exited with zero' ) ||
  diag(@mpl_out);

my $make = make_run();
my $make_out = run("$make");
is( $?, 0,                                 '  make exited normally' ) || 
    diag $make_out;

my $test_out = run("$make test");
is( $?, 0,                                 '  make test exited normally' ) || 
    diag $test_out;
