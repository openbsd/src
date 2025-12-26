use Test2::Bundle::Extended -target => 'Test2::Compare::String';

my $number = $CLASS->new(input => '22.0');
my $string = $CLASS->new(input => 'hello');
my $untru1 = $CLASS->new(input => '');
my $untru2 = $CLASS->new(input => 0);

isa_ok($_, $CLASS, 'Test2::Compare::Base') for $number, $string, $untru1, $untru2;

subtest name => sub {
    is($number->name, '22.0',    "got expected name");
    is($string->name, 'hello',   "got expected name");
    is($untru1->name, '',        "got expected name");
    is($untru2->name, '0',       "got expected name");
};

subtest operator => sub {
    is($number->operator(),      '',   "no operator for number + nothing");
    is($number->operator(undef), '',   "no operator for number + undef");
    is($number->operator('x'),   'eq', "eq operator for number + string");
    is($number->operator(1),     'eq', "eq operator for number + number");

    is($string->operator(),      '',   "no operator for string + nothing");
    is($string->operator(undef), '',   "no operator for string + undef");
    is($string->operator('x'),   'eq', "eq operator for string + string");
    is($string->operator(1),     'eq', "eq operator for string + number");

    is($untru1->operator(),      '',   "no operator for empty string + nothing");
    is($untru1->operator(undef), '',   "no operator for empty string + undef");
    is($untru1->operator('x'),   'eq', "eq operator for empty string + string");
    is($untru1->operator(1),     'eq', "eq operator for empty string + number");

    is($untru2->operator(),      '',   "no operator for 0 + nothing");
    is($untru2->operator(undef), '',   "no operator for 0 + undef");
    is($untru2->operator('x'),   'eq', "eq operator for 0 + string");
    is($untru2->operator(1),     'eq', "eq operator for 0 + number");
};

subtest verify => sub {
    ok(!$number->verify(exists => 0, got => undef), 'does not verify against DNE');
    ok(!$number->verify(exists => 1, got => {}),    'ref will not verify');
    ok(!$number->verify(exists => 1, got => undef), 'looking for a number, not undef');
    ok(!$number->verify(exists => 1, got => 'x'),   'not looking for a string');
    ok(!$number->verify(exists => 1, got => 1),     'wrong number');
    ok(!$number->verify(exists => 1, got => 22),     '22.0 ne 22');
    ok($number->verify(exists => 1, got => '22.0'), 'exact match with decimal');

    ok(!$string->verify(exists => 0, got => undef), 'does not verify against DNE');
    ok(!$string->verify(exists => 1, got => {}),    'ref will not verify');
    ok(!$string->verify(exists => 1, got => undef), 'looking for a string, not undef');
    ok(!$string->verify(exists => 1, got => 'x'),   'looking for a different string');
    ok(!$string->verify(exists => 1, got => 1),     'looking for a string, not a number');
    ok($string->verify(exists => 1, got => 'hello'), 'exact match');

    ok(!$untru1->verify(exists => 0, got => undef), 'does not verify against DNE');
    ok(!$untru1->verify(exists => 1, got => {}),    'ref will not verify');
    ok(!$untru1->verify(exists => 1, got => undef), 'looking for a string, not undef');
    ok(!$untru1->verify(exists => 1, got => 'x'),   'wrong string');
    ok(!$untru1->verify(exists => 1, got => 1),     'not a number');
    ok($untru1->verify(exists => 1, got => ''), 'exact match, empty string');

    ok(!$untru2->verify(exists => 0, got => undef), 'does not verify against DNE');
    ok(!$untru2->verify(exists => 1, got => {}),    'ref will not verify');
    ok(!$untru2->verify(exists => 1, got => undef), 'undef is not 0 for this test');
    ok(!$untru2->verify(exists => 1, got => 'x'),   'x is not 0');
    ok(!$untru2->verify(exists => 1, got => 1),     '1 is not 0');
    ok(!$untru2->verify(exists => 1, got => '0.0'),  '0.0 ne 0');
    ok(!$untru2->verify(exists => 1, got => '-0.0'), '-0.0 ne 0');
    ok($untru2->verify(exists => 1, got => 0),      'got 0');
};

like(
    dies { $CLASS->new() },
    qr/input must be defined for 'String' check/,
    "Cannot use undef as a string"
);

done_testing;
