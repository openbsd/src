use warnings;
use strict;
use Test::Simple tests => 3;

package Foo;
use overload
  nomethod => sub { die "unimplemented\n" };
sub new { bless {}, shift };

package main;

my $foo = Foo->new;

eval {my $val = $foo + 1};
ok( $@ =~ /unimplemented/ );

eval {$foo += 1};
ok( $@ =~ /unimplemented/ );

eval {my $val = 0; $val += $foo};
ok( $@ =~ /unimplemented/ );

