use strict;
use lib 't/lib';
use MBTest;
plan tests => 3; # or 'no_plan'
use DistGen;

# Ensure any Module::Build modules are loaded from correct directory
blib_load('Module::Build');

# create dist object in a temp directory
# enter the directory and generate the skeleton files
my $dist = DistGen->new->chdir_in;
$dist->add_file('mylib/MBUtil.pm', << "---");
package MBUtil;
sub foo { 42 }
1;
---

$dist->add_file('Build.PL', << "---");
use strict;
use lib 'mylib';
use MBUtil;
use Module::Build;

die unless MBUtil::foo() == 42;

my \$builder = Module::Build->new(
module_name         => '$dist->{name}',
license             => 'perl',
);

\$builder->create_build_script();
---

$dist->regen;

# get a Module::Build object and test with it
my $mb = $dist->new_from_context(); # quiet by default
isa_ok( $mb, "Module::Build" );
is( $mb->dist_name, "Simple", "dist_name is 'Simple'" );
ok( ( grep { /mylib/ } @INC ), "resume added \@INC addition to \@INC");

# vim:ts=2:sw=2:et:sta:sts=2
