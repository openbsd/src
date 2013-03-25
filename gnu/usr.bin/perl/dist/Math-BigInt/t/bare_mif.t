#!/usr/bin/perl -w

# test rounding, accuracy, precision and fallback, round_mode and mixing
# of classes under BareCalc

use strict;
use Test::More tests => 684
    + 1;		# our own tests

BEGIN { unshift @INC, 't'; }

print "# ",Math::BigInt->config()->{lib},"\n";

use Math::BigInt lib => 'BareCalc';
use Math::BigFloat lib => 'BareCalc';

use vars qw/$mbi $mbf/;

$mbi = 'Math::BigInt';
$mbf = 'Math::BigFloat';

is (Math::BigInt->config()->{lib},'Math::BigInt::BareCalc');

require 't/mbimbf.inc';
