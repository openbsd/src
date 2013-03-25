#!./perl -T

# Tests for constant.pm that require the utf8 pragma

use utf8;
use Test::More tests => 2;

use constant π		=> 4 * atan2 1, 1;

ok defined π,                    'basic scalar constant with funny name';
is substr(π, 0, 7), '3.14159',   '    in substr()';

