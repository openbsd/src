package local_B;
use strict;
use Test::More;

is_deeply \@_, [], '@_ is empty when a module is loaded';

1;
