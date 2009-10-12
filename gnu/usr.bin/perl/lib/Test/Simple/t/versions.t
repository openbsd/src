#!/usr/bin/perl -w

# Make sure all the modules have the same version
#
# TBT has its own version system.

use strict;
use Test::More;

require Test::Builder;
require Test::Builder::Module;
require Test::Simple;

my $dist_version = $Test::More::VERSION;

like( $dist_version, qr/^ \d+ \. \d+ $/x );
is( $dist_version, $Test::Builder::VERSION,             'Test::Builder' );
is( $dist_version, $Test::Builder::Module::VERSION,     'TB::Module' );
is( $dist_version, $Test::Simple::VERSION,              'Test::Simple' );

done_testing(4);
