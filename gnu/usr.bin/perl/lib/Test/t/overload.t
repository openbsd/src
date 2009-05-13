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
use Test::More tests => 15;


package Overloaded;

use overload
  q{eq}    => sub { $_[0]->{string} },
  q{==}    => sub { $_[0]->{num} },
  q{""}    => sub { $_[0]->{stringfy}++; $_[0]->{string} },
  q{0+}    => sub { $_[0]->{numify}++;   $_[0]->{num}    }
;

sub new {
    my $class = shift;
    bless {
        string  => shift,
        num     => shift,
        stringify       => 0,
        numify          => 0,
    }, $class;
}


package main;

local $SIG{__DIE__} = sub {
    my($call_file, $call_line) = (caller)[1,2];
    fail("SIGDIE accidentally called");
    diag("From $call_file at $call_line");
};

my $obj = Overloaded->new('foo', 42);
isa_ok $obj, 'Overloaded';

is $obj, 'foo',            'is() with string overloading';
cmp_ok $obj, 'eq', 'foo',  'cmp_ok() ...';
is $obj->{stringify}, 0, 'cmp_ok() eq does not stringify';
cmp_ok $obj, '==', 42,     'cmp_ok() with number overloading';
is $obj->{numify}, 0,    'cmp_ok() == does not numify';

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
