# sample.t -- a sample test file for Module::Build

use strict;
use lib 't/lib';
use MBTest tests => 2; # or 'no_plan'
use DistGen;

# Ensure any Module::Build modules are loaded from correct directory
blib_load('Module::Build');

# create dist object in a temp directory
# enter the directory and generate the skeleton files
my $dist = DistGen->new->chdir_in->regen;

# get a Module::Build object and test with it
my $mb = $dist->new_from_context(); # quiet by default
isa_ok( $mb, "Module::Build" );
is( $mb->dist_name, "Simple", "dist_name is 'Simple'" );

# vim:ts=2:sw=2:et:sta:sts=2
