# sample.t -- a sample test file for Module::Build

use strict;
use lib 't/lib';
use MBTest;
use DistGen;

plan tests => 8; # or 'no_plan'

# Ensure any Module::Build modules are loaded from correct directory
blib_load('Module::Build');

# create dist object in a temp directory
# enter the directory and generate the skeleton files
my $dist = DistGen->new->chdir_in->regen;

$dist->add_file('t/subtest/foo.t', <<'END_T');
use strict;
use Test::More tests => 1;
ok(1, "this is a recursive test");
END_T

$dist->regen;

# get a Module::Build object and test with it
my $mb = $dist->new_from_context(); # quiet by default
isa_ok( $mb, "Module::Build" );
is( $mb->dist_name, "Simple", "dist_name is 'Simple'" );
ok( ! $mb->recursive_test_files, "set for no recursive testing" );

# set for recursive testing using PERL_MB_OPT
{
  local $ENV{PERL_MB_OPT} = "--verbose --recursive_test_files 1";

  my $out = stdout_stderr_of( sub {
      $dist->run_build('test');
  });
  like( $out, qr/this is a recursive test/,
    "recursive tests run via PERL_MB_OPT"
  );
}

# set Build.PL opts using PERL_MB_OPT
{
  local $ENV{PERL_MB_OPT} = "--verbose --recursive_test_files 1";
  my $mb = $dist->new_from_context(); # quiet by default
  ok( $mb->recursive_test_files, "PERL_MB_OPT set recusive tests in Build.PL" );
  ok( $mb->verbose, "PERL_MB_OPT set verbose in Build.PL" );
}

# verify settings preserved during 'Build test'
{
  ok( !$ENV{PERL_MB_OPT}, "PERL_MB_OPT cleared" );
  my $out = stdout_stderr_of( sub {
      $dist->run_build('test');
  });
  like( $out, qr/this is a recursive test/,
    "recursive tests run via Build object"
  );
}

# vim:ts=2:sw=2:et:sta:sts=2
