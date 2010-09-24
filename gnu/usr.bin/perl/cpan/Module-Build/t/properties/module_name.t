# sample.t -- a sample test file for Module::Build

use strict;
use lib 't/lib';
use MBTest;
use DistGen;

plan tests => 4;

# Ensure any Module::Build modules are loaded from correct directory
blib_load('Module::Build');

my $dist;

#--------------------------------------------------------------------------#
# try getting module_name from dist_name
#--------------------------------------------------------------------------#

$dist = DistGen->new(
  name => "Not::So::Simple",
  distdir => 'Random-Name',
)->chdir_in;

$dist->change_build_pl(
  dist_name => 'Not-So-Simple',
  dist_version => 1,
)->regen;

my $mb = $dist->new_from_context();
isa_ok( $mb, "Module::Build" );
is( $mb->module_name, "Not::So::Simple",
  "module_name guessed from dist_name"
);

#--------------------------------------------------------------------------#
# Try getting module_name from dist_version_from
#--------------------------------------------------------------------------#

$dist->add_file( 'lib/Simple/Name.pm', << 'END_PACKAGE' );
package Simple::Name;
our $VERSION = 1.23;
1;
END_PACKAGE

$dist->change_build_pl(
  dist_name => 'Random-Name',
  dist_version_from => 'lib/Simple/Name.pm',
  dist_abstract => "Don't complain about missing abstract",
)->regen( clean => 1 );

$mb = $dist->new_from_context();
isa_ok( $mb, "Module::Build" );
is( $mb->module_name, "Simple::Name",
  "module_name guessed from dist_version_from"
);

# vim:ts=2:sw=2:et:sta:sts=2
