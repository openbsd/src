#!/usr/bin/perl -w

# This tests MakeMaker against recursive builds

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
use Config;

use Test::More tests => 26;
use MakeMaker::Test::Utils;
use MakeMaker::Test::Setup::Recurs;

# 'make disttest' sets a bunch of environment variables which interfere
# with our testing.
delete @ENV{qw(PREFIX LIB MAKEFLAGS)};

my $perl = which_perl();
my $Is_VMS = $^O eq 'VMS';

chdir('t');

perl_lib;

my $Touch_Time = calibrate_mtime();

$| = 1;

ok( setup_recurs(), 'setup' );
END { 
    ok( chdir File::Spec->updir );
    ok( teardown_recurs(), 'teardown' );
}

ok( chdir('Recurs'), q{chdir'd to Recurs} ) ||
    diag("chdir failed: $!");


# Check recursive Makefile building.
my @mpl_out = run(qq{$perl Makefile.PL});

cmp_ok( $?, '==', 0, 'Makefile.PL exited with zero' ) ||
  diag(@mpl_out);

my $makefile = makefile_name();

ok( -e $makefile, 'Makefile written' );
ok( -e File::Spec->catfile('prj2',$makefile), 'sub Makefile written' );

my $make = make_run();

my $make_out = run("$make");
is( $?, 0, 'recursive make exited normally' ) || diag $make_out;

ok( chdir File::Spec->updir );
ok( teardown_recurs(), 'cleaning out recurs' );
ok( setup_recurs(),    '  setting up fresh copy' );
ok( chdir('Recurs'), q{chdir'd to Recurs} ) ||
    diag("chdir failed: $!");


# Check NORECURS
@mpl_out = run(qq{$perl Makefile.PL "NORECURS=1"});

cmp_ok( $?, '==', 0, 'Makefile.PL NORECURS=1 exited with zero' ) ||
  diag(@mpl_out);

$makefile = makefile_name();

ok( -e $makefile, 'Makefile written' );
ok( !-e File::Spec->catfile('prj2',$makefile), 'sub Makefile not written' );

$make = make_run();

run("$make");
is( $?, 0, 'recursive make exited normally' );


ok( chdir File::Spec->updir );
ok( teardown_recurs(), 'cleaning out recurs' );
ok( setup_recurs(),    '  setting up fresh copy' );
ok( chdir('Recurs'), q{chdir'd to Recurs} ) ||
    diag("chdir failed: $!");


# Check that arguments aren't stomped when they have .. prepended
# [rt.perl.org 4345]
@mpl_out = run(qq{$perl Makefile.PL "INST_SCRIPT=cgi"});

cmp_ok( $?, '==', 0, 'Makefile.PL exited with zero' ) ||
  diag(@mpl_out);

$makefile = makefile_name();
my $submakefile = File::Spec->catfile('prj2',$makefile);

ok( -e $makefile,    'Makefile written' );
ok( -e $submakefile, 'sub Makefile written' );

my $inst_script = File::Spec->catdir(File::Spec->updir, 'cgi');
ok( open(MAKEFILE, $submakefile) ) || diag("Can't open $submakefile: $!");
{ local $/;  
  like( <MAKEFILE>, qr/^\s*INST_SCRIPT\s*=\s*\Q$inst_script\E/m, 
        'prepend .. not stomping WriteMakefile args' ) 
}
close MAKEFILE;


{
    # Quiet "make test" failure noise
    close *STDERR;

    my $test_out = run("$make test");
    isnt $?, 0, 'test failure in a subdir causes make to fail';
}