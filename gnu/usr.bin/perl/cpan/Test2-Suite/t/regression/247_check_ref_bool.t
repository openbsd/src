use Test2::V0;

BEGIN {
    skip_all "Need JSON::MaybeXS: $@" unless eval {
        require JSON::MaybeXS;
        JSON::MaybeXS->import(qw/decode_json/);
        1;
    };
}

my $data = '{ "aaa": true, "bbb": false }';
my $h = decode_json($data);

ok($h->{aaa}, "true");
ok(!$h->{bbb}, "false");
is($h->{aaa}, T(), 'Test true on true');
is($h->{bbb}, F(), 'Test false on false');
is($h, hash {aaa => T(), etc}, 'Test true on true');
is($h, hash {bbb => F(), etc}, 'Test false on false');

my $events = intercept {
    ok(!$h->{aaa}, "true");
    ok($h->{bbb}, "false");
    is($h, hash {field aaa => F(); etc}, 'Test false on true');
    is($h, hash {field bbb => T(); etc}, 'Test true on false');
};

is(
    [map { $_->causes_fail ? 1 : 0 } grep { $_->facet_data->{assert} } @$events],
    [1, 1, 1, 1],
    "All 4 events cause failure"
);

done_testing;
