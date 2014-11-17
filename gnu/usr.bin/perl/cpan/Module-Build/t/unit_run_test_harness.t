#!/usr/bin/perl -w

use strict;
use lib 't/lib';
use MBTest tests => 9;

blib_load('Module::Build');

my $tmp = MBTest->tmpdir;

use DistGen;
my $dist = DistGen->new( dir => $tmp );
$dist->regen;

$dist->chdir_in;

#########################


# make sure Test::Harness loaded before we define Test::Harness::runtests otherwise we'll
# get another redefined warning inside Test::Harness::runtests
use Test::Harness;

{
  package MB::Subclass;
  use base qw(Module::Build);
  sub harness_switches { return }
}

{
  local $SIG{__WARN__} = sub { die "Termination after a warning: $_[0]"};
  my $mock1 = { A => 1 };
  my $mock2 = { B => 2 };

  no warnings qw[redefine once];

  # This runs run_test_harness with Test::Harness::switches = undef and harness_switches() returning empty list,
  # ensure there are no warnings, and output is empty too
  {
    my $mb = MB::Subclass->new( module_name => $dist->name );
    local *Test::Harness::runtests = sub {
      is shift(), $mock1, "runtests ran with expected parameters";
      is shift(), $mock2, "runtests ran with expected parameters";
      is $Test::Harness::switches, '', "switches are undef";
      is $Test::Harness::Switches, '', "switches are undef";
    };

    # $Test::Harness::switches and $Test::Harness::switches are aliases, but we pretend we don't know this
    local $Test::Harness::switches = '';
    local $Test::Harness::switches = '';
    $mb->run_test_harness([$mock1, $mock2]);

    ok 1, "run_test_harness should not produce warning if Test::Harness::[Ss]witches are undef and harness_switches() return empty list";
  }

  # This runs run_test_harness with Test::Harness::switches = '' and harness_switches() returning empty list,
  # ensure there are no warnings, and switches are empty string
  {
    my $mb = MB::Subclass->new( module_name => $dist->name );
    local *Test::Harness::runtests = sub {
      is shift(), $mock1, "runtests ran with expected parameters";
      is shift(), $mock2, "runtests ran with expected parameters";
      is $Test::Harness::switches, '', "switches are empty string";
      is $Test::Harness::Switches, '', "switches are empty string";
    };

    # $Test::Harness::switches and $Test::Harness::switches are aliases, but we pretend we don't know this
    local $Test::Harness::switches = '';
    local $Test::Harness::switches = '';
    $mb->run_test_harness([$mock1, $mock2]);
  }

}
