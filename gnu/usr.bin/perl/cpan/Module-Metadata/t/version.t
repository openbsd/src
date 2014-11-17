use strict;
use warnings;
use Test::More;
use Module::Metadata;
use lib "t/lib/0_2";

plan tests => 4;

require Foo;
is $Foo::VERSION, 0.2;

my $meta = Module::Metadata->new_from_module("Foo", inc => [ "t/lib/0_1" ] );
is $meta->version, 0.1;

is $Foo::VERSION, 0.2;

ok eval "use Foo 0.2; 1";






