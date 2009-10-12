#!/usr/bin/perl -w

use strict;
use lib $ENV{PERL_CORE} ? '../lib/Module/Build/t/lib' : 't/lib';
use MBTest tests => 3;

require_ok('Module::Build');
ensure_blib('Module::Build');

my $tmp = MBTest->tmpdir;

use DistGen;
my $dist = DistGen->new( dir => $tmp );
$dist->regen;
END{ $dist->remove }

$dist->chdir_in;

#########################

# Test debug output
{
  my $output;
  $output = stdout_of sub {
    Module::Build->run_perl_script('Build.PL', [], [])
  };
  $output = stdout_of sub {
    Module::Build->run_perl_script('Build', [], ['--debug'])
  };
  like($output, '/Starting ACTION_build.*?Starting ACTION_code.*?Finished ACTION_code.*?Finished ACTION_build/ms',
    "found nested ACTION_* debug statements"
  );
}

#########################

# cleanup
