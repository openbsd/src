package local_A;
use strict;
use Test::More;

is_deeply \@_, [], '@_ is empty when a module is loaded';
@_ = qw(Foo Bar);

1;
