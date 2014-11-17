#!/usr/bin/perl -w

use strict;
use lib 't/lib';
use MBTest;
use CPAN::Meta 2.110420;
use CPAN::Meta::YAML;
use Parse::CPAN::Meta 1.4401;
plan tests => 39;

blib_load('Module::Build');

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
  configure_requires  => {
    'Module::Build' => '0.42',
  }
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
  ok( ! -e "META.json", "META.json doesn't exist before Build.PL runs" );
  ok( ! -e "MYMETA.json", "MYMETA.json doesn't exist before Build.PL runs" );
  my $output;
  $output = stdout_of sub { $dist->run_build_pl };
  like($output, qr/Created MYMETA\.yml and MYMETA\.json/,
    "Ran Build.PL and saw MYMETA.yml creation message"
  );
  ok( -e "MYMETA.yml", "MYMETA.yml exists" );
  ok( -e "MYMETA.json", "MYMETA.json exists" );
}

#########################

# Test interactions between META/MYMETA
{
  my $output = stdout_stderr_of sub { $dist->run_build('distmeta') };
  like($output, qr/Created META\.yml and META\.json/,
    "Ran Build distmeta to create META.yml");
  # regenerate MYMETA to pick up from META instead of creating from scratch
  $output = stdout_of sub { $dist->run_build_pl };
  like($output, qr/Created MYMETA\.yml and MYMETA\.json/,
    "Re-ran Build.PL and regenerated MYMETA.yml based on META.yml"
  );

  for my $suffix ( qw/.yml .json/ ) {
    my $meta = Parse::CPAN::Meta->load_file("META$suffix");
    my $mymeta = Parse::CPAN::Meta->load_file("MYMETA$suffix");
    is( delete $meta->{dynamic_config}, 1,
      "META$suffix 'dynamic_config' is 1"
    );
    is( delete $mymeta->{dynamic_config}, 0,
      "MYMETA$suffix 'dynamic_config' is 0"
    );
    is_deeply( $mymeta, $meta, "Other generated MYMETA$suffix matches generated META$suffix" )
      or do {
        require Data::Dumper;
        diag "MYMETA:\n" . Data::Dumper::Dumper($mymeta)
          .  "META:\n" . Data::Dumper::Dumper($meta);
      };
  }

  $output = stdout_stderr_of sub { $dist->run_build('realclean') };
  like( $output, qr/Cleaning up/, "Ran realclean");
  ok( ! -e 'Build', "Build file removed" );
  ok( ! -e 'MYMETA.yml', "MYMETA.yml file removed" );
  ok( ! -e 'MYMETA.json', "MYMETA.json file removed" );

  # test that dynamic prereq is picked up
  my $meta = Parse::CPAN::Meta->load_file("META.yml");
  my $meta2 = Parse::CPAN::Meta->load_file("META.json");
  local $ENV{BUMP_PREREQ} = 1;
  $output = stdout_of sub { $dist->run_build_pl };
  like($output, qr/Created MYMETA\.yml and MYMETA\.json/,
    "Ran Build.PL with dynamic config"
  );
  ok( -e "MYMETA.yml", "MYMETA.yml exists" );
  ok( -e "MYMETA.json", "MYMETA.json exists" );
  my $mymeta = Parse::CPAN::Meta->load_file('MYMETA.yml');
  my $mymeta2 = Parse::CPAN::Meta->load_file('MYMETA.json');
  isnt(   $meta->{requires}{'File::Spec'},
        $mymeta->{requires}{'File::Spec'},
        "MYMETA.yml requires differs from META.yml"
  );
  isnt(   $meta2->{prereqs}{runtime}{requires}{'File::Spec'},
        $mymeta2->{prereqs}{runtime}{requires}{'File::Spec'},
        "MYMETA.json requires differs from META.json"
  );
  $output = stdout_stderr_of sub { $dist->run_build('realclean') };
  like( $output, qr/Cleaning up/, "Ran realclean");
  ok( ! -e 'Build', "Build file removed" );
  ok( ! -e 'MYMETA.yml', "MYMETA file removed" );
  ok( ! -e 'MYMETA.json', "MYMETA file removed" );

  # manually change META and check that changes are preserved
  $meta->{author} = ['John Gault'];
  $meta2->{author} = ['John Gault'];
  ok( CPAN::Meta::YAML->new($meta)->write('META.yml'),
    "Wrote manually modified META.yml" );
  ok( CPAN::Meta->new( $meta2 )->save('META.json'),
    "Wrote manually modified META.json" );

  $output = stdout_of sub { $dist->run_build_pl };
  like($output, qr/Created MYMETA\.yml and MYMETA\.json/,
    "Ran Build.PL"
  );
  $mymeta = Parse::CPAN::Meta->load_file('MYMETA.yml');
  $mymeta2 = Parse::CPAN::Meta->load_file('MYMETA.json');
  is_deeply( $mymeta->{author}, [ 'John Gault' ],
    "MYMETA.yml preserved META.yml modifications"
  );
  is_deeply( $mymeta2->{author}, [ 'John Gault' ],
    "MYMETA.json preserved META.json modifications"
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
  like($output, qr/Created MYMETA\.yml and MYMETA\.json/,
    "Ran Build.PL and saw MYMETA.yml creation message"
  );
  $output = stdout_stderr_of sub { $dist->run_build('distclean') };
  ok( ! -f 'MYMETA.yml', "No MYMETA.yml after distclean" );
  ok( ! -f 'MYMETA.json', "No MYMETA.json after distclean" );
  ok( ! -f 'MANIFEST.SKIP', "No MANIFEST.SKIP after distclean" );
}


