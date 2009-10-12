#!perl -w

use strict;
use FindBin;
use lib (($FindBin::Bin."/lib")=~/^(.*)$/);
use Test::More tests => 10;

{
    my $foo;
    ok($foo = MyTest->new());

    my @plugins;
    my @expected = qw(MyTest::Plugin::Bar MyTest::Plugin::Quux::Foo);
    ok(@plugins = sort $foo->plugins);

    is_deeply(\@plugins, \@expected);

    @plugins = ();

    ok(@plugins = sort MyTest->plugins);
    is_deeply(\@plugins, \@expected);
}

{
    my $foo;
    ok($foo = MyTestSub->new());

    my @plugins;
    my @expected = qw(MyTest::Plugin::Bar MyTest::Plugin::Quux::Foo);
    ok(@plugins = sort $foo->plugins);

    is_deeply(\@plugins, \@expected);

    @plugins = ();

    ok(@plugins = sort MyTestSub->plugins);
    is_deeply(\@plugins, \@expected);
}

package MyTest;

use strict;
use Module::Pluggable except => "MyTest::Plugin::Foo";



sub new {
    my $class = shift;
    return bless {}, $class;

}

package MyTestSub;

use strict;
use Module::Pluggable search_path => "MyTest::Plugin";


sub new {
    my $class = shift;
    my $self = bless {}, $class;

    $self->except("MyTest::Plugin::Foo");

    return $self;
}
1;

