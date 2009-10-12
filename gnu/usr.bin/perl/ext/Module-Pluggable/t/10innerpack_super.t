#!perl -wT

use Test::More tests => 3;
use strict;
use_ok('Devel::InnerPackage');
Bar->whee;
is_deeply([Devel::InnerPackage::list_packages("Bar")],[], "Don't pick up ::SUPER pseudo stash"); 
is_deeply([Devel::InnerPackage::list_packages("Foo")],['Foo::Bar'], "Still pick up other inner package");

package Foo;

sub whee {
    1;
}

package Foo::Bar;

sub whee {}

package Bar;
use base 'Foo';

sub whee {
    shift->SUPER::whee;
    2;
}


1;
