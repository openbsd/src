#!/usr/bin/perl -w

BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = ('../lib', 'lib/');
    }
    else {
        unshift @INC, 't/lib/';
    }
}
chdir 't';

use Test::More;
use MakeMaker::Test::Utils;
use MakeMaker::Test::Setup::XS;
use File::Find;
use File::Spec;
use File::Path;

if( have_compiler() ) {
    plan tests => 7;
}
else {
    plan skip_all => "ExtUtils::CBuilder not installed or couldn't find a compiler";
}

my $Is_VMS = $^O eq 'VMS';
my $perl = which_perl();

# GNV logical interferes with testing
$ENV{'bin'} = '[.bin]' if $Is_VMS;

chdir 't';

perl_lib;

$| = 1;

ok( setup_xs(), 'setup' );
END {
    ok( chdir File::Spec->updir );
    ok( teardown_xs(), 'teardown' );
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

my $test_out = run("$make");
is( $?, 0,                                 '  make test exited normally' ) || 
    diag $test_out;
