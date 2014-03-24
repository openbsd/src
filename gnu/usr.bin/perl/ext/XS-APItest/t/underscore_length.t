use warnings; no warnings 'experimental::lexical_topic';
use strict;

use Test::More tests => 4;

use XS::APItest qw(underscore_length);

$_ = "foo";
is underscore_length(), 3;

$_ = "snowman \x{2603}";
is underscore_length(), 9;

my $_ = "xyzzy";
is underscore_length(), 5;

$_ = "pile of poo \x{1f4a9}";
is underscore_length(), 13;

1;
