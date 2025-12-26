# Tests for overloads (+,-,<,>, etc)
use strict;
use warnings;

use Test::More tests => 1;
use Time::Piece;
my $t = localtime;
my $s = Time::Seconds->new(15);
eval { my $result = $t + $s };
is($@, "", "Adding Time::Seconds does not cause runtime error");

