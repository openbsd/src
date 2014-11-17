# sample.t -- a sample test file for Module::Build

use strict;
use lib 't/lib';
use MBTest; # or 'no_plan'
use DistGen;
use Config;
use File::Spec;
use ExtUtils::Packlist;
use ExtUtils::Installed;
use File::Path;

# Ensure any Module::Build modules are loaded from correct directory
blib_load('Module::Build');
blib_load('Module::Build::ConfigData');

if ( $ENV{PERL_CORE} ) {
  plan skip_all => 'bundle_inc tests will never succeed in PERL_CORE';
}
elsif ( ! $ENV{MB_TEST_EXPERIMENTAL} ) {
  plan skip_all => '$ENV{MB_TEST_EXPERIMENTAL} is not set';
}
elsif ( ! MBTest::check_EUI() ) {
  plan skip_all => 'ExtUtils::Installed takes too long on your system';
}
elsif ( Module::Build::ConfigData->feature('inc_bundling_support') ) {
  plan tests => 19;
} else {
  plan skip_all => 'inc_bundling_support feature is not enabled';
}

# need to do a temp install of M::B being tested to ensure a packlist
# is available for bundling

my $current_mb = Module::Build->resume();
my $temp_install = MBTest->tmpdir();
my $arch = $Config{archname};
my $lib_path = File::Spec->catdir($temp_install,qw/lib perl5/);
my $arch_path = File::Spec->catdir( $lib_path, $arch );
mkpath ( $arch_path );
ok( -d $arch_path, "created temporary M::B pseudo-install directory");

unshift @INC, $lib_path, $arch_path;
local $ENV{PERL5LIB} = join( $Config{path_sep},
  $lib_path, ($ENV{PERL5LIB} ? $ENV{PERL5LIB} : () )
);

# must uninst=0 so we don't try to remove an installed M::B!
stdout_of( sub { $current_mb->dispatch(
      'install', install_base => $temp_install, uninst => 0
    )
  }
);

# create dist object in a temp directory
# enter the directory and generate the skeleton files
my $dist = DistGen->new( inc => 1 )->chdir_in->regen;

# get a Module::Build object and test with it
my $mb = $dist->new_from_context(); # quiet by default
isa_ok( $mb, "Module::Build" );
is( $mb->dist_name, "Simple", "dist_name is 'Simple'" );
is_deeply( $mb->bundle_inc, [ 'Module::Build' ],
  "Module::Build is flagged for bundling"
);

# bundle stuff into distdir
stdout_stderr_of( sub { $mb->dispatch('distdir') } );

my $dist_inc = File::Spec->catdir($mb->dist_dir, 'inc');
ok( -e File::Spec->catfile( $dist_inc, 'latest.pm' ),
  "dist_dir/inc/latest.pm created"
);

ok( -d File::Spec->catdir( $dist_inc, 'inc_Module-Build' ),
  "dist_dir/inc/inc_Module_Build created"
);

my $mb_file =
  File::Spec->catfile( $dist_inc, qw/inc_Module-Build Module Build.pm/ );

ok( -e $mb_file,
  "dist_dir/inc/inc_Module_Build/Module/Build.pm created"
);

ok( -e File::Spec->catfile( $dist_inc, qw/inc_Module-Build Module Build Base.pm/ ),
  "dist_dir/inc/inc_Module_Build/Module/Build/Base.pm created"
);

# Force bundled M::B to a higher version so it gets loaded
# This has failed on Win32 for no known reason, so we'll skip if
# we can't edit the file.

eval {
  chmod 0666, $mb_file;
  open(my $fh, '<', $mb_file) or die "Could not read $mb_file: $!";
  my $mb_code = do { local $/; <$fh> };
  $mb_code =~ s{\$VERSION\s+=\s+\S+}{\$VERSION = 9999;};
  close $fh;
  open($fh, '>', $mb_file) or die "Could not write $mb_file: $!";
  print {$fh} $mb_code;
  close $fh;
};

my $err = $@;
diag $@ if $@;
SKIP: {
  skip "Couldn't adjust \$VERSION in bundled M::B for testing", 10
    if $err;

  # test the bundling in dist_dir
  chdir $mb->dist_dir;

  stdout_of( sub { Module::Build->run_perl_script('Build.PL',[],[]) } );
  ok( -e 'MYMETA.yml', 'MYMETA was created' );

  open(my $meta, '<', 'MYMETA.yml');
  ok( $meta, "opened MYMETA.yml" );
  ok( scalar( grep { /generated_by:.*9999/ } <$meta> ),
    "dist_dir Build.PL loaded bundled Module::Build"
  );
  close $meta;

  #--------------------------------------------------------------------------#
  # test identification of dependencies
  #--------------------------------------------------------------------------#

  $dist->chdir_in;

  $dist->add_file( 'mylib/Foo.pm', << 'HERE' );
package Foo;
our $VERSION = 1;
1;
HERE

  $dist->add_file( 'mylib/Bar.pm', << 'HERE' );
package Bar;
use Foo;
our $VERSION = 42;
1;
HERE

  $dist->change_file( 'Build.PL', << "HERE" );
use inc::latest 'Module::Build';
use inc::latest 'Foo';

Module::Build->new(
  module_name => '$dist->{name}',
  license => 'perl',
)->create_build_script;
HERE

  $dist->regen( clean => 1 );

  make_packlist($_,'mylib') for qw/Foo Bar/;

  # get a Module::Build object and test with it
  my $abs_mylib = File::Spec->rel2abs('mylib');


  unshift @INC, $abs_mylib;
  $mb = $dist->new_from_context(); # quiet by default
  isa_ok( $mb, "Module::Build" );
  is_deeply( [sort @{$mb->bundle_inc}], [ 'Foo', 'Module::Build' ],
    "Module::Build and Foo are flagged for bundling"
  );

  my $output = stdout_stderr_of( sub { $mb->dispatch('distdir') } );

  ok( -e File::Spec->catfile( $dist_inc, 'latest.pm' ),
    "./inc/latest.pm created"
  );

  ok( -d File::Spec->catdir( $dist_inc, 'inc_Foo' ),
    "dist_dir/inc/inc_Foo created"
  );

  $dist->change_file( 'Build.PL', << "HERE" );
use inc::latest 'Module::Build';
use inc::latest 'Bar';

Module::Build->new(
  module_name => '$dist->{name}',
  license => 'perl',
)->create_build_script;
HERE

  $dist->regen( clean => 1 );
  make_packlist($_,'mylib') for qw/Foo Bar/;

  $mb = $dist->new_from_context(); # quiet by default
  isa_ok( $mb, "Module::Build" );
  is_deeply( [sort @{$mb->bundle_inc}], [ 'Bar', 'Module::Build' ],
    "Module::Build and Bar are flagged for bundling"
  );

  $output = stdout_stderr_of( sub { $mb->dispatch('distdir') } );

  ok( -e File::Spec->catfile( $dist_inc, 'latest.pm' ),
    "./inc/latest.pm created"
  );

  ok( -d File::Spec->catdir( $dist_inc, 'inc_Bar' ),
    "dist_dir/inc/inc_Bar created"
  );
}


sub make_packlist {
  my ($mod, $lib) = @_;
  my $arch = $Config{archname};
  (my $mod_path = $mod) =~ s{::}{/}g;
  my $mod_file = File::Spec->catfile( $lib, "$mod_path\.pm" );
  my $abs = File::Spec->rel2abs($mod_file);
  my $packlist_path = File::Spec->catdir($lib, $arch, 'auto', $mod_path);
  mkpath $packlist_path;
  my $packlist = ExtUtils::Packlist->new;
  $packlist->{$abs}++;
  $packlist->write( File::Spec->catfile( $packlist_path, '.packlist' ));
}

# vim:ts=2:sw=2:et:sta:sts=2
