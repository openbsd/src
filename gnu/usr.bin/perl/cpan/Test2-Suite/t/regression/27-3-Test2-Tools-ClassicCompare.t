use Test2::Tools::ClassicCompare;
use strict;
use warnings;

is_deeply({a => [1]}, {a => [1]}, "is_deeply() works, stuff is loaded");

require Test2::Tools::Basic;
Test2::Tools::Basic::done_testing();
