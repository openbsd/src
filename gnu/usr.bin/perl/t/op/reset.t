#!./perl -w

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}
use strict;

plan tests => 30;

package aiieee;

sub zlopp {
    (shift =~ m?zlopp?) ? 1 : 0;
}

sub reset_zlopp {
    reset;
}

package CLINK;

sub ZZIP {
    shift =~ m?ZZIP? ? 1 : 0;
}

sub reset_ZZIP {
    reset;
}

package main;

is(aiieee::zlopp(""), 0, "mismatch doesn't match");
is(aiieee::zlopp("zlopp"), 1, "match matches first time");
is(aiieee::zlopp(""), 0, "mismatch doesn't match");
is(aiieee::zlopp("zlopp"), 0, "match doesn't match second time");
aiieee::reset_zlopp();
is(aiieee::zlopp("zlopp"), 1, "match matches after reset");
is(aiieee::zlopp(""), 0, "mismatch doesn't match");

aiieee::reset_zlopp();

is(aiieee::zlopp(""), 0, "mismatch doesn't match");
is(aiieee::zlopp("zlopp"), 1, "match matches first time");
is(CLINK::ZZIP(""), 0, "mismatch doesn't match");
is(CLINK::ZZIP("ZZIP"), 1, "match matches first time");
is(CLINK::ZZIP(""), 0, "mismatch doesn't match");
is(CLINK::ZZIP("ZZIP"), 0, "match doesn't match second time");
is(aiieee::zlopp(""), 0, "mismatch doesn't match");
is(aiieee::zlopp("zlopp"), 0, "match doesn't match second time");

aiieee::reset_zlopp();
is(aiieee::zlopp("zlopp"), 1, "match matches after reset");
is(aiieee::zlopp(""), 0, "mismatch doesn't match");

is(CLINK::ZZIP(""), 0, "mismatch doesn't match");
is(CLINK::ZZIP("ZZIP"), 0, "match doesn't match third time");

CLINK::reset_ZZIP();
is(CLINK::ZZIP("ZZIP"), 1, "match matches after reset");
is(CLINK::ZZIP(""), 0, "mismatch doesn't match");

sub match_foo{
    "foo" =~ m?foo?;
}
match_foo();
reset "";
ok !match_foo(), 'reset "" leaves patterns alone [perl #97958]';

$scratch::a = "foo";
$scratch::a2 = "bar";
$scratch::b   = "baz";
package scratch { reset "a" }
is join("-", $scratch::a//'u', $scratch::a2//'u', $scratch::b//'u'),
   "u-u-baz",
   'reset "char"';

$scratch::a = "foo";
$scratch::a2 = "bar";
$scratch::b   = "baz";
$scratch::c    = "sea";
package scratch { reset "bc" }
is join("-", $scratch::a//'u', $scratch::a2//'u', $scratch::b//'u',
             $scratch::c//'u'),
   "foo-bar-u-u",
   'reset "chars"';

$scratch::a = "foo";
$scratch::a2 = "bar";
$scratch::b   = "baz";
$scratch::c    = "sea";
package scratch { reset "a-b" }
is join("-", $scratch::a//'u', $scratch::a2//'u', $scratch::b//'u',
             $scratch::c//'u'),
   "u-u-u-sea",
   'reset "range"';

{ no strict; ${"scratch::\0foo"} = "bar" }
$scratch::a = "foo";
package scratch { reset "\0a" }
is join("-", $scratch::a//'u', do { no strict; ${"scratch::\0foo"} }//'u'),
   "u-u",
   'reset "\0char"';

# This used to crash under threaded builds, because pmops were remembering
# their stashes by name, rather than by pointer.
fresh_perl_is( # it crashes more reliably with a smaller script
  'package bar;
   sub foo {
     m??;
     BEGIN { *baz:: = *bar::; *bar:: = *foo:: }
     # The name "bar" no langer refers to the same package
   }
   undef &foo; # so freeing the op does not remove it from the stash’s list
   $_ = "";
   push @_, ($_) x 10000;  # and its memory is scribbled over
   reset;  # so reset on the original package tries to reset an invalid op
   print "ok\n";',
  "ok\n", {},
  "no crash if package is effectively renamed before op is freed");


undef $/;
my $prog = <DATA>;

SKIP:
{
    eval {require threads; 1} or
	skip "No threads", 4;
    foreach my $eight ('/', '?') {
	foreach my $nine ('/', '?') {
	    my $copy = $prog;
	    $copy =~ s/8/$eight/gm;
	    $copy =~ s/9/$nine/gm;
	    fresh_perl_is($copy, "pass", "",
			  "first pattern $eight$eight, second $nine$nine");
	}
    }
}

__DATA__
#!perl
use warnings;
use strict;

# Note that there are no digits in this program, other than the placeholders
sub a {
m8one8;
}
sub b {
m9two9;
}

use threads;
use threads::shared;

sub wipe {
    eval 'no warnings; sub b {}; 1' or die $@;
}

sub lock_then_wipe {
    my $l_r = shift;
    lock $$l_r;
    cond_wait($$l_r) until $$l_r eq "B";
    wipe;
    $$l_r = "C";
    cond_signal $$l_r;
}

my $lock : shared = "A";
my $r = \$lock;

my $t;
{
    lock $$r;
    $t = threads->new(\&lock_then_wipe, $r);
    wipe;
    $lock = "B";
    cond_signal $lock;
}

{
    lock $lock;
    cond_wait($lock) until $lock eq "C";
    reset;
}

$t->join;
print "pass\n";
