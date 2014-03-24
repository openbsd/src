#!./perl

# "This IS structured code.  It's just randomly structured."

BEGIN {
    chdir 't' if -d 't';
    @INC = qw(. ../lib);
    require "test.pl";
}

use warnings;
use strict;
plan tests => 89;
our $TODO;

my $deprecated = 0;
local $SIG{__WARN__} = sub { if ($_[0] =~ m/jump into a construct/) { $deprecated++; } else { warn $_[0] } };

our $foo;
while ($?) {
    $foo = 1;
  label1:
    is($deprecated, 1, "following label1");
    $deprecated = 0;
    $foo = 2;
    goto label2;
} continue {
    $foo = 0;
    goto label4;
  label3:
    is($deprecated, 1, "following label3");
    $deprecated = 0;
    $foo = 4;
    goto label4;
}
is($deprecated, 0, "after 'while' loop");
goto label1;

$foo = 3;

label2:
is($foo, 2, 'escape while loop');
is($deprecated, 0, "following label2");
goto label3;

label4:
is($foo, 4, 'second escape while loop');

my $r = run_perl(prog => 'goto foo;', stderr => 1);
like($r, qr/label/, 'cant find label');

my $ok = 0;
sub foo {
    goto bar;
    return;
bar:
    $ok = 1;
}

&foo;
ok($ok, 'goto in sub');

sub bar {
    my $x = 'bypass';
    eval "goto $x";
}

&bar;
exit;

FINALE:
is(curr_test(), 20, 'FINALE');

# does goto LABEL handle block contexts correctly?
# note that this scope-hopping differs from last & next,
# which always go up-scope strictly.
my $count = 0;
my $cond = 1;
for (1) {
    if ($cond == 1) {
	$cond = 0;
	goto OTHER;
    }
    elsif ($cond == 0) {
      OTHER:
	$cond = 2;
	is($count, 0, 'OTHER');
	$count++;
	goto THIRD;
    }
    else {
      THIRD:
	is($count, 1, 'THIRD');
	$count++;
    }
}
is($count, 2, 'end of loop');

# Does goto work correctly within a for(;;) loop?
#  (BUG ID 20010309.004)

for(my $i=0;!$i++;) {
  my $x=1;
  goto label;
  label: is($x, 1, 'goto inside a for(;;) loop body from inside the body');
}

# Does goto work correctly going *to* a for(;;) loop?
#  (make sure it doesn't skip the initializer)

my ($z, $y) = (0);
FORL1: for ($y=1; $z;) {
    ok($y, 'goto a for(;;) loop, from outside (does initializer)');
    goto TEST19}
($y,$z) = (0, 1);
goto FORL1;

# Even from within the loop?
TEST19: $z = 0;
FORL2: for($y=1; 1;) {
  if ($z) {
    ok($y, 'goto a for(;;) loop, from inside (does initializer)');
    last;
  }
  ($y, $z) = (0, 1);
  goto FORL2;
}

# Does goto work correctly within a try block?
#  (BUG ID 20000313.004) - [perl #2359]
$ok = 0;
eval {
  my $variable = 1;
  goto LABEL20;
  LABEL20: $ok = 1 if $variable;
};
ok($ok, 'works correctly within a try block');
is($@, "", '...and $@ not set');

# And within an eval-string?
$ok = 0;
eval q{
  my $variable = 1;
  goto LABEL21;
  LABEL21: $ok = 1 if $variable;
};
ok($ok, 'works correctly within an eval string');
is($@, "", '...and $@ still not set');


# Test that goto works in nested eval-string
$ok = 0;
{eval q{
  eval q{
    goto LABEL22;
  };
  $ok = 0;
  last;

  LABEL22: $ok = 1;
};
$ok = 0 if $@;
}
ok($ok, 'works correctly in a nested eval string');

{
    my $false = 0;
    my $count;

    $ok = 0;
    { goto A; A: $ok = 1 } continue { }
    ok($ok, '#20357 goto inside /{ } continue { }/ loop');

    $ok = 0;
    { do { goto A; A: $ok = 1 } while $false }
    ok($ok, '#20154 goto inside /do { } while ()/ loop');
    $ok = 0;
    foreach(1) { goto A; A: $ok = 1 } continue { };
    ok($ok, 'goto inside /foreach () { } continue { }/ loop');

    $ok = 0;
    sub a {
	A: { if ($false) { redo A; B: $ok = 1; redo A; } }
	goto B unless $count++;
    }
    is($deprecated, 0, "before calling sub a()");
    a();
    ok($ok, '#19061 loop label wiped away by goto');
    is($deprecated, 1, "after calling sub a()");
    $deprecated = 0;

    $ok = 0;
    my $p;
    for ($p=1;$p && goto A;$p=0) { A: $ok = 1 }
    ok($ok, 'weird case of goto and for(;;) loop');
    is($deprecated, 1, "following goto and for(;;) loop");
    $deprecated = 0;
}

# bug #9990 - don't prematurely free the CV we're &going to.

sub f1 {
    my $x;
    goto sub { $x=0; ok(1,"don't prematurely free CV\n") }
}
f1();

# bug #99850, which is similar - freeing the subroutine we are about to
# go(in)to during a FREETMPS call should not crash perl.

package _99850 {
    sub reftype{}
    DESTROY { undef &reftype }
    eval { sub { my $guard = bless []; goto &reftype }->() };
}
like $@, qr/^Goto undefined subroutine &_99850::reftype at /,
   'goto &foo undefining &foo on sub cleanup';

# bug #22181 - this used to coredump or make $x undefined, due to
# erroneous popping of the inner BLOCK context

undef $ok;
for ($count=0; $count<2; $count++) {
    my $x = 1;
    goto LABEL29;
    LABEL29:
    $ok = $x;
}
is($ok, 1, 'goto in for(;;) with continuation');

# bug #22299 - goto in require doesn't find label

open my $f, ">Op_goto01.pm" or die;
print $f <<'EOT';
package goto01;
goto YYY;
die;
YYY: print "OK\n";
1;
EOT
close $f;

$r = runperl(prog => 'use Op_goto01; print qq[DONE\n]');
is($r, "OK\nDONE\n", "goto within use-d file"); 
unlink_all "Op_goto01.pm";

# test for [perl #24108]
$ok = 1;
$count = 0;
sub i_return_a_label {
    $count++;
    return "returned_label";
}
eval { goto +i_return_a_label; };
$ok = 0;

returned_label:
is($count, 1, 'called i_return_a_label');
ok($ok, 'skipped to returned_label');

# [perl #29708] - goto &foo could leave foo() at depth two with
# @_ == PL_sv_undef, causing a coredump


$r = runperl(
    prog =>
	'sub f { return if $d; $d=1; my $a=sub {goto &f}; &$a; f() } f(); print qq(ok\n)',
    stderr => 1
    );
is($r, "ok\n", 'avoid pad without an @_');

goto moretests;
fail('goto moretests');
exit;

bypass:

is(curr_test(), 9, 'eval "goto $x"');

# Test autoloading mechanism.

sub two {
    my ($pack, $file, $line) = caller;	# Should indicate original call stats.
    is("@_ $pack $file $line", "1 2 3 main $::FILE $::LINE",
	'autoloading mechanism.');
}

sub one {
    eval <<'END';
    no warnings 'redefine';
    sub one { pass('sub one'); goto &two; fail('sub one tail'); }
END
    goto &one;
}

$::FILE = __FILE__;
$::LINE = __LINE__ + 1;
&one(1,2,3);

{
    my $wherever = 'NOWHERE';
    eval { goto $wherever };
    like($@, qr/Can't find label NOWHERE/, 'goto NOWHERE sets $@');
}

# see if a modified @_ propagates
{
  my $i;
  package Foo;
  sub DESTROY	{ my $s = shift; ::is($s->[0], $i, "destroy $i"); }
  sub show	{ ::is(+@_, 5, "show $i",); }
  sub start	{ push @_, 1, "foo", {}; goto &show; }
  for (1..3)	{ $i = $_; start(bless([$_]), 'bar'); }
}

sub auto {
    goto &loadit;
}

sub AUTOLOAD { $ok = 1 if "@_" eq "foo" }

$ok = 0;
auto("foo");
ok($ok, 'autoload');

{
    my $wherever = 'FINALE';
    goto $wherever;
}
fail('goto $wherever');

moretests:
# test goto duplicated labels.
{
    my $z = 0;
    eval {
	$z = 0;
	for (0..1) {
	  L4: # not outer scope
	    $z += 10;
	    last;
	}
	goto L4 if $z == 10;
	last;
    };
    like($@, qr/Can't "goto" into the middle of a foreach loop/,
	    'catch goto middle of foreach');

    $z = 0;
    # ambiguous label resolution (outer scope means endless loop!)
  L1:
    for my $x (0..1) {
	$z += 10;
	is($z, 10, 'prefer same scope (loop body) to outer scope (loop entry)');
	goto L1 unless $x;
	$z += 10;
      L1:
	is($z, 10, 'prefer same scope: second');
	last;
    }

    $z = 0;
  L2: 
    { 
	$z += 10;
	is($z, 10, 'prefer this scope (block body) to outer scope (block entry)');
	goto L2 if $z == 10;
	$z += 10;
      L2:
	is($z, 10, 'prefer this scope: second');
    }


    { 
	$z = 0;
	while (1) {
	  L3: # not inner scope
	    $z += 10;
	    last;
	}
	is($z, 10, 'prefer this scope to inner scope');
	goto L3 if $z == 10;
	$z += 10;
      L3: # this scope !
	is($z, 10, 'prefer this scope to inner scope: second');
    }

  L4: # not outer scope
    { 
	$z = 0;
	while (1) {
	  L4: # not inner scope
	    $z += 1;
	    last;
	}
	is($z, 1, 'prefer this scope to inner,outer scopes');
	goto L4 if $z == 1;
	$z += 10;
      L4: # this scope !
	is($z, 1, 'prefer this scope to inner,outer scopes: second');
    }

    {
	my $loop = 0;
	for my $x (0..1) { 
	  L2: # without this, fails 1 (middle) out of 3 iterations
	    $z = 0;
	  L2: 
	    $z += 10;
	    is($z, 10,
		"same label, multiple times in same scope (choose 1st) $loop");
	    goto L2 if $z == 10 and not $loop++;
	}
    }
}

# deep recursion with gotos eventually caused a stack reallocation
# which messed up buggy internals that didn't expect the stack to move

sub recurse1 {
    unshift @_, "x";
    no warnings 'recursion';
    goto &recurse2;
}
sub recurse2 {
    my $x = shift;
    $_[0] ? +1 + recurse1($_[0] - 1) : 0
}
my $w = 0;
$SIG{__WARN__} = sub { ++$w };
is(recurse1(500), 500, 'recursive goto &foo');
is $w, 0, 'no recursion warnings for "no warnings; goto &sub"';
delete $SIG{__WARN__};

# [perl #32039] Chained goto &sub drops data too early. 

sub a32039 { @_=("foo"); goto &b32039; }
sub b32039 { goto &c32039; }
sub c32039 { is($_[0], 'foo', 'chained &goto') }
a32039();

# [perl #35214] next and redo re-entered the loop with the wrong cop,
# causing a subsequent goto to crash

{
    my $r = runperl(
		stderr => 1,
		prog =>
'for ($_=0;$_<3;$_++){A: if($_==1){next} if($_==2){$_++;goto A}}print qq(ok\n)'
    );
    is($r, "ok\n", 'next and goto');

    $r = runperl(
		stderr => 1,
		prog =>
'for ($_=0;$_<3;$_++){A: if($_==1){$_++;redo} if($_==2){$_++;goto A}}print qq(ok\n)'
    );
    is($r, "ok\n", 'redo and goto');
}

# goto &foo not allowed in evals

sub null { 1 };
eval 'goto &null';
like($@, qr/Can't goto subroutine from an eval-string/, 'eval string');
eval { goto &null };
like($@, qr/Can't goto subroutine from an eval-block/, 'eval block');
 
# goto &foo leaves @_ alone when called from a sub
sub returnarg { $_[0] };
is sub {
    local *_ = ["ick and queasy"];
    goto &returnarg;
}->("quick and easy"), "ick and queasy",
  'goto &foo with *_{ARRAY} replaced';
my @__ = "\xc4\x80";
sub { local *_ = \@__; goto &utf8::decode }->("no thinking aloud");
is "@__", chr 256, 'goto &xsub with replaced *_{ARRAY}';

# And goto &foo should leave reified @_ alone
sub { *__ = \@_;  goto &null } -> ("rough and tubbery");
is ${*__}[0], 'rough and tubbery', 'goto &foo leaves reified @_ alone';


# [perl #36521] goto &foo in warn handler could defeat recursion avoider

{
    my $r = runperl(
		stderr => 1,
		prog => 'my $d; my $w = sub { return if $d++; warn q(bar)}; local $SIG{__WARN__} = sub { goto &$w; }; warn q(foo);'
    );
    like($r, qr/bar/, "goto &foo in warn");
}

TODO: {
    local $TODO = "[perl #43403] goto() from an if to an else doesn't undo local () changes";
    our $global = "unmodified";
    if ($global) { # true but not constant-folded
         local $global = "modified";
         goto ELSE;
    } else {
         ELSE: is($global, "unmodified");
    }
}

is($deprecated, 0, "following TODOed test for #43403");

#74290
{
    my $x;
    my $y;
    F1:++$x and eval 'return if ++$y == 10; goto F1;';
    is($x, 10,
       'labels outside evals can be distinguished from the start of the eval');
}

goto wham_eth;
die "You can't get here";

wham_eth: 1 if 0;
ouch_eth: pass('labels persist even if their statement is optimised away');

$foo = "(0)";
if($foo eq $foo) {
    goto bungo;
}
$foo .= "(9)";
bungo:
format CHOLET =
wellington
.
$foo .= "(1)";
SKIP: {
    skip_if_miniperl("no dynamic loading on miniperl, so can't load PerlIO::scalar", 1);
    my $cholet;
    open(CHOLET, ">", \$cholet);
    write CHOLET;
    close CHOLET;
    $foo .= "(".$cholet.")";
    is($foo, "(0)(1)(wellington\n)", "label before format decl");
}

$foo = "(A)";
if($foo eq $foo) {
    goto orinoco;
}
$foo .= "(X)";
orinoco:
sub alderney { return "tobermory"; }
$foo .= "(B)";
$foo .= "(".alderney().")";
is($foo, "(A)(B)(tobermory)", "label before sub decl");

$foo = "[0:".__PACKAGE__."]";
if($foo eq $foo) {
    goto bulgaria;
}
$foo .= "[9]";
bulgaria:
package Tomsk;
$foo .= "[1:".__PACKAGE__."]";
$foo .= "[2:".__PACKAGE__."]";
package main;
$foo .= "[3:".__PACKAGE__."]";
is($foo, "[0:main][1:Tomsk][2:Tomsk][3:main]", "label before package decl");

$foo = "[A:".__PACKAGE__."]";
if($foo eq $foo) {
    goto adelaide;
}
$foo .= "[Z]";
adelaide:
package Cairngorm {
    $foo .= "[B:".__PACKAGE__."]";
}
$foo .= "[C:".__PACKAGE__."]";
is($foo, "[A:main][B:Cairngorm][C:main]", "label before package block");

our $obidos;
$foo = "{0}";
if($foo eq $foo) {
    goto shansi;
}
$foo .= "{9}";
shansi:
BEGIN { $obidos = "x"; }
$foo .= "{1$obidos}";
is($foo, "{0}{1x}", "label before BEGIN block");

$foo = "{A:".(1.5+1.5)."}";
if($foo eq $foo) {
    goto stepney;
}
$foo .= "{Z}";
stepney:
use integer;
$foo .= "{B:".(1.5+1.5)."}";
is($foo, "{A:3}{B:2}", "label before use decl");

$foo = "<0>";
if($foo eq $foo) {
    goto tom;
}
$foo .= "<9>";
tom: dick: harry:
$foo .= "<1>";
$foo .= "<2>";
is($foo, "<0><1><2>", "first of three stacked labels");

$foo = "<A>";
if($foo eq $foo) {
    goto beta;
}
$foo .= "<Z>";
alpha: beta: gamma:
$foo .= "<B>";
$foo .= "<C>";
is($foo, "<A><B><C>", "second of three stacked labels");

$foo = ",0.";
if($foo eq $foo) {
    goto gimel;
}
$foo .= ",9.";
alef: bet: gimel:
$foo .= ",1.";
$foo .= ",2.";
is($foo, ",0.,1.,2.", "third of three stacked labels");

# [perl #112316] Wrong behavior regarding labels with same prefix
sub same_prefix_labels {
    my $pass;
    my $first_time = 1;
    CATCH: {
        if ( $first_time ) {
            CATCHLOOP: {
                if ( !$first_time ) {
                  return 0;
                }
                $first_time--;
                goto CATCH;
            }
        }
        else {
            return 1;
        }
    }
}

ok(
   same_prefix_labels(),
   "perl 112316: goto and labels with the same prefix doesn't get mixed up"
);

eval { my $x = ""; goto $x };
like $@, qr/^goto must have label at /, 'goto $x where $x is empty string';
eval { goto "" };
like $@, qr/^goto must have label at /, 'goto ""';
eval { goto };
like $@, qr/^goto must have label at /, 'argless goto';

eval { my $x = "\0"; goto $x };
like $@, qr/^Can't find label \0 at /, 'goto $x where $x begins with \0';
eval { goto "\0" };
like $@, qr/^Can't find label \0 at /, 'goto "\0"';

sub TIESCALAR { bless [pop] }
sub FETCH     { $_[0][0] }
tie my $t, "", sub { "cluck up porridge" };
is eval { sub { goto $t }->() }//$@, 'cluck up porridge',
  'tied arg returning sub ref';
