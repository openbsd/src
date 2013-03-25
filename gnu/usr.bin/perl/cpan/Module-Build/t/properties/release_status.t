use strict;
use lib 't/lib';
use MBTest;
use DistGen;

if ( $] lt 5.008001 ) { 
  plan skip_all => "dotted-version numbers are buggy before 5.8.1";
} else {
  plan 'no_plan';
}

# Ensure any Module::Build modules are loaded from correct directory
blib_load('Module::Build');

#--------------------------------------------------------------------------#
# Create test distribution
#--------------------------------------------------------------------------#

{
  my $dist = DistGen->new( name => 'Simple::Name', version => '0.01' );

  $dist->change_build_pl(
    module_name => 'Simple::Name',
  )->regen;

  $dist->chdir_in;

  my $mb = $dist->new_from_context();
  isa_ok( $mb, "Module::Build" );
  is( $mb->release_status, "stable",
    "regular version has release_status 'stable'"
  );
}

{
  my $dist = DistGen->new( name => 'Simple::Name', version => 'v1.2.3' );

  $dist->change_build_pl(
    module_name => 'Simple::Name',
  )->regen;

  $dist->chdir_in;

  my $mb = $dist->new_from_context();
  isa_ok( $mb, "Module::Build" );
  is( $mb->release_status, "stable",
    "dotted-decimal version has release_status 'stable'"
  );
}

{
  my $dist = DistGen->new( name => 'Simple::Name', version => q{'0.01_01'} );

  $dist->change_build_pl(
    module_name => 'Simple::Name',
  )->regen;

  $dist->chdir_in;

  my $mb = $dist->new_from_context();
  isa_ok( $mb, "Module::Build" );
  is( $mb->release_status, "testing",
    "alpha version has release_status 'testing'"
  );
}

{
  my $dist = DistGen->new( name => 'Simple::Name', version => 'v1.2.3_1' );

  $dist->change_build_pl(
    module_name => 'Simple::Name',
  )->regen;

  $dist->chdir_in;

  my $mb = $dist->new_from_context();
  isa_ok( $mb, "Module::Build" );
  is( $mb->release_status, "testing",
    "dotted alpha version has release_status 'testing'"
  );
}

{
  my $dist = DistGen->new( name => 'Simple::Name', version => q{'0.01_01'} );

  $dist->change_build_pl(
    module_name => 'Simple::Name',
    release_status => 'unstable',
  )->regen;

  $dist->chdir_in;

  my $mb = $dist->new_from_context();
  isa_ok( $mb, "Module::Build" );
  is( $mb->release_status, "unstable",
    "explicit 'unstable' keeps release_status 'unstable'"
  );
}

{
  my $dist = DistGen->new( name => 'Simple::Name', version => '0.01' );

  $dist->change_build_pl(
    module_name => 'Simple::Name',
    release_status => 'testing',
  )->regen;

  $dist->chdir_in;

  my $mb = $dist->new_from_context();
  isa_ok( $mb, "Module::Build" );
  is( $mb->dist_suffix, "TRIAL",
    "regular version marked 'testing' gets 'TRIAL' suffix"
  );
}

{
  my $dist = DistGen->new( name => 'Simple::Name', version => 'v1.2.3' );

  $dist->change_build_pl(
    module_name => 'Simple::Name',
    release_status => 'testing',
  )->regen;

  $dist->chdir_in;

  my $mb = $dist->new_from_context();
  isa_ok( $mb, "Module::Build" );
  is( $mb->dist_suffix, "TRIAL",
    "dotted version marked 'testing' gets 'TRIAL' suffix"
  );
}

{
  my $dist = DistGen->new( name => 'Simple::Name', version => '0.01' );

  $dist->change_build_pl(
    module_name => 'Simple::Name',
    release_status => 'unstable',
  )->regen;

  $dist->chdir_in;

  my $mb = $dist->new_from_context();
  isa_ok( $mb, "Module::Build" );
  is( $mb->dist_suffix, "TRIAL",
    "regular version marked 'unstable' gets 'TRIAL' suffix"
  );
}

{
  my $dist = DistGen->new( name => 'Simple::Name', version => '0.01' );

  $dist->change_build_pl(
    module_name => 'Simple::Name',
    release_status => 'beta',
  )->regen;

  $dist->chdir_in;

  my $output = stdout_stderr_of sub { $dist->run_build_pl() };
  like( $output, qr/Illegal value 'beta' for release_status/i,
    "Got error message for illegal release_status"
  );
}

{
  my $dist = DistGen->new( name => 'Simple::Name', version => q{'0.01_01'} );

  $dist->change_build_pl(
    module_name => 'Simple::Name',
    release_status => 'stable',
  )->regen;

  $dist->chdir_in;

  my $output = stdout_stderr_of sub { $dist->run_build_pl() };
  like( $output, qr/Illegal value 'stable' with version '0.01_01'/i,
    "Got error message for illegal 'stable' with alpha version"
  );
}

{
  my $dist = DistGen->new( name => 'Simple::Name', version => q{'0.01_01'} );

  $dist->change_build_pl(
    module_name => 'Simple::Name',
    dist_version => '1.23beta1',
  )->regen;

  $dist->chdir_in;

  my $mb = $dist->new_from_context();
  isa_ok( $mb, "Module::Build" );
  is( $mb->dist_suffix, "",
    "non-standard dist_version does not get a suffix"
  );
  is( $mb->release_status, "stable",
    "non-standard dist_version defaults to stable release_status"
  );
}

# Test with alpha number
# vim:ts=2:sw=2:et:sta:sts=2
