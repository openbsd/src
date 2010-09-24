#!perl -w

use strict;
use FindBin;
use lib (($FindBin::Bin."/lib")=~/^(.*)$/);
use Test::More tests => 5;

my $foo;
ok($foo = ExtTest->new());

my @plugins;
my @expected = qw(ExtTest::Plugin::Bar ExtTest::Plugin::Foo ExtTest::Plugin::Quux::Foo);
ok(@plugins = sort $foo->plugins);



is_deeply(\@plugins, \@expected, "is deeply");

@plugins = ();

ok(@plugins = sort ExtTest->plugins);




is_deeply(\@plugins, \@expected, "is deeply class");



package ExtTest;

use strict;
use Module::Pluggable file_regex => qr/\.plugin$/;


sub new {
    my $class = shift;
    return bless {}, $class;

}
1;

