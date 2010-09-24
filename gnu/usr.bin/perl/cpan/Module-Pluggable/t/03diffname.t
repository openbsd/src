#!perl -w

use strict;
use FindBin;
use lib (($FindBin::Bin."/lib")=~/^(.*)$/);
use Test::More tests => 3;

my $foo;
ok($foo = MyTest->new());

my @plugins;
my @expected = qw(MyTest::Plugin::Bar MyTest::Plugin::Foo MyTest::Plugin::Quux::Foo);
ok(@plugins = sort $foo->foo);
is_deeply(\@plugins, \@expected);



package MyTest;

use strict;
use Module::Pluggable ( sub_name => 'foo');


sub new {
    my $class = shift;
    return bless {}, $class;

}
1;

