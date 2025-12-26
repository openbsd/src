use Test2::Tools::Class;
use strict;
use warnings;

{
    package My::Object;
    use overload 'bool' => sub {$_[0]->{value}}
}

my $true_value  = bless {value => 1}, 'My::Object';
my $false_value = bless {value => 0}, 'My::Object';

isa_ok($true_value,  ['My::Object'], 'isa_ok when object overloads to true');
isa_ok($false_value, ['My::Object'], 'isa_ok when object overloads to false');

require Test2::Tools::Basic;
Test2::Tools::Basic::done_testing();
