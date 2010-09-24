#!perl -w

BEGIN {
  push @INC, "::lib:$MacPerl::Architecture:" if $^O eq 'MacOS';
  require Config; import Config;
  if ($Config{'extensions'} !~ /\bXS\/APItest\b/) {
    # Look, I'm using this fully-qualified variable more than once!
    my $arch = $MacPerl::Architecture;
    print "1..0 # Skip: XS::APItest was not built\n";
    exit 0;
  }
  if ($] < 5.009) {
    print "1..0 # Skip: hints hash not present before 5.10.0\n";
    exit 0;
  }
}

use strict;
use utf8;
use Test::More 'no_plan';

use_ok('XS::APItest');

*hint_exists = *hint_exists = \&XS::APItest::Hash::refcounted_he_exists;
*hint_fetch = *hint_fetch = \&XS::APItest::Hash::refcounted_he_fetch;

require '../../t/op/caller.pl';
