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

use strict;
use Config;

use Test::More tests => 8;
use MakeMaker::Test::Utils;

# 'make disttest' sets a bunch of environment variables which interfere
# with our testing.
delete @ENV{qw(PREFIX LIB MAKEFLAGS)};

my $Perl = which_perl();
my $Makefile = makefile_name();
my $Is_VMS = $^O eq 'VMS';

chdir($Is_VMS ? 'BFD_TEST_ROOT:[t]' : 't');
perl_lib;

$| = 1;

ok( chdir('Big-Dummy'), "chdir'd to Big-Dummy" ) ||
  diag("chdir failed: $!");

unlink $Makefile;
my $prereq_out = run(qq{$Perl Makefile.PL "PREREQ_PRINT=1"});
ok( !-r $Makefile, "PREREQ_PRINT produces no $Makefile" );
is( $?, 0,         '  exited normally' );
{
    package _Prereq::Print;
    no strict;
    $PREREQ_PM = undef;  # shut up "used only once" warning.
    eval $prereq_out;
    ::is_deeply( $PREREQ_PM, { strict => 0 }, 'prereqs dumped' );
    ::is( $@, '',                             '  without error' );
}


$prereq_out = run(qq{$Perl Makefile.PL "PRINT_PREREQ=1"});
ok( !-r $Makefile, "PRINT_PREREQ produces no $Makefile" );
is( $?, 0,         '  exited normally' );
::like( $prereq_out, qr/^perl\(strict\) \s* >= \s* 0 \s*$/x, 
                                                      'prereqs dumped' );


# Currently a bug.
#my $prereq_out = run(qq{$Perl Makefile.PL "PREREQ_PRINT=0"});
#ok( -r $Makefile, "PREREQ_PRINT=0 produces a $Makefile" );
#is( $?, 0,         '  exited normally' );
#unlink $Makefile;

# Currently a bug.
#my $prereq_out = run(qq{$Perl Makefile.PL "PRINT_PREREQ=1"});
#ok( -r $Makefile, "PRINT_PREREQ=0 produces a $Makefile" );
#is( $?, 0,         '  exited normally' );
#unlink $Makefile;
