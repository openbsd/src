use Test2::Bundle::Extended -target => 'Test2::Compare::Pattern';

my $one = $CLASS->new(pattern => qr/HASH/);
isa_ok($one, $CLASS, 'Test2::Compare::Base');
is($one->name, "" . qr/HASH/, "got name");
is($one->operator, '=~', "got operator");
ok(!$one->verify(got => {}, exists => 1), "A hashref does not validate against the pattern 'HASH'");
ok(!$one->verify(exists => 0), "DNE does not validate");
ok(!$one->verify(exists => 1, got => undef), "undef does not validate");
ok(!$one->verify(exists => 1, got => 'foo'), "Not a match");
ok($one->verify(exists => 1, got => 'A HASH B'), "Matches");

$one = $CLASS->new(pattern => qr/HASH/, negate => 1);
isa_ok($one, $CLASS, 'Test2::Compare::Base');
is($one->name, "" . qr/HASH/, "got name");
is($one->operator, '!~', "got operator");
ok(!$one->verify(exists => 1, got => {}), "A hashref does not validate against the pattern 'HASH' even when negated");
ok(!$one->verify(exists => 0), "DNE does not validate");
ok(!$one->verify(exists => 1, got => undef), "undef does not validate");
ok($one->verify(exists => 1, got => 'foo'), "Not a match, but negated");
ok(!$one->verify(exists => 1, got => 'A HASH B'), "Matches, but negated");


like(
    dies { $CLASS->new },
    qr/'pattern' is a required attribute/,
    "Need to specify a pattern"
);

done_testing;
