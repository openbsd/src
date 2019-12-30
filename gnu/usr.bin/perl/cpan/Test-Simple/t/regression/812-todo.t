use strict;
use warnings;

use Test2::API qw/intercept/;
use Test::More;

my @values = (
    "",               # false but defined -> inconsistent
    0,                # false but defined -> inconsistent
    0.0,              # false but defined -> inconsistent
    "0.0",            # true -> TODO
    "this is why",    # as expected
);

for my $value (@values) {
    local $TODO = $value;
    my $x = defined($value) ? "\"$value\"" : 'UNDEF';
    fail "Testing: $x";
}

done_testing;
