#!./perl
# Tests for caller()

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
    plan( tests => 31 );
}

my @c;

print "# Tests with caller(0)\n";

@c = caller(0);
ok( (!@c), "caller(0) in main program" );

eval { @c = caller(0) };
is( $c[3], "(eval)", "subroutine name in an eval {}" );
ok( !$c[4], "hasargs false in an eval {}" );

eval q{ @c = (Caller(0))[3] };
is( $c[3], "(eval)", "subroutine name in an eval ''" );
ok( !$c[4], "hasargs false in an eval ''" );

sub { @c = caller(0) } -> ();
is( $c[3], "main::__ANON__", "anonymous subroutine name" );
ok( $c[4], "hasargs true with anon sub" );

# Bug 20020517.003, used to dump core
sub foo { @c = caller(0) }
my $fooref = delete $::{foo};
$fooref -> ();
is( $c[3], "(unknown)", "unknown subroutine name" );
ok( $c[4], "hasargs true with unknown sub" );

print "# Tests with caller(1)\n";

sub f { @c = caller(1) }

sub callf { f(); }
callf();
is( $c[3], "main::callf", "subroutine name" );
ok( $c[4], "hasargs true with callf()" );
&callf;
ok( !$c[4], "hasargs false with &callf" );

eval { f() };
is( $c[3], "(eval)", "subroutine name in an eval {}" );
ok( !$c[4], "hasargs false in an eval {}" );

eval q{ f() };
is( $c[3], "(eval)", "subroutine name in an eval ''" );
ok( !$c[4], "hasargs false in an eval ''" );

sub { f() } -> ();
is( $c[3], "main::__ANON__", "anonymous subroutine name" );
ok( $c[4], "hasargs true with anon sub" );

sub foo2 { f() }
my $fooref2 = delete $::{foo2};
$fooref2 -> ();
is( $c[3], "(unknown)", "unknown subroutine name" );
ok( $c[4], "hasargs true with unknown sub" );

# See if caller() returns the correct warning mask

sub testwarn {
    my $w = shift;
    is( (caller(0))[9], $w, "warnings");
}

# NB : extend the warning mask values below when new warnings are added
{
    no warnings;
    BEGIN { is( ${^WARNING_BITS}, "\0" x 12, 'warning bits' ) }
    testwarn("\0" x 12);
    use warnings;
    BEGIN { is( ${^WARNING_BITS}, "UUUUUUUUUUU\25", 'warning bits' ) }
    BEGIN { testwarn("UUUUUUUUUUU\25"); }
    # run-time :
    # the warning mask has been extended by warnings::register
    testwarn("UUUUUUUUUUUU");
    use warnings::register;
    BEGIN { is( ${^WARNING_BITS}, "UUUUUUUUUUUU", 'warning bits' ) }
    testwarn("UUUUUUUUUUUU");
}


# The next two cases test for a bug where caller ignored evals if
# the DB::sub glob existed but &DB::sub did not (for example, if 
# $^P had been set but no debugger has been loaded).  The tests
# thus assume that there is no &DB::sub: if there is one, they 
# should both pass  no matter whether or not this bug has been
# fixed.

my $debugger_test =  q<
    my @stackinfo = caller(0);
    return scalar @stackinfo;
>;

sub pb { return (caller(0))[3] }

my $i = eval $debugger_test;
is( $i, 10, "do not skip over eval (and caller returns 10 elements)" );

is( eval 'pb()', 'main::pb', "actually return the right function name" );

my $saved_perldb = $^P;
$^P = 16;
$^P = $saved_perldb;

$i = eval $debugger_test;
is( $i, 10, 'do not skip over eval even if $^P had been on at some point' );
is( eval 'pb()', 'main::pb', 'actually return the right function name even if $^P had been on at some point' );

