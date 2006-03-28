#!/usr/bin/perl -w

BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = ('../lib', 'lib');
    }
    else {
        unshift @INC, 't/lib';
    }
}

use strict;
use Test::More;

BEGIN {
    if( !eval "require overload" ) {
        plan skip_all => "needs overload.pm";
    }
    else {
        plan tests => 13;
    }
}


package Overloaded;

use overload
        q{""}    => sub { $_[0]->{string} },
        q{0+}    => sub { $_[0]->{num} };

sub new {
    my $class = shift;
    bless { string => shift, num => shift }, $class;
}


package main;

my $obj = Overloaded->new('foo', 42);
isa_ok $obj, 'Overloaded';

is $obj, 'foo',            'is() with string overloading';
cmp_ok $obj, 'eq', 'foo',  'cmp_ok() ...';
cmp_ok $obj, '==', 42,     'cmp_ok() with number overloading';

is_deeply [$obj], ['foo'],                 'is_deeply with string overloading';
ok eq_array([$obj], ['foo']),              'eq_array ...';
ok eq_hash({foo => $obj}, {foo => 'foo'}), 'eq_hash ...';

# rt.cpan.org 13506
is_deeply $obj, 'foo',        'is_deeply with string overloading at the top';

Test::More->builder->is_num($obj, 42);
Test::More->builder->is_eq ($obj, "foo");


{
    # rt.cpan.org 14675
    package TestPackage;
    use overload q{""} => sub { ::fail("This should not be called") };

    package Foo;
    ::is_deeply(['TestPackage'], ['TestPackage']);
    ::is_deeply({'TestPackage' => 'TestPackage'}, 
                {'TestPackage' => 'TestPackage'});
    ::is_deeply('TestPackage', 'TestPackage');
}
