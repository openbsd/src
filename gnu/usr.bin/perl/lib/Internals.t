#!/usr/bin/perl -Tw

BEGIN {
    if( $ENV{PERL_CORE} ) {
        @INC = '../lib';
        chdir 't';
    }
}

use Test::More tests => 33;

my $foo;
my @foo;
my %foo;

ok( !Internals::SvREADONLY $foo );
ok(  Internals::SvREADONLY $foo, 1 );
ok(  Internals::SvREADONLY $foo );
ok( !Internals::SvREADONLY $foo, 0 );
ok( !Internals::SvREADONLY $foo );

ok( !Internals::SvREADONLY @foo );
ok(  Internals::SvREADONLY @foo, 1 );
ok(  Internals::SvREADONLY @foo );
ok( !Internals::SvREADONLY @foo, 0 );
ok( !Internals::SvREADONLY @foo );

ok( !Internals::SvREADONLY $foo[2] );
ok(  Internals::SvREADONLY $foo[2], 1 );
ok(  Internals::SvREADONLY $foo[2] );
ok( !Internals::SvREADONLY $foo[2], 0 );
ok( !Internals::SvREADONLY $foo[2] );

ok( !Internals::SvREADONLY %foo );
ok(  Internals::SvREADONLY %foo, 1 );
ok(  Internals::SvREADONLY %foo );
ok( !Internals::SvREADONLY %foo, 0 );
ok( !Internals::SvREADONLY %foo );

ok( !Internals::SvREADONLY $foo{foo} );
ok(  Internals::SvREADONLY $foo{foo}, 1 );
ok(  Internals::SvREADONLY $foo{foo} );
ok( !Internals::SvREADONLY $foo{foo}, 0 );
ok( !Internals::SvREADONLY $foo{foo} );

is(  Internals::SvREFCNT($foo), 1 );
{
    my $bar = \$foo;
    is(  Internals::SvREFCNT($foo), 2 );
    is(  Internals::SvREFCNT($bar), 1 );
}
is(  Internals::SvREFCNT($foo), 1 );

is(  Internals::SvREFCNT(@foo), 1 );
is(  Internals::SvREFCNT($foo[2]), 1 );
is(  Internals::SvREFCNT(%foo), 1 );
is(  Internals::SvREFCNT($foo{foo}), 1 );

