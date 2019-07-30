#!./perl -w

# Some miscellaneous checks for the list assignment operator, OP_AASSIGN.
#
# This file was only added in 2015; before then, such tests were
# typically in various other random places like op/array.t. This test file
# doesn't therefore attempt to be comprehensive; it merely provides a
# central place to new put additional tests, especially those related to
# the trickiness of commonality, e.g. ($a,$b) = ($b,$a).
#
# In particular, it's testing the flags
#    OPpASSIGN_COMMON_SCALAR
#    OPpASSIGN_COMMON_RC1
#    OPpASSIGN_COMMON_AGG

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

use warnings;
use strict;

# general purpose package vars

our $pkg_scalar;
our @pkg_array;
our %pkg_hash;

sub f_ret_14 { return 1..4 }

# stringify a hash ref

sub sh {
    my $rh = $_[0];
    join ',', map "$_:$rh->{$_}", sort keys %$rh;
}


# where the RHS has surplus elements

{
    my ($a,$b);
    ($a,$b) = f_ret_14();
    is("$a:$b", "1:2", "surplus");
}

# common with slices

{
    my @a = (1,2);
    @a[0,1] = @a[1,0];
    is("$a[0]:$a[1]", "2:1", "lex array slice");
}

# package alias

{
    my ($a, $b) = 1..2;
    for $pkg_scalar ($a) {
        ($pkg_scalar, $b) = (3, $a);
        is($pkg_scalar, 3, "package alias pkg");
        is("$a:$b", "3:1", "package alias a:b");
    }
}

# my array/hash populated via closure

{
    my $ra = f1();
    my ($x, @a) = @$ra;
    sub f1 { $x = 1; @a = 2..4; \@a }
    is($x,       2, "my: array closure x");
    is("@a", "3 4", "my: array closure a");

    my $rh = f2();
    my ($k, $v, %h) = (d => 4, %$rh, e => 6);
    sub f2 { $k = 'a'; $v = 1; %h = qw(b 2 c 3); \%h }
    is("$k:$v", "d:4", "my: hash closure k:v");
    is(sh(\%h), "b:2,c:3,e:6", "my: hash closure h");
}


# various shared element scenarios within a my (...)

{
    my ($x,$y) = f3(); # $x and $y on both sides
    sub f3 : lvalue { ($x,$y) = (1,2); $y, $x }
    is ("$x:$y", "2:1", "my: scalar and lvalue sub");
}

{
    my $ra = f4();
    my @a = @$ra;  # elements of @a on both sides
    sub f4 { @a = 1..4; \@a }
    is("@a", "1 2 3 4", "my: array and elements");
}

{
    my $rh = f5();
    my %h = %$rh;  # elements of %h on both sides
    sub f5 { %h = qw(a 1 b 2 c 3); \%h }
    is(sh(\%h), "a:1,b:2,c:3", "my: hash and elements");
}

{
    f6();
    our $xalias6;
    my ($x, $y) = (2, $xalias6);
    sub f6 { $x = 1; *xalias6 = \$x; }
    is ("$x:$y", "2:1", "my: pkg var aliased to lexical");
}


{
    my @a;
    f7();
    my ($x,$y) = @a;
    is ("$x:$y", "2:1", "my: lex array elements aliased");

    sub f7 {
        ($x, $y) = (1,2);
        use feature 'refaliasing';
        no warnings 'experimental';
        \($a[0], $a[1]) = \($y,$x);
    }
}

{
    @pkg_array = ();
    f8();
    my ($x,$y) = @pkg_array;
    is ("$x:$y", "2:1", "my: pkg array elements aliased");

    sub f8 {
        ($x, $y) = (1,2);
        use feature 'refaliasing';
        no warnings 'experimental';
        \($pkg_array[0], $pkg_array[1]) = \($y,$x);
    }
}

{
    f9();
    my ($x,$y) = f9();
    is ("$x:$y", "2:1", "my: pkg scalar alias");

    our $xalias9;
    sub f9 : lvalue {
        ($x, $y) = (1,2);
        *xalias9 = \$x;
        $y, $xalias9;
    }
}

{
    use feature 'refaliasing';
    no warnings 'experimental';

    f10();
    our $pkg10;
    \(my $lex) = \$pkg10;
    my @a = ($lex,3); # equivalent to ($a[0],3)
    is("@a", "1 3", "my: lex alias of array alement");

    sub f10 {
        @a = (1,2);
        \$pkg10 = \$a[0];
    }

}

{
    use feature 'refaliasing';
    no warnings 'experimental';

    f11();
    my @b;
    my @a = (@b);
    is("@a", "2 1", "my: lex alias of array alements");

    sub f11 {
        @a = (1,2);
        \$b[0] = \$a[1];
        \$b[1] = \$a[0];
    }
}

# package aliasing

{
    my ($x, $y) = (1,2);

    for $pkg_scalar ($x) {
        ($pkg_scalar, $y) = (3, $x);
        is("$pkg_scalar,$y", "3,1", "package scalar aliased");
    }
}

# lvalue subs on LHS

{
    my @a;
    sub f12 : lvalue { @a }
    (f12()) = 1..3;
    is("@a", "1 2 3", "lvalue sub on RHS returns array");
}

{
    my ($x,$y);
    sub f13 : lvalue { $x,$y }
    (f13()) = 1..3;
    is("$x:$y", "1:2", "lvalue sub on RHS returns scalars");
}


# package shared scalar vars

{
    our $pkg14a = 1;
    our $pkg14b = 2;
    ($pkg14a,$pkg14b) = ($pkg14b,$pkg14a);
    is("$pkg14a:$pkg14b", "2:1", "shared package scalars");
}

# lexical shared scalar vars

{
    my $a = 1;
    my $b = 2;
    ($a,$b) = ($b,$a);
    is("$a:$b", "2:1", "shared lexical scalars");
}


# lexical nested array elem swap

{
    my @a;
    $a[0][0] = 1;
    $a[0][1] = 2;
    ($a[0][0],$a[0][1]) =  ($a[0][1],$a[0][0]);
    is("$a[0][0]:$a[0][1]", "2:1", "lexical nested array elem swap");
}

# package nested array elem swap

{
    our @a15;
    $a15[0][0] = 1;
    $a15[0][1] = 2;
    ($a15[0][0],$a15[0][1]) =  ($a15[0][1],$a15[0][0]);
    is("$a15[0][0]:$a15[0][1]", "2:1", "package nested array elem swap");
}

# surplus RHS junk
#
{
    our ($a16, $b16);
    ($a16, undef, $b16) = 1..30;
    is("$a16:$b16", "1:3", "surplus RHS junk");
}

# my ($scalar,....) = @_
#
# technically this is an unsafe usage commonality-wise, but
# a) you have to try really hard to break it, as this test shows;
# b) it's such an important usage that for performance reasons we
#    mark it as safe even though it isn't really. Hence it's a TODO.

SKIP: {
    use Config;
    # debugging builds will detect this failure and panic
    skip "DEBUGGING build" if $::Config{ccflags} =~ /DEBUGGING/
                              or $^O eq 'VMS' && $::Config{usedebugging_perl} eq 'Y';
    local $::TODO = 'cheat and optimise my (....) = @_';
    local @_ = 1..3;
    &f17;
    my ($a, @b) = @_;
    is("($a)(@b)", "(3)(2 1)", 'my (....) = @_');

    sub f17 {
        use feature 'refaliasing';
        no warnings 'experimental';
        ($a, @b) = @_;
        \($_[2], $_[1], $_[0]) = \($a, $b[0], $b[1]);
    }
}

# single scalar on RHS that's in an aggregate on LHS

{
    my @a = 1..3;
    for my $x ($a[0]) {
        (@a) = ($x);
        is ("(@a)", "(1)", 'single scalar on RHS, agg');
    }
}

# TEMP buffer stealing.
# In something like
#    (...) = (f())[0,0]
# the same TEMP RHS element may be used more than once, so when copying
# it, we mustn't steal its buffer.

{
    # a string long enough for COW and buffer stealing to be enabled
    my $long = 'def' . ('x' x 2000);

    # a sub that is intended to return a TEMP string that isn't COW
    # the concat returns a non-COW PADTMP; pp_leavesub sees a long
    # stealable string, so creates a TEMP with the stolen buffer from the
    # PADTMP - hence it returns a non-COW string
    sub f18 {
        my $x = "abc";
        $x . $long;
    }

    my @a;

    # with @a initially empty,the code path creates a new copy of each
    # RHS element to store in the array

    @a = (f18())[0,0];
    is (substr($a[0], 0, 7), "abcdefx", 'NOSTEAL empty $a[0]');
    is (substr($a[1], 0, 7), "abcdefx", 'NOSTEAL empty $a[1]');

    # with @a initially non-empty, it takes a different code path that
    # makes a mortal copy of each RHS element
    @a = 1..3;
    @a = (f18())[0,0];
    is (substr($a[0], 0, 7), "abcdefx", 'NOSTEAL non-empty $a[0]');
    is (substr($a[1], 0, 7), "abcdefx", 'NOSTEAL non-empty $a[1]');

}

{
    my $x = 1;
    my $y = 2;
    ($x,$y) = (undef, $x);
    is($x, undef, 'single scalar on RHS, but two on LHS: x');
    is($y, 1, 'single scalar on RHS, but two on LHS: y');
}

{ # magic handling, see #126633
    use v5.22;
    my $set;
    package ArrayProxy {
        sub TIEARRAY { bless [ $_[1] ] }
        sub STORE { $_[0][0]->[$_[1]] = $_[2]; $set = 1 }
        sub FETCH { $_[0][0]->[$_[1]] }
        sub CLEAR { @{$_[0][0]} = () }
        sub EXTEND {}
    };
    my @base = ( "a", "b" );
    my @real = @base;
    my @proxy;
    my $temp;
    tie @proxy, "ArrayProxy", \@real;
    @proxy[0, 1] = @real[1, 0];
    is($real[0], "b", "tied left first");
    is($real[1], "a", "tied left second");
    @real = @base;
    @real[0, 1] = @proxy[1, 0];
    is($real[0], "b", "tied right first");
    is($real[1], "a", "tied right second");
    @real = @base;
    @proxy[0, 1] = @proxy[1, 0];
    is($real[0], "b", "tied both first");
    is($real[1], "a", "tied both second");
    @real = @base;
    ($temp, @real) = @proxy[1, 0];
    is($real[0], "a", "scalar/array tied right");
    @real = @base;
    ($temp, @proxy) = @real[1, 0];
    is($real[0], "a", "scalar/array tied left");
    @real = @base;
    ($temp, @proxy) = @proxy[1, 0];
    is($real[0], "a", "scalar/array tied both");
    $set = 0;
    my $orig;
    ($proxy[0], $orig) = (1, $set);
    is($orig, 0, 'previous value of $set');

    # from cpan #110278
  SKIP: {
      skip "no List::Util::min on miniperl", 2, if is_miniperl;
      require List::Util;
      my $x = 1;
      my $y = 2;
      ( $x, $y ) = ( List::Util::min($y), List::Util::min($x) );
      is($x, 2, "check swap for \$x");
      is($y, 1, "check swap for \$y");
    }
}

done_testing();
