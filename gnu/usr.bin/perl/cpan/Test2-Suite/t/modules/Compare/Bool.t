use Test2::Bundle::Extended -target => 'Test2::Compare::Bool';

my $one = $CLASS->new(input => 'foo');
is($one->name, '<TRUE (foo)>', "Got name");
is($one->operator, '==', "Got operator");

$one = $CLASS->new(input => 0, negate => 1);
is($one->name, '<FALSE (0)>', "Got name");
is($one->operator, '!=', "Got operator");

done_testing;
