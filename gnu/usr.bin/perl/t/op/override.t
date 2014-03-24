#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

plan tests => 28;

#
# This file tries to test builtin override using CORE::GLOBAL
#
my $dirsep = "/";

BEGIN { package Foo; *main::getlogin = sub { "kilroy"; } }

is( getlogin, "kilroy" );

my $t = 42;
BEGIN { *CORE::GLOBAL::time = sub () { $t; } }

is( 45, time + 3 );

#
# require has special behaviour
#
my $r;
BEGIN { *CORE::GLOBAL::require = sub { $r = shift; 1; } }

require Foo;
is( $r, "Foo.pm" );

require Foo::Bar;
is( $r, join($dirsep, "Foo", "Bar.pm") );

require 'Foo';
is( $r, "Foo" );

require 5.006;
is( $r, "5.006" );

require v5.6;
ok( abs($r - 5.006) < 0.001 && $r eq "\x05\x06" );

eval "use Foo";
is( $r, "Foo.pm" );

eval "use Foo::Bar";
is( $r, join($dirsep, "Foo", "Bar.pm") );

{
    my @r;
    local *CORE::GLOBAL::require = sub { push @r, shift; 1; };
    eval "use 5.006";
    like( " @r ", qr " 5\.006 " );
}

{
    local $_ = 'foo.pm';
    require;
    is( $r, 'foo.pm' );
}

{
    BEGIN {
        # Can’t do ‘no warnings’ with CORE::GLOBAL::require overridden. :-)
        CORE::require warnings;
        unimport warnings 'experimental::lexical_topic';
    }
    my $_ = 'bar.pm';
    require;
    is( $r, 'bar.pm' );
}

# localizing *CORE::GLOBAL::foo should revert to finding CORE::foo
{
    local(*CORE::GLOBAL::require);
    $r = '';
    eval "require NoNeXiSt;";
    ok( ! ( $r or $@ !~ /^Can't locate NoNeXiSt/i ) );
}

#
# readline() has special behaviour too
#

$r = 11;
BEGIN { *CORE::GLOBAL::readline = sub (;*) { ++$r }; }
is( <FH>	, 12 );
is( <$fh>	, 13 );
my $pad_fh;
is( <$pad_fh>	, 14 );

# Non-global readline() override
BEGIN { *Rgs::readline = sub (;*) { --$r }; }
{
    package Rgs;
    ::is( <FH>	, 13 );
    ::is( <$fh>	, 12 );
    ::is( <$pad_fh>	, 11 );
}

# Global readpipe() override
BEGIN { *CORE::GLOBAL::readpipe = sub ($) { "$_[0] " . --$r }; }
is( `rm`,	    "rm 10", '``' );
is( qx/cp/,	    "cp 9", 'qx' );

# Non-global readpipe() override
BEGIN { *Rgs::readpipe = sub ($) { ++$r . " $_[0]" }; }
{
    package Rgs;
    ::is( `rm`,		  "10 rm", '``' );
    ::is( qx/cp/,	  "11 cp", 'qx' );
}

# Verify that the parsing of overridden keywords isn't messed up
# by the indirect object notation
{
    local $SIG{__WARN__} = sub {
	::like( $_[0], qr/^ok overriden at/ );
    };
    BEGIN { *OverridenWarn::warn = sub { CORE::warn "@_ overriden"; }; }
    package OverridenWarn;
    sub foo { "ok" }
    warn( OverridenWarn->foo() );
    warn OverridenWarn->foo();
}
BEGIN { *OverridenPop::pop = sub { ::is( $_[0][0], "ok" ) }; }
{
    package OverridenPop;
    sub foo { [ "ok" ] }
    pop( OverridenPop->foo() );
    pop OverridenPop->foo();
}

{
    eval {
        local *CORE::GLOBAL::require = sub {
            CORE::require($_[0]);
        };
        require 5;
        require Text::ParseWords;
    };
    is $@, '';
}
