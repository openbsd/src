use Test2::Bundle::More;
use strict;
use warnings;

is_deeply({a => [1]}, {a => [1]}, "is_deeply() works, stuff is loaded");

done_testing;
