#!/usr/bin/perl -w

# This test puts MakeMaker through the paces of a basic perl module
# build, test and installation of the Big::Fat::Dummy module.

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
use Test::More tests => 17;
use MakeMaker::Test::Utils;
use File::Spec;
use TieOut;

my $perl = which_perl();

my $root_dir = 't';

if( $^O eq 'VMS' ) {
    # On older systems we might exceed the 8-level directory depth limit
    # imposed by RMS.  We get around this with a rooted logical, but we
    # can't create logical names with attributes in Perl, so we do it
    # in a DCL subprocess and put it in the job table so the parent sees it.
    open( BFDTMP, '>bfdtesttmp.com' ) || die "Error creating command file; $!";
    print BFDTMP <<'COMMAND';
$ IF F$TRNLNM("PERL_CORE") .EQS. "" .AND. F$TYPE(PERL_CORE) .EQS. ""
$ THEN
$!  building CPAN version
$   BFD_TEST_ROOT = F$PARSE("SYS$DISK:[]",,,,"NO_CONCEAL")-".][000000"-"]["-"].;"+".]"
$ ELSE
$!  we're in the core
$   BFD_TEST_ROOT = F$PARSE("SYS$DISK:[-]",,,,"NO_CONCEAL")-".][000000"-"]["-"].;"+".]"
$ ENDIF
$ DEFINE/JOB/NOLOG/TRANSLATION=CONCEALED BFD_TEST_ROOT 'BFD_TEST_ROOT'
COMMAND
    close BFDTMP;

    system '@bfdtesttmp.com';
    END { 1 while unlink 'bfdtesttmp.com' }
    $root_dir = 'BFD_TEST_ROOT:[t]';
}

chdir $root_dir;


perl_lib;

my $Touch_Time = calibrate_mtime();

$| = 1;

ok( chdir 'Big-Dummy', "chdir'd to Big-Dummy" ) ||
  diag("chdir failed: $!");

my @mpl_out = `$perl Makefile.PL PREFIX=dummy-install`;

cmp_ok( $?, '==', 0, 'Makefile.PL exited with zero' ) ||
  diag(@mpl_out);

my $makefile = makefile_name();
ok( grep(/^Writing $makefile for Big::Dummy/, 
         @mpl_out) == 1,
                                           'Makefile.PL output looks right');

ok( grep(/^Current package is: main$/,
         @mpl_out) == 1,
                                           'Makefile.PL run in package main');

ok( -e $makefile,       'Makefile exists' );

# -M is flakey on VMS
my $mtime = (stat($makefile))[9];
cmp_ok( $Touch_Time, '<=', $mtime,  '  its been touched' );

END { unlink makefile_name(), makefile_backup() }

my $make = make_run();

{
    # Supress 'make manifest' noise
    local $ENV{PERL_MM_MANIFEST_VERBOSE} = 0;
    my $manifest_out = `$make manifest`;
    ok( -e 'MANIFEST',      'make manifest created a MANIFEST' );
    ok( -s 'MANIFEST',      '  its not empty' );
}

END { unlink 'MANIFEST'; }

my $test_out = `$make test`;
like( $test_out, qr/All tests successful/, 'make test' );
is( $?, 0 );

# Test 'make test TEST_VERBOSE=1'
my $make_test_verbose = make_macro($make, 'test', TEST_VERBOSE => 1);
$test_out = `$make_test_verbose`;
like( $test_out, qr/ok \d+ - TEST_VERBOSE/, 'TEST_VERBOSE' );
like( $test_out, qr/All tests successful/, '  successful' );
is( $?, 0 );

my $dist_test_out = `$make disttest`;
is( $?, 0, 'disttest' ) || diag($dist_test_out);


# Make sure init_dirscan doesn't go into the distdir
@mpl_out = `$perl Makefile.PL "PREFIX=dummy-install"`;

cmp_ok( $?, '==', 0, 'Makefile.PL exited with zero' ) ||
  diag(@mpl_out);

ok( grep(/^Writing $makefile for Big::Dummy/, 
         @mpl_out) == 1,
                                'init_dirscan skipped distdir') || 
  diag(@mpl_out);

# I know we'll get ignored errors from make here, that's ok.
# Send STDERR off to oblivion.
open(SAVERR, ">&STDERR") or die $!;
open(STDERR, ">".File::Spec->devnull) or die $!;

my $realclean_out = `$make realclean`;
is( $?, 0, 'realclean' ) || diag($realclean_out);

open(STDERR, ">&SAVERR") or die $!;
close SAVERR;
