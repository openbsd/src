#!perl -w

use strict;
use FindBin;
use lib (($FindBin::Bin."/lib")=~/^(.*)$/);
use Test::More tests => 3;

my $foo;
ok($foo = MyTest->new());

my @plugins;
my @expected = qw(MyTest::Extend::Plugin::Bar MyTest::Plugin::Bar MyTest::Plugin::Foo MyTest::Plugin::Quux::Foo);

push @plugins,  $foo->plugins;
push @plugins, $foo->foo;

@plugins = sort @plugins;
is_deeply(\@plugins, \@expected);

@plugins = ();

push @plugins,  MyTest->plugins;
push @plugins,  MyTest->foo; 
@plugins = sort @plugins;
is_deeply(\@plugins, \@expected);



package MyTest;

use strict;
use Module::Pluggable;
use Module::Pluggable ( search_path => [ "MyTest::Extend::Plugin" ] , sub_name => 'foo' );


sub new {
    my $class = shift;
    return bless {}, $class;

}


1;

