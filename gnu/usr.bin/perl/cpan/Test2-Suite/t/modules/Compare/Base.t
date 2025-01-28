use Test2::Bundle::Extended -target => 'Test2::Compare::Base';

my $one = $CLASS->new();
isa_ok($one, $CLASS);

is($one->delta_class, 'Test2::Compare::Delta', "Got expected delta class");

is([$one->deltas],    [], "no deltas");
is([$one->got_lines], [], "no lines");

is($one->operator, '', "no default operator");

like(dies { $one->verify }, qr/unimplemented/, "unimplemented");
like(dies { $one->name },   qr/unimplemented/, "unimplemented");

{
    no warnings 'redefine';
    *Test2::Compare::Base::name = sub { 'bob' };
    *Test2::Compare::Base::verify = sub { shift; my %p = @_; $p{got} eq 'xxx' };
}

is($one->render, 'bob', "got name");

is(
    [$one->run(id => 'xxx', got => 'xxx', convert => sub { $_[-1] }, seen => {})],
    [],
    "Valid"
);

is(
    [$one->run(id => [META => 'xxx'], got => 'xxy', convert => sub { $_[-1] }, seen => {})],
    [
        {
            verified => '',
            id       => [META => 'xxx'],
            got      => 'xxy',
            chk      => {%$one},
            children => [],
        }
    ],
    "invalid"
);

$one = $CLASS->new;
is($one->lines, [], "no lines");

my $line1 = __LINE__ + 1;
$one = $CLASS->new(builder => sub {
    print "A";
    print "B";
});
my $line2 = __LINE__ - 1;

is($one->lines, [$line1, $line2], "got lines from builder.");

$one = $CLASS->new(called => ['foo', 'bar', 42]);
is($one->lines, [42], "got line from caller");

done_testing;
