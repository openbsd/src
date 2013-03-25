use warnings; no warnings 'deprecated';
use strict;

use Test::More tests => 12;

our $t = "abcdefghijkl";

$[ = 3;

is index($t, "cdef"), 5;
is index($t, "cdef", 3), 5;
is index($t, "cdef", 4), 5;
is index($t, "cdef", 5), 5;
is index($t, "cdef", 6), 2;
is index($t, "cdef", 7), 2;
is rindex($t, "cdef"), 5;
is rindex($t, "cdef", 7), 5;
is rindex($t, "cdef", 6), 5;
is rindex($t, "cdef", 5), 5;
is rindex($t, "cdef", 4), 2;
is rindex($t, "cdef", 3), 2;

1;
