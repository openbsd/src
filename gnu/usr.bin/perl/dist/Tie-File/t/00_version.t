#!/usr/bin/perl

print "1..1\n";

my $testversion = "0.99";
use Tie::File;

if ($Tie::File::VERSION != $testversion) {
  print STDERR "

*** WHOA THERE!!! ***

You seem to be running version $Tie::File::VERSION of the module
against version $testversion of the test suite!

None of the other test results will be reliable.
";
  exit 1;
}

print "ok 1\n";

