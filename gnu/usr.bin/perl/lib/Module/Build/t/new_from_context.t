#!/usr/bin/perl -w

use strict;
use lib $ENV{PERL_CORE} ? '../lib/Module/Build/t/lib' : 't/lib';
use MBTest tests => 2;

use Cwd ();
my $cwd = Cwd::cwd;
my $tmp = MBTest->tmpdir;

use DistGen;
my $dist = DistGen->new( dir => $tmp );

my $libdir = 'badlib';
$dist->add_file("$libdir/Build.PL", 'die');
$dist->regen;

chdir( $dist->dirname ) or die "Can't chdir to '@{[$dist->dirname]}': $!";

use IO::File;
use Module::Build;

unshift(@INC, $libdir);
my $mb = eval { Module::Build->new_from_context};
ok(! $@, 'dodged the bullet') or die;
ok($mb);

# cleanup
chdir( $cwd ) or die "Can''t chdir to '$cwd': $!";
$dist->remove;

use File::Path;
rmtree( $tmp );

# vim:ts=2:sw=2:et:sta
