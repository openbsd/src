# sample.t -- a sample test file for Module::Build

use strict;
use lib 't/lib';
use MBTest;
use DistGen;

plan tests => 2;

# Ensure any Module::Build modules are loaded from correct directory
blib_load('Module::Build');

#--------------------------------------------------------------------------#
# Create test distribution
#--------------------------------------------------------------------------#

use DistGen;
my $dist = DistGen->new( name => 'Simple::Name' );

$dist->change_build_pl(
  module_name => 'Simple::Name',
  dist_suffix => 'SUFFIX',
)->regen;

$dist->chdir_in;

my $mb = $dist->new_from_context();
isa_ok( $mb, "Module::Build" );
is( $mb->dist_dir, "Simple-Name-0.01-SUFFIX",
  "dist_suffix set correctly"
);

# vim:ts=2:sw=2:et:sta:sts=2
