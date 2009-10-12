#!/usr/bin/perl -w

use strict;
use lib $ENV{PERL_CORE} ? '../lib/Module/Build/t/lib' : 't/lib';
use MBTest tests => 4;

use_ok 'Module::Build';
ensure_blib('Module::Build');

use IO::File;
my $tmp = MBTest->tmpdir;

use DistGen;
my $dist = DistGen->new( dir => $tmp );

my $libdir = 'badlib';
$dist->add_file("$libdir/Build.PL", 'die');
$dist->regen;

$dist->chdir_in;


unshift(@INC, $libdir);
my $mb = eval { Module::Build->new_from_context};
ok(! $@, 'dodged the bullet') or die;
ok($mb);

# cleanup
$dist->remove;

# vim:ts=2:sw=2:et:sta
