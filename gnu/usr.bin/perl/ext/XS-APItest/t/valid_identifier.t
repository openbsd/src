#!perl

use strict;
use warnings;

use open ':std', ':encoding(UTF-8)';
use Test::More;

use_ok('XS::APItest');

# These should all be valid
foreach my $id (qw( abc ab_cd _abc x123 )) {
    ok(valid_identifier($id), "'$id' is valid identifier");
}

# These should all not be
foreach my $id (qw( ab-cd 123 abc() ), "ab cd") {
    ok(!valid_identifier($id), "'$id' is not valid identifier");
}

# Now for some UTF-8 tests
{
    use utf8;

    foreach my $id (qw( café sandviĉon )) {
        ok(valid_identifier($id), "'$id' is valid UTF-8 identifier");
    }

    # en-dash
    ok(!valid_identifier("ab–cd"), "'ab–cd' is not valid UTF-8 identifier");
}

# objects with "" overloading still work
{
    package WithStringify {
        use overload '""' => sub { return "an_identifier"; };
        sub new { bless [], shift; }
    }

    ok(valid_identifier(WithStringify->new), 'Object with stringify overload can be valid identifier');
}

done_testing;
