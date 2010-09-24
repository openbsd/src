#!./perl -w
# tests state variables

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

use strict;
use feature ":5.10";

plan tests => 130;

ok( ! defined state $uninit, q(state vars are undef by default) );

# basic functionality

sub stateful {
    state $x;
    state $y = 1;
    my $z = 2;
    state ($t) //= 3;
    return ($x++, $y++, $z++, $t++);
}

my ($x, $y, $z, $t) = stateful();
is( $x, 0, 'uninitialized state var' );
is( $y, 1, 'initialized state var' );
is( $z, 2, 'lexical' );
is( $t, 3, 'initialized state var, list syntax' );

($x, $y, $z, $t) = stateful();
is( $x, 1, 'incremented state var' );
is( $y, 2, 'incremented state var' );
is( $z, 2, 'reinitialized lexical' );
is( $t, 4, 'incremented state var, list syntax' );

($x, $y, $z, $t) = stateful();
is( $x, 2, 'incremented state var' );
is( $y, 3, 'incremented state var' );
is( $z, 2, 'reinitialized lexical' );
is( $t, 5, 'incremented state var, list syntax' );

# in a nested block

sub nesting {
    state $foo = 10;
    my $t;
    { state $bar = 12; $t = ++$bar }
    ++$foo;
    return ($foo, $t);
}

($x, $y) = nesting();
is( $x, 11, 'outer state var' );
is( $y, 13, 'inner state var' );

($x, $y) = nesting();
is( $x, 12, 'outer state var' );
is( $y, 14, 'inner state var' );

# in a closure

sub generator {
    my $outer;
    # we use $outer to generate a closure
    sub { ++$outer; ++state $x }
}

my $f1 = generator();
is( $f1->(), 1, 'generator 1' );
is( $f1->(), 2, 'generator 1' );
my $f2 = generator();
is( $f2->(), 1, 'generator 2' );
is( $f1->(), 3, 'generator 1 again' );
is( $f2->(), 2, 'generator 2 once more' );

# with ties
{
    package countfetches;
    our $fetchcount = 0;
    sub TIESCALAR {bless {}};
    sub FETCH { ++$fetchcount; 18 };
    tie my $y, "countfetches";
    sub foo { state $x = $y; $x++ }
    ::is( foo(), 18, "initialisation with tied variable" );
    ::is( foo(), 19, "increments correctly" );
    ::is( foo(), 20, "increments correctly, twice" );
    ::is( $fetchcount, 1, "fetch only called once" );
}

# state variables are shared among closures

sub gen_cashier {
    my $amount = shift;
    state $cash_in_store = 0;
    return {
	add => sub { $cash_in_store += $amount },
	del => sub { $cash_in_store -= $amount },
	bal => sub { $cash_in_store },
    };
}

gen_cashier(59)->{add}->();
gen_cashier(17)->{del}->();
is( gen_cashier()->{bal}->(), 42, '$42 in my drawer' );

# stateless assignment to a state variable

sub stateless {
    state $reinitme = 42;
    ++$reinitme;
}
is( stateless(), 43, 'stateless function, first time' );
is( stateless(), 44, 'stateless function, second time' );

# array state vars

sub stateful_array {
    state @x;
    push @x, 'x';
    return $#x;
}

my $xsize = stateful_array();
is( $xsize, 0, 'uninitialized state array' );

$xsize = stateful_array();
is( $xsize, 1, 'uninitialized state array after one iteration' );

# hash state vars

sub stateful_hash {
    state %hx;
    return $hx{foo}++;
}

my $xhval = stateful_hash();
is( $xhval, 0, 'uninitialized state hash' );

$xhval = stateful_hash();
is( $xhval, 1, 'uninitialized state hash after one iteration' );

# Recursion

sub noseworth {
    my $level = shift;
    state $recursed_state = 123;
    is($recursed_state, 123, "state kept through recursion ($level)");
    noseworth($level - 1) if $level;
}
noseworth(2);

# Assignment return value

sub pugnax { my $x = state $y = 42; $y++; $x; }

is( pugnax(), 42, 'scalar state assignment return value' );
is( pugnax(), 43, 'scalar state assignment return value' );


#
# Test various blocks.
#
foreach my $x (1 .. 3) {
    state $y = $x;
    is ($y, 1, "foreach $x");
}

for (my $x = 1; $x < 4; $x ++) {
    state $y = $x;
    is ($y, 1, "for $x");
}

while ($x < 4) {
    state $y = $x;
    is ($y, 1, "while $x");
    $x ++;
}

$x = 1;
until ($x >= 4) {
    state $y = $x;
    is ($y, 1, "until $x");
    $x ++;
}

$x = 0;
$y = 0;
{
    state $z = $x;
    $z ++;
    $y ++;
    is ($z, $y, "bare block $y");
    redo if $y < 3
}


#
# Check state $_
#
my @stones = qw [fred wilma barny betty];
my $first  = $stones [0];
my $First  = ucfirst $first;
$_ = "bambam";
foreach my $flint (@stones) {
    state $_ = $flint;
    is $_, $first, 'state $_';
    ok /$first/, '/.../ binds to $_';
    is ucfirst, $First, '$_ default argument';
}
is $_, "bambam", '$_ is still there';

#
# Goto.
#
my @simpsons = qw [Homer Marge Bart Lisa Maggie];
again:
    my $next = shift @simpsons;
    state $simpson = $next;
    is $simpson, 'Homer', 'goto 1';
    goto again if @simpsons;

my $vi;
{
    goto Elvis unless $vi;
           state $calvin = ++ $vi;
    Elvis: state $vile   = ++ $vi;
    redo unless defined $calvin;
    is $calvin, 2, "goto 2";
    is $vile,   1, "goto 3";
    is $vi,     2, "goto 4";
}
my @presidents = qw [Taylor Garfield Ford Arthur Monroe];
sub president {
    my $next = shift @presidents;
    state $president = $next;
    goto  &president if @presidents;
    $president;
}
my $president_answer = $presidents [0];
is president, $president_answer, '&goto';

my @flowers = qw [Bluebonnet Goldenrod Hawthorn Peony];
foreach my $f (@flowers) {
    goto state $flower = $f;
    ok 0, 'computed goto 0'; next;
    Bluebonnet: ok 1, 'computed goto 1'; next;
    Goldenrod:  ok 0, 'computed goto 2'; next;
    Hawthorn:   ok 0, 'computed goto 3'; next;
    Peony:      ok 0, 'computed goto 4'; next;
    ok 0, 'computed goto 5'; next;
}

#
# map/grep
#
my @apollo  = qw [Eagle Antares Odyssey Aquarius];
my @result1 = map  {state $x = $_;}     @apollo;
my @result2 = grep {state $x = /Eagle/} @apollo;
{
    local $" = "";
    is "@result1", $apollo [0] x @apollo, "map";
    is "@result2", "@apollo", "grep";
}

#
# Reference to state variable.
#
sub reference {\state $x}
my $ref1 = reference;
my $ref2 = reference;
is $ref1, $ref2, "Reference to state variable";

#
# Pre/post increment.
#
foreach my $x (1 .. 3) {
    ++ state $y;
    state $z ++;
    is $y, $x, "state pre increment";
    is $z, $x, "state post increment";
}


#
# Substr
#
my $tintin = "Tin-Tin";
my @thunderbirds  = qw [Scott Virgel Alan Gordon John];
my @thunderbirds2 = qw [xcott xxott xxxtt xxxxt xxxxx];
foreach my $x (0 .. 4) {
    state $c = \substr $tintin, $x, 1;
    my $d = \substr ((state $tb = $thunderbirds [$x]), $x, 1);
    $$c = "x";
    $$d = "x";
    is $tintin, "xin-Tin", "substr";
    is $tb, $thunderbirds2 [$x], "substr";
}


#
# Use with given.
#
my @spam = qw [spam ham bacon beans];
foreach my $spam (@spam) {
    given (state $spam = $spam) {
        when ($spam [0]) {ok 1, "given"}
        default          {ok 0, "given"}
    }
}

#
# Redefine.
#
{
    state $x = "one";
    no warnings;
    state $x = "two";
    is $x, "two", "masked"
}

# normally closureless anon subs share a CV and pad. If the anon sub has a
# state var, this would mean that it is shared. Check that this doesn't
# happen

{
    my @f;
    push @f, sub { state $x; ++$x } for 1..2;
    $f[0]->() for 1..10;
    is $f[0]->(), 11;
    is $f[1]->(), 1;
}

# each copy of an anon sub should get its own 'once block'

{
    my $x; # used to force a closure
    my @f;
    push @f, sub { $x=0; state $s = $_[0]; $s } for 1..2;
    is $f[0]->(1), 1;
    is $f[0]->(2), 1;
    is $f[1]->(3), 3;
    is $f[1]->(4), 3;
}




foreach my $forbidden (<DATA>) {
    chomp $forbidden;
    no strict 'vars';
    eval $forbidden;
    like $@, qr/Initialization of state variables in list context currently forbidden/, "Currently forbidden: $forbidden";
}

# [perl #49522] state variable not available

{
    my @warnings;
    local $SIG{__WARN__} = sub { push @warnings, $_[0] };

    eval q{
	use warnings;

	sub f_49522 {
	    state $s = 88;
	    sub g_49522 { $s }
	    sub { $s };
	}

	sub h_49522 {
	    state $t = 99;
	    sub i_49522 {
		sub { $t };
	    }
	}
    };
    is $@, '', "eval f_49522";
    # shouldn't be any 'not available' or 'not stay shared' warnings
    ok !@warnings, "suppress warnings part 1 [@warnings]";

    @warnings = ();
    my $f = f_49522();
    is $f->(), 88, "state var closure 1";
    is g_49522(), 88, "state var closure 2";
    ok !@warnings, "suppress warnings part 2 [@warnings]";


    @warnings = ();
    $f = i_49522();
    h_49522(); # initialise $t
    is $f->(), 99, "state var closure 3";
    ok !@warnings, "suppress warnings part 3 [@warnings]";


}


__DATA__
state ($a) = 1;
(state $a) = 1;
state @a = 1;
state (@a) = 1;
(state @a) = 1;
state %a = ();
state (%a) = ();
(state %a) = ();
state ($a, $b) = ();
state ($a, @b) = ();
(state $a, state $b) = ();
(state $a, $b) = ();
(state $a, my $b) = ();
(state $a, state @b) = ();
(state $a, local @b) = ();
(state $a, undef, state $b) = ();
state ($a, undef, $b) = ();
