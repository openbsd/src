#!/usr/bin/perl -w

BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = '../lib';
    }
}

use Test::More tests => 10;

# Using Symbol because it's core and exports lots of stuff.
{
    package Foo::one;
    ::use_ok("Symbol");
    ::ok( defined &gensym,        'use_ok() no args exports defaults' );
}

{
    package Foo::two;
    ::use_ok("Symbol", qw(qualify));
    ::ok( !defined &gensym,       '  one arg, defaults overriden' );
    ::ok( defined &qualify,       '  right function exported' );
}

{
    package Foo::three;
    ::use_ok("Symbol", qw(gensym ungensym));
    ::ok( defined &gensym && defined &ungensym,   '  multiple args' );
}

{
    package Foo::four;
    my $warn; local $SIG{__WARN__} = sub { $warn .= shift; };
    ::use_ok("constant", qw(foo bar));
    ::ok( defined &foo, 'constant' );
    ::is( $warn, undef, 'no warning');
}
