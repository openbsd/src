#!perl
use strict;
use warnings;

use Test::More;

use XS::APItest qw(newSVpvf_blank);

# gh #23393 - empty format string crashes newSVpvf
is newSVpvf_blank, "", 'newSVpvf("") returns "" without crashing';

done_testing;
