#!/usr/bin/perl -w

# test rounding, accuracy, precicion and fallback, round_mode and mixing
# of classes under BareCalc

use strict;
use Test;

BEGIN
  {
  $| = 1;
  # to locate the testing files
  my $location = $0; $location =~ s/bare_mif.t//i;
  if ($ENV{PERL_CORE})
    {
    @INC = qw(../t/lib); 		# testing with the core distribution
    }
  unshift @INC, '../lib';	# for testing manually
  if (-d 't')
    {
    chdir 't';
    require File::Spec;
    unshift @INC, File::Spec->catdir(File::Spec->updir, $location);
    }
  else
    {
    unshift @INC, $location;
    }
  print "# INC = @INC\n";

  plan tests => 617
    + 1;		# our onw tests
  }

print "# ",Math::BigInt->config()->{lib},"\n";

use Math::BigInt lib => 'BareCalc';
use Math::BigFloat lib => 'BareCalc';

use vars qw/$mbi $mbf/;

$mbi = 'Math::BigInt';
$mbf = 'Math::BigFloat';

ok (Math::BigInt->config()->{lib},'Math::BigInt::BareCalc');

require 'mbimbf.inc';

