use Test2::Tools::Defer;
use strict;
use warnings;

# Make sure convert loads necessary modules (must do before loading the
# extended bundle)
BEGIN {
    require Test2::Compare;
    def ok => (defined Test2::Compare::convert(undef), "convert returned something to us");
    def ok => ($INC{'Test2/Compare/Undef.pm'}, "loaded the Test2::Compare::Undef module");
}

use Test2::Bundle::Extended;
use Test2::API qw/intercept/;
use Data::Dumper;

use Test2::Compare qw{
    compare get_build push_build pop_build build
    strict_convert relaxed_convert
};
pass "Loaded Test2::Compare";

imported_ok qw{
    compare get_build push_build pop_build build
    strict_convert relaxed_convert
};

do_def;

{
    package Fake::Check;

    sub run {
        my $self = shift;
        return {@_, self => $self}
    }
}

my $check = bless {}, 'Fake::Check';
my $convert = sub { $_[-1]->{ran}++; $_[-1] };
my $got = compare('foo', $check, $convert);

like(
    $got,
    {
        self    => {ran => 1},
        id      => undef,
        got     => 'foo',
        convert => sub  { $_ == $convert },
        seen    => {},
    },
    "check got expected args"
);

is(get_build(), undef, "no build");

like(
    dies { pop_build(['a']) },
    qr/INTERNAL ERROR: Attempted to pop incorrect build, have undef, tried to pop ARRAY/,
    "Got error popping from nothing"
);

push_build(['a']);
is(get_build(), ['a'], "pushed build");

like(
    dies { pop_build() },
    qr/INTERNAL ERROR: Attempted to pop incorrect build, have ARRAY\(.*\), tried to pop undef/,
    "Got error popping undef"
);

like(
    dies { pop_build(['a']) },
    qr/INTERNAL ERROR: Attempted to pop incorrect build, have ARRAY\(.*\), tried to pop ARRAY/,
    "Got error popping wrong ref"
);

# Don't ever actually do this...
ok(pop_build(get_build()), "Popped");

my $inner;
my $build = sub { build('Test2::Compare::Array', sub {
    local $_ = 1;
    $inner = get_build();
}) }->();
is($build->lines, [__LINE__ - 4, __LINE__ - 1], "got lines");
is($build->file, __FILE__, "got file");

ref_is($inner, $build, "Build was set inside block");

like(
    dies { my $x = build('Test2::Compare::Array', sub { die 'xxx' }) },
    qr/xxx at/,
    "re-threw exception"
);

like(
    dies { build('Test2::Compare::Array', sub { }) },
    qr/should not be called in void context/,
    "You need to retain the return from build"
);

subtest convert => sub {
    my $true  = do { bless \(my $dummy = 1), "My::Boolean" };
    my $false = do { bless \(my $dummy = 0), "My::Boolean" };

    my @sets = (
        ['a',   'String', 'String'],
        [undef, 'Undef', 'Undef'],
        ['',    'String', 'String'],
        [1,     'String', 'String'],
        [0,     'String', 'String'],
        [[],    'Array',  'Array'],
        [{},    'Hash',   'Hash'],
        [qr/x/, 'Regex',  'Pattern'],
        [sub { 1 }, 'Ref', 'Custom'],
        [\*STDERR, 'Ref',    'Ref'],
        [\'foo',   'Scalar', 'Scalar'],
        [\v1.2.3,  'Scalar', 'Scalar'],
        [$true,    'Scalar', 'Scalar'],
        [$false,   'Scalar', 'Scalar'],

        [
            bless({}, 'Test2::Compare::Base'),
            'Base',
            'Base'
        ],

        [
            bless({expect => 'a'}, 'Test2::Compare::Wildcard'),
            'String',
            'String',
        ],
    );

    for my $set (@sets) {
        my ($item, $strict, $relaxed) = @$set;

        my $name = defined $item ? "'$item'" : 'undef';

        my $gs = strict_convert($item);
        my $st = join '::', grep {$_} 'Test2::Compare', $strict;
        ok($gs->isa($st), "$name -> $st") || diag Dumper($item);

        my $gr = relaxed_convert($item);
        my $rt = join '::', grep {$_} 'Test2::Compare', $relaxed;
        ok($gr->isa($rt), "$name -> $rt") || diag Dumper($item);
    }
};

done_testing;
