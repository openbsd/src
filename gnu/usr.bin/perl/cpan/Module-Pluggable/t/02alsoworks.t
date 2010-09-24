#!perl -w

use strict;
use FindBin;
use lib (($FindBin::Bin."/lib")=~/^(.*)$/);
use Test::More tests => 5;

my $foo;
ok($foo = MyOtherTest->new());

my @plugins;
my @expected = qw(MyOtherTest::Plugin::Bar MyOtherTest::Plugin::Foo  MyOtherTest::Plugin::Quux MyOtherTest::Plugin::Quux::Foo);
ok(@plugins = sort $foo->plugins);



is_deeply(\@plugins, \@expected, "is deeply");

@plugins = ();

ok(@plugins = sort MyOtherTest->plugins);




is_deeply(\@plugins, \@expected, "is deeply class");



package MyOtherTest;

use strict;
use Module::Pluggable;


sub new {
    my $class = shift;
    return bless {}, $class;

}
1;

