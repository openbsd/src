use Test2::Bundle::Extended;
use Test2::Tools::Spec qw/:ALL/;
use Test2::Util qw/get_tid/;

sub get_ids {
    return {
        pid => $$,
        tid => get_tid(),
    };
}

my $orig = get_ids();

spec_defaults case  => (iso => 1, async => 1);
spec_defaults tests => (iso => 1, async => 1);

tests outside => sub {
    isnt(get_ids(), $orig, "In child (lexical)");
};

describe wrapper => sub {
    case foo => sub {
        isnt(get_ids(), $orig, "In child (inherited)")
    };

    case 'bar', {iso => 0, async => 0} => sub {
        is(get_ids(), $orig, "In orig (overridden)")
    };

    tests a => sub { ok(1, 'stub') };
    tests b => sub { ok(1, 'stub') };

    my $x = describe nested => sub {
        tests nested_t => sub { ok(0, 'Should not see this') };
    };

    tests nested => sub {
        ok(!$x->primary->[0]->iso, "Did not inherit when captured");
        ok(!$x->primary->[0]->async, "Did not inherit when captured");
    };
};

done_testing;
