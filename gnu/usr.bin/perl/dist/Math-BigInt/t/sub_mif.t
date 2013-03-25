#!/usr/bin/perl -w

# test rounding, accuracy, precision and fallback, round_mode and mixing
# of classes

use strict;
use Test::More tests => 684;

BEGIN { unshift @INC, 't'; }

use Math::BigInt::Subclass;
use Math::BigFloat::Subclass;

use vars qw/$mbi $mbf/;

$mbi = 'Math::BigInt::Subclass';
$mbf = 'Math::BigFloat::Subclass';

require 't/mbimbf.inc';
