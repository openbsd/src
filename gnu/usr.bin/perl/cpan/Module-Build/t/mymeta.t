#!/usr/bin/perl -w

use strict;
use lib 't/lib';
use MBTest;
plan tests => 24;

blib_load('Module::Build');
blib_load('Module::Build::YAML');

my $tmp = MBTest->tmpdir;

use DistGen;
my $dist = DistGen->new( dir => $tmp );
$dist->change_file('Build.PL', <<"---");
use strict;
use Module::Build;

my \$builder = Module::Build->new(
  module_name         => '$dist->{name}',
  license             => 'perl',
  requires            => {
    'File::Spec' => ( \$ENV{BUMP_PREREQ} ? 0.86 : 0 ),
  },
);

\$builder->create_build_script();
---
$dist->regen;
$dist->chdir_in;

#########################

# Test MYMETA generation
{
  ok( ! -e "META.yml", "META.yml doesn't exist before Build.PL runs" );
  ok( ! -e "MYMETA.yml", "MYMETA.yml doesn't exist before Build.PL runs" );
  my $output;
  $output = stdout_of sub { $dist->run_build_pl };
  like($output, qr/Creating new 'MYMETA.yml' with configuration results/,
    "Ran Build.PL and saw MYMETA.yml creation message"
  );
  ok( -e "MYMETA.yml", "MYMETA.yml exists" );
}

#########################

# Test interactions between META/MYMETA
{
  my $output = stdout_of sub { $dist->run_build('distmeta') };
  like($output, qr/Creating META.yml/,
    "Ran Build distmeta to create META.yml");
  my $meta = Module::Build::YAML->read('META.yml')->[0];
  my $mymeta = Module::Build::YAML->read('MYMETA.yml')->[0];
  is( delete $mymeta->{dynamic_config}, 0,
    "MYMETA 'dynamic_config' is 0"
  );
  is_deeply( $meta, $mymeta, "Other generated MYMETA matches generated META" );
  $output = stdout_stderr_of sub { $dist->run_build('realclean') };
  like( $output, qr/Cleaning up/, "Ran realclean");
  ok( ! -e 'Build', "Build file removed" );
  ok( ! -e 'MYMETA.yml', "MYMETA file removed" );

  # test that dynamic prereq is picked up
  local $ENV{BUMP_PREREQ} = 1;
  $output = stdout_of sub { $dist->run_build_pl };
  like($output, qr/Creating new 'MYMETA.yml' with configuration results/,
    "Ran Build.PL with dynamic config"
  );
  ok( -e "MYMETA.yml", "MYMETA.yml exists" );
  $mymeta = Module::Build::YAML->read('MYMETA.yml')->[0];
  isnt(   $meta->{requires}{'File::Spec'},
        $mymeta->{requires}{'File::Spec'},
        "MYMETA requires differs from META"
  );
  $output = stdout_stderr_of sub { $dist->run_build('realclean') };
  like( $output, qr/Cleaning up/, "Ran realclean");
  ok( ! -e 'Build', "Build file removed" );
  ok( ! -e 'MYMETA.yml', "MYMETA file removed" );

  # manually change META and check that changes are preserved
  $meta->{author} = ['John Gault'];
  ok( Module::Build::YAML->new($meta)->write('META.yml'),
    "Wrote manually modified META.yml" );

  $output = stdout_of sub { $dist->run_build_pl };
  like($output, qr/Creating new 'MYMETA.yml' with configuration results/,
    "Ran Build.PL"
  );
  my $mymeta2 = Module::Build::YAML->read('MYMETA.yml')->[0];
  is_deeply( $mymeta2->{author}, [ 'John Gault' ],
    "MYMETA preserved META modifications"
  );



}

#########################

# Test cleanup
{
  my $output = stdout_stderr_of sub { $dist->run_build('distcheck') };
  like($output, qr/Creating a temporary 'MANIFEST.SKIP'/,
    "MANIFEST.SKIP created for distcheck"
  );
  unlike($output, qr/MYMETA/,
    "MYMETA not flagged by distcheck"
  );
}


{
  my $output = stdout_of sub { $dist->run_build_pl };
  like($output, qr/Creating new 'MYMETA.yml' with configuration results/,
    "Ran Build.PL and saw MYMETA.yml creation message"
  );
  $output = stdout_stderr_of sub { $dist->run_build('distclean') };
  ok( ! -f 'MYMETA.yml', "No MYMETA.yml after distclean" );
  ok( ! -f 'MANIFEST.SKIP', "No MANIFEST.SKIP after distclean" );
}


