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

use Test::More tests => 13;

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

{
    package Foo::five;
    ::use_ok("Symbol", 1.02);
}

{
    package Foo::six;
    ::use_ok("NoExporter", 1.02);
}

{
    package Foo::seven;
    local $SIG{__WARN__} = sub {
        # Old perls will warn on X.YY_ZZ style versions.  Not our problem
        warn @_ unless $_[0] =~ /^Argument "\d+\.\d+_\d+" isn't numeric/;
    };
    ::use_ok("Test::More", 0.47);
}
