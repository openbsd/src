#!perl -w

use strict;
use FindBin;
use lib (($FindBin::Bin."/lib")=~/^(.*)$/);
use Test::More tests => 4;



my $t = InnerTest->new();

my %plugins = map { $_ => 1 } $t->plugins;

ok(keys %plugins, "Got some plugins");
ok($plugins{'InnerTest::Plugin::Foo'}, "Got Foo");
ok($plugins{'InnerTest::Plugin::Bar'}, "Got Bar - the inner package");
ok($plugins{'InnerTest::Plugin::Quux'}, "Got Quux - the other inner package");



package InnerTest;
use strict;
use Module::Pluggable require => 1;
use base qw(Module::Pluggable);


sub new {
    my $class = shift;
    return bless {}, $class;

}


1;

