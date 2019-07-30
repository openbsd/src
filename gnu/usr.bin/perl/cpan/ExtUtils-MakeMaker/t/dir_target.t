#!/usr/bin/perl -w

use lib 't/lib';

use File::Temp qw[tempdir];
my $tmpdir = tempdir( DIR => 't', CLEANUP => 1 );
chdir $tmpdir;

use Test::More tests => 1;
use ExtUtils::MakeMaker;

# dir_target() was typo'd as dir_targets()
can_ok('MM', 'dir_target');
