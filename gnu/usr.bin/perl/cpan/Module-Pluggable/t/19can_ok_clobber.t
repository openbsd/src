#!/usr/bin/perl
use strict; 
use warnings;
use Data::Dumper;
use FindBin;
use lib (($FindBin::Bin."/lib")=~/^(.*)$/);

use Test::More tests=>5;

#use_ok( 'MyTest' );
#diag "Module::Pluggable::VERSION $Module::Pluggable::VERSION";

my @plugins = sort MyTest->plugins;
my @plugins_after;

use_ok( 'MyTest::Plugin::Foo' );
ok( my $foo = MyTest::Plugin::Foo->new() );

@plugins_after = MyTest->plugins;
is_deeply(
    \@plugins_after,
    \@plugins,
    "plugins haven't been clobbered",
) or diag Dumper(\@plugins_after,\@plugins);

can_ok ($foo, 'frobnitz');

@plugins_after = sort MyTest->plugins;
is_deeply(
    \@plugins_after,
    \@plugins,
    "plugins haven't been clobbered",
) or diag Dumper(\@plugins_after,\@plugins);



package MyTest;

use strict;
use Module::Pluggable;


sub new {
    my $class = shift;
    return bless {}, $class;

}
1;


