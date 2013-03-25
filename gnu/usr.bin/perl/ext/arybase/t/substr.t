use warnings; no warnings 'deprecated';
use strict;

use Test::More tests => 6;

our $t;

$[ = 3;

$t = "abcdef";
is substr($t, 5), "cdef";
is $t, "abcdef";

$t = "abcdef";
is substr($t, 5, 2), "cd";
is $t, "abcdef";

$t = "abcdef";
is substr($t, 5, 2, "xyz"), "cd";
is $t, "abxyzef";

1;
