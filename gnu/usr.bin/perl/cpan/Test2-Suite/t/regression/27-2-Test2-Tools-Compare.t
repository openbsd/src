use Test2::Tools::Compare;
use strict;
use warnings;

is({a => [1]}, {a => [1]}, "is() works, stuff is loaded");

require Test2::Tools::Basic;
Test2::Tools::Basic::done_testing();
