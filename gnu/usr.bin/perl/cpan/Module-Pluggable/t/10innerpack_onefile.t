#!perl -wT

use strict;
use Test::More tests => 2;
use Data::Dumper;

my $mc  = MyClass->new();
my $mc2 = MyClass2->new();


is_deeply([$mc->plugins],  [qw(MyClass::Plugin::MyPlugin)], "Got inner plugin");
is_deeply([$mc2->plugins], [],                              "Didn't get plugin");

package MyClass::Plugin::MyPlugin;
sub pretty { print "I am pretty" };

package MyClass;
use Module::Pluggable inner => 1;

sub new { return bless {}, $_[0] }

package MyClass2;
use Module::Pluggable search_path => "MyClass::Plugin", inner => 0;

sub new { return bless {}, $_[0] }
1;

