#!perl -w

use strict;
use FindBin;
use lib (($FindBin::Bin."/lib")=~/^(.*)$/);
use Test::More tests => 5;

my $foo;
ok($foo = MyTest->new());

my @plugins;
my @expected = qw(MyTest::Plugin::Bar MyTest::Plugin::Foo MyTest::Plugin::Quux::Foo);
ok(@plugins = sort $foo->plugins);



is_deeply(\@plugins, \@expected, "is deeply");

@plugins = ();

ok(@plugins = sort MyTest->plugins);




is_deeply(\@plugins, \@expected, "is deeply class");



package MyTest;

use strict;
use Module::Pluggable;


sub new {
    my $class = shift;
    return bless {}, $class;

}
1;

