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
use Config;
use ExtUtils::MakeMaker;

use Test::More tests => 79;
use MakeMaker::Test::Utils;
use MakeMaker::Test::Setup::BFD;
use File::Find;
use File::Spec;
use File::Path;

my $perl = which_perl();
my $Is_VMS = $^O eq 'VMS';

chdir 't';

perl_lib;

my $Touch_Time = calibrate_mtime();

$| = 1;

ok( setup_recurs(), 'setup' );
END {
    ok( chdir File::Spec->updir );
    ok( teardown_recurs(), 'teardown' );
}

ok( chdir('Big-Dummy'), "chdir'd to Big-Dummy" ) ||
  diag("chdir failed: $!");

my @mpl_out = run(qq{$perl Makefile.PL "PREFIX=../dummy-install"});
END { rmtree '../dummy-install'; }

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
    my $manifest_out = run("$make manifest");
    ok( -e 'MANIFEST',      'make manifest created a MANIFEST' );
    ok( -s 'MANIFEST',      '  its not empty' );
}

END { unlink 'MANIFEST'; }


my $ppd_out = run("$make ppd");
is( $?, 0,                      '  exited normally' ) || diag $ppd_out;
ok( open(PPD, 'Big-Dummy.ppd'), '  .ppd file generated' );
my $ppd_html;
{ local $/; $ppd_html = <PPD> }
close PPD;
like( $ppd_html, qr{^<SOFTPKG NAME="Big-Dummy" VERSION="0.01">}m, 
                                                           '  <SOFTPKG>' );
like( $ppd_html, qr{^\s*<ABSTRACT>Try "our" hot dog's</ABSTRACT>}m,         
                                                           '  <ABSTRACT>');
like( $ppd_html, 
      qr{^\s*<AUTHOR>Michael G Schwern &lt;schwern\@pobox.com&gt;</AUTHOR>}m,
                                                           '  <AUTHOR>'  );
like( $ppd_html, qr{^\s*<IMPLEMENTATION>}m,          '  <IMPLEMENTATION>');
like( $ppd_html, qr{^\s*<REQUIRE NAME="strict::" />}m,  '  <REQUIRE>' );

my $archname = $Config{archname};
if( $] >= 5.008 ) {
    # XXX This is a copy of the internal logic, so it's not a great test
    $archname .= "-$Config{PERL_REVISION}.$Config{PERL_VERSION}";
}
like( $ppd_html, qr{^\s*<ARCHITECTURE NAME="$archname" />}m,
                                                           '  <ARCHITECTURE>');
like( $ppd_html, qr{^\s*<CODEBASE HREF="" />}m,            '  <CODEBASE>');
like( $ppd_html, qr{^\s*</IMPLEMENTATION>}m,           '  </IMPLEMENTATION>');
like( $ppd_html, qr{^\s*</SOFTPKG>}m,                      '  </SOFTPKG>');
END { unlink 'Big-Dummy.ppd' }


my $test_out = run("$make test");
like( $test_out, qr/All tests successful/, 'make test' );
is( $?, 0,                                 '  exited normally' ) || 
    diag $test_out;

# Test 'make test TEST_VERBOSE=1'
my $make_test_verbose = make_macro($make, 'test', TEST_VERBOSE => 1);
$test_out = run("$make_test_verbose");
like( $test_out, qr/ok \d+ - TEST_VERBOSE/, 'TEST_VERBOSE' );
like( $test_out, qr/All tests successful/,  '  successful' );
is( $?, 0,                                  '  exited normally' ) ||
    diag $test_out;


my $install_out = run("$make install");
is( $?, 0, 'install' ) || diag $install_out;
like( $install_out, qr/^Installing /m );

ok( -r '../dummy-install',     '  install dir created' );
my %files = ();
find( sub { 
    # do it case-insensitive for non-case preserving OSs
    my $file = lc $_;

    # VMS likes to put dots on the end of things that don't have them.
    $file =~ s/\.$// if $Is_VMS;

    $files{$file} = $File::Find::name; 
}, '../dummy-install' );
ok( $files{'dummy.pm'},     '  Dummy.pm installed' );
ok( $files{'liar.pm'},      '  Liar.pm installed'  );
ok( $files{'program'},      '  program installed'  );
ok( $files{'.packlist'},    '  packlist created'   );
ok( $files{'perllocal.pod'},'  perllocal.pod created' );


SKIP: {
    skip 'VMS install targets do not preserve $(PREFIX)', 8 if $Is_VMS;

    $install_out = run("$make install PREFIX=elsewhere");
    is( $?, 0, 'install with PREFIX override' ) || diag $install_out;
    like( $install_out, qr/^Installing /m );

    ok( -r 'elsewhere',     '  install dir created' );
    %files = ();
    find( sub { $files{$_} = $File::Find::name; }, 'elsewhere' );
    ok( $files{'Dummy.pm'},     '  Dummy.pm installed' );
    ok( $files{'Liar.pm'},      '  Liar.pm installed'  );
    ok( $files{'program'},      '  program installed'  );
    ok( $files{'.packlist'},    '  packlist created'   );
    ok( $files{'perllocal.pod'},'  perllocal.pod created' );
    rmtree('elsewhere');
}


SKIP: {
    skip 'VMS install targets do not preserve $(DESTDIR)', 10 if $Is_VMS;

    $install_out = run("$make install PREFIX= DESTDIR=other");
    is( $?, 0, 'install with DESTDIR' ) || 
        diag $install_out;
    like( $install_out, qr/^Installing /m );

    ok( -d 'other',  '  destdir created' );
    %files = ();
    my $perllocal;
    find( sub { 
        $files{$_} = $File::Find::name;
    }, 'other' );
    ok( $files{'Dummy.pm'},     '  Dummy.pm installed' );
    ok( $files{'Liar.pm'},      '  Liar.pm installed'  );
    ok( $files{'program'},      '  program installed'  );
    ok( $files{'.packlist'},    '  packlist created'   );
    ok( $files{'perllocal.pod'},'  perllocal.pod created' );

    ok( open(PERLLOCAL, $files{'perllocal.pod'} ) ) || 
        diag("Can't open $files{'perllocal.pod'}: $!");
    { local $/;
      unlike(<PERLLOCAL>, qr/other/, 'DESTDIR should not appear in perllocal');
    }
    close PERLLOCAL;

# TODO not available in the min version of Test::Harness we require
#    ok( open(PACKLIST, $files{'.packlist'} ) ) || 
#        diag("Can't open $files{'.packlist'}: $!");
#    { local $/;
#      local $TODO = 'DESTDIR still in .packlist';
#      unlike(<PACKLIST>, qr/other/, 'DESTDIR should not appear in .packlist');
#    }
#    close PACKLIST;

    rmtree('other');
}


SKIP: {
    skip 'VMS install targets do not preserve $(PREFIX)', 9 if $Is_VMS;

    $install_out = run("$make install PREFIX=elsewhere DESTDIR=other/");
    is( $?, 0, 'install with PREFIX override and DESTDIR' ) || 
        diag $install_out;
    like( $install_out, qr/^Installing /m );

    ok( !-d 'elsewhere',       '  install dir not created' );
    ok( -d 'other/elsewhere',  '  destdir created' );
    %files = ();
    find( sub { $files{$_} = $File::Find::name; }, 'other/elsewhere' );
    ok( $files{'Dummy.pm'},     '  Dummy.pm installed' );
    ok( $files{'Liar.pm'},      '  Liar.pm installed'  );
    ok( $files{'program'},      '  program installed'  );
    ok( $files{'.packlist'},    '  packlist created'   );
    ok( $files{'perllocal.pod'},'  perllocal.pod created' );
    rmtree('other');
}


my $dist_test_out = run("$make disttest");
is( $?, 0, 'disttest' ) || diag($dist_test_out);

# Test META.yml generation
use ExtUtils::Manifest qw(maniread);

my $distdir  = 'Big-Dummy-0.01';
$distdir =~ s/\./_/g if $Is_VMS;
my $meta_yml = "$distdir/META.yml";

ok( !-f 'META.yml',  'META.yml not written to source dir' );
ok( -f $meta_yml,    'META.yml written to dist dir' );
ok( !-e "META_new.yml", 'temp META.yml file not left around' );

SKIP: {
    # META.yml spec 1.4 was added in 0.11
    skip "Test::YAML::Meta >= 0.11 required", 2
      unless eval { require Test::YAML::Meta }   and
             Test::YAML::Meta->VERSION >= 0.11;

    Test::YAML::Meta::meta_spec_ok($meta_yml);
}

ok open META, $meta_yml or diag $!;
my $meta = join '', <META>;
ok close META;

is $meta, <<"END";
--- #YAML:1.0
name:               Big-Dummy
version:            0.01
abstract:           Try "our" hot dog's
author:
    - Michael G Schwern <schwern\@pobox.com>
license:            unknown
distribution_type:  module
configure_requires:
    ExtUtils::MakeMaker:  0
build_requires:
    ExtUtils::MakeMaker:  0
requires:
    strict:  0
no_index:
    directory:
        - t
        - inc
generated_by:       ExtUtils::MakeMaker version $ExtUtils::MakeMaker::VERSION
meta-spec:
    url:      http://module-build.sourceforge.net/META-spec-v1.4.html
    version:  1.4
END

my $manifest = maniread("$distdir/MANIFEST");
# VMS is non-case preserving, so we can't know what the MANIFEST will
# look like. :(
_normalize($manifest);
is( $manifest->{'meta.yml'}, 'Module meta-data (added by MakeMaker)' );


# Test NO_META META.yml suppression
unlink $meta_yml;
ok( !-f $meta_yml,   'META.yml deleted' );
@mpl_out = run(qq{$perl Makefile.PL "NO_META=1"});
cmp_ok( $?, '==', 0, 'Makefile.PL exited with zero' ) || diag(@mpl_out);
my $distdir_out = run("$make distdir");
is( $?, 0, 'distdir' ) || diag($distdir_out);
ok( !-f $meta_yml,   'META.yml generation suppressed by NO_META' );


# Make sure init_dirscan doesn't go into the distdir
@mpl_out = run(qq{$perl Makefile.PL "PREFIX=../dummy-install"});

cmp_ok( $?, '==', 0, 'Makefile.PL exited with zero' ) || diag(@mpl_out);

ok( grep(/^Writing $makefile for Big::Dummy/, @mpl_out) == 1,
                                'init_dirscan skipped distdir') || 
  diag(@mpl_out);

# I know we'll get ignored errors from make here, that's ok.
# Send STDERR off to oblivion.
open(SAVERR, ">&STDERR") or die $!;
open(STDERR, ">",File::Spec->devnull) or die $!;

my $realclean_out = run("$make realclean");
is( $?, 0, 'realclean' ) || diag($realclean_out);

open(STDERR, ">&SAVERR") or die $!;
close SAVERR;


sub _normalize {
    my $hash = shift;

    while(my($k,$v) = each %$hash) {
        delete $hash->{$k};
        $hash->{lc $k} = $v;
    }
}
