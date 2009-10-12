#!/usr/bin/perl -w

# Tests INSTALL_BASE

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
use File::Path;
use Config;

use Test::More tests => 20;
use MakeMaker::Test::Utils;
use MakeMaker::Test::Setup::BFD;

my $Is_VMS = $^O eq 'VMS';

my $perl = which_perl();

chdir 't';
perl_lib;

ok( setup_recurs(), 'setup' );
END {
    ok( chdir File::Spec->updir );
    ok( teardown_recurs(), 'teardown' );
}

ok( chdir('Big-Dummy'), "chdir'd to Big-Dummy") || diag("chdir failed; $!");

my @mpl_out = run(qq{$perl Makefile.PL "INSTALL_BASE=../dummy-install"});
END { rmtree '../dummy-install'; }

cmp_ok( $?, '==', 0, 'Makefile.PL exited with zero' ) ||
  diag(@mpl_out);

my $makefile = makefile_name();
ok( grep(/^Writing $makefile for Big::Dummy/, 
         @mpl_out) == 1,
                                           'Makefile.PL output looks right');

my $make = make_run();
run("$make");   # this is necessary due to a dmake bug.
my $install_out = run("$make install");
is( $?, 0, '  make install exited normally' ) || diag $install_out;
like( $install_out, qr/^Installing /m );

ok( -r '../dummy-install',      '  install dir created' );

my @installed_files = 
  ('../dummy-install/lib/perl5/Big/Dummy.pm',
   '../dummy-install/lib/perl5/Big/Liar.pm',
   '../dummy-install/bin/program',
   "../dummy-install/lib/perl5/$Config{archname}/perllocal.pod",
   "../dummy-install/lib/perl5/$Config{archname}/auto/Big/Dummy/.packlist"
  );

foreach my $file (@installed_files) {
    ok( -e $file, "  $file installed" );
    ok( -r $file, "  $file readable" );
}


# nmake outputs its damned logo
# Send STDERR off to oblivion.
open(SAVERR, ">&STDERR") or die $!;
open(STDERR, ">".File::Spec->devnull) or die $!;

my $realclean_out = run("$make realclean");
is( $?, 0, 'realclean' ) || diag($realclean_out);

open(STDERR, ">&SAVERR") or die $!;
close SAVERR;
