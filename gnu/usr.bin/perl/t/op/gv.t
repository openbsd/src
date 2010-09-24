#!./perl

#
# various typeglob tests
#

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

use warnings;

require './test.pl';
plan( tests => 188 );

# type coersion on assignment
$foo = 'foo';
$bar = *main::foo;
$bar = $foo;
is(ref(\$bar), 'SCALAR');
$foo = *main::bar;

# type coersion (not) on misc ops

ok($foo);
is(ref(\$foo), 'GLOB');

unlike ($foo, qr/abcd/);
is(ref(\$foo), 'GLOB');

is($foo, '*main::bar');
is(ref(\$foo), 'GLOB');

# type coersion on substitutions that match
$a = *main::foo;
$b = $a;
$a =~ s/^X//;
is(ref(\$a), 'GLOB');
$a =~ s/^\*//;
is($a, 'main::foo');
is(ref(\$b), 'GLOB');

# typeglobs as lvalues
substr($foo, 0, 1) = "XXX";
is(ref(\$foo), 'SCALAR');
is($foo, 'XXXmain::bar');

# returning glob values
sub foo {
  local($bar) = *main::foo;
  $foo = *main::bar;
  return ($foo, $bar);
}

($fuu, $baa) = foo();
ok(defined $fuu);
is(ref(\$fuu), 'GLOB');


ok(defined $baa);
is(ref(\$baa), 'GLOB');

# nested package globs
# NOTE:  It's probably OK if these semantics change, because the
#        fact that %X::Y:: is stored in %X:: isn't documented.
#        (I hope.)

{ package Foo::Bar; no warnings 'once'; $test=1; }
ok(exists $Foo::{'Bar::'});
is($Foo::{'Bar::'}, '*Foo::Bar::');


# test undef operator clearing out entire glob
$foo = 'stuff';
@foo = qw(more stuff);
%foo = qw(even more random stuff);
undef *foo;
is ($foo, undef);
is (scalar @foo, 0);
is (scalar %foo, 0);

{
    # test warnings from assignment of undef to glob
    my $msg = '';
    local $SIG{__WARN__} = sub { $msg = $_[0] };
    use warnings;
    *foo = 'bar';
    is($msg, '');
    *foo = undef;
    like($msg, qr/Undefined value assigned to typeglob/);

    no warnings 'once';
    # test warnings for converting globs to other forms
    my $copy = *PWOMPF;
    foreach ($copy, *SKREEE) {
	$msg = '';
	my $victim = sprintf "%d", $_;
	like($msg, qr/Argument "\*main::[A-Z]{6}" isn't numeric in sprintf/,
	     "Warning on conversion to IV");
	is($victim, 0);

	$msg = '';
	$victim = sprintf "%u", $_;
	like($msg, qr/Argument "\*main::[A-Z]{6}" isn't numeric in sprintf/,
	     "Warning on conversion to UV");
	is($victim, 0);

	$msg = '';
	$victim = sprintf "%e", $_;
	like($msg, qr/Argument "\*main::[A-Z]{6}" isn't numeric in sprintf/,
	     "Warning on conversion to NV");
	like($victim, qr/^0\.0+E\+?00/i, "Expect floating point zero");

	$msg = '';
	$victim = sprintf "%s", $_;
	is($msg, '', "No warning on stringification");
	is($victim, '' . $_);
    }
}

my $test = curr_test();
# test *glob{THING} syntax
$x = "ok $test\n";
++$test;
@x = ("ok $test\n");
++$test;
%x = ("ok $test" => "\n");
++$test;
sub x { "ok $test\n" }
print ${*x{SCALAR}}, @{*x{ARRAY}}, %{*x{HASH}}, &{*x{CODE}};
# This needs to go here, after the print, as sub x will return the current
# value of test
++$test;
format x =
XXX This text isn't used. Should it be?
.
curr_test($test);

is (ref *x{FORMAT}, "FORMAT");
*x = *STDOUT;
is (*{*x{GLOB}}, "*main::STDOUT");

{
    my $test = curr_test();

    print {*x{IO}} "ok $test\n";
    ++$test;

    my $warn;
    local $SIG{__WARN__} = sub {
	$warn .= $_[0];
    };
    my $val = *x{FILEHANDLE};
    print {*x{IO}} ($warn =~ /is deprecated/
		    ? "ok $test\n" : "not ok $test\n");
    curr_test(++$test);
}


{
    # test if defined() doesn't create any new symbols

    my $a = "SYM000";
    ok(!defined *{$a});

    ok(!defined @{$a});
    ok(!defined *{$a});

    {
	no warnings 'deprecated';
	ok(!defined %{$a});
    }
    ok(!defined *{$a});

    ok(!defined ${$a});
    ok(!defined *{$a});

    ok(!defined &{$a});
    ok(!defined *{$a});

    my $state = "not";
    *{$a} = sub { $state = "ok" };
    ok(defined &{$a});
    ok(defined *{$a});
    &{$a};
    is ($state, 'ok');
}

{
    # although it *should* if you're talking about magicals

    my $a = "]";
    ok(defined ${$a});
    ok(defined *{$a});

    $a = "1";
    "o" =~ /(o)/;
    ok(${$a});
    ok(defined *{$a});
    $a = "2";
    ok(!${$a});
    ok(defined *{$a});
    $a = "1x";
    ok(!defined ${$a});
    ok(!defined *{$a});
    $a = "11";
    "o" =~ /(((((((((((o)))))))))))/;
    ok(${$a});
    ok(defined *{$a});
}

# [ID 20010526.001] localized glob loses value when assigned to

$j=1; %j=(a=>1); @j=(1); local *j=*j; *j = sub{};

is($j, 1);
is($j{a}, 1);
is($j[0], 1);

{
    # does pp_readline() handle glob-ness correctly?
    my $g = *foo;
    $g = <DATA>;
    is ($g, "Perl\n");
}

{
    my $w = '';
    local $SIG{__WARN__} = sub { $w = $_[0] };
    sub abc1 ();
    local *abc1 = sub { };
    is ($w, '');
    sub abc2 ();
    local *abc2;
    *abc2 = sub { };
    is ($w, '');
    sub abc3 ();
    *abc3 = sub { };
    like ($w, qr/Prototype mismatch/);
}

{
    # [17375] rcatline to formerly-defined undef was broken. Fixed in
    # do_readline by checking SvOK. AMS, 20020918
    my $x = "not ";
    $x  = undef;
    $x .= <DATA>;
    is ($x, "Rules\n");
}

{
    # test the assignment of a GLOB to an LVALUE
    my $e = '';
    local $SIG{__DIE__} = sub { $e = $_[0] };
    my $v;
    sub f { $_[0] = 0; $_[0] = "a"; $_[0] = *DATA }
    f($v);
    is ($v, '*main::DATA');
    my $x = <$v>;
    is ($x, "perl\n");
}

{
    $e = '';
    # GLOB assignment to tied element
    local $SIG{__DIE__} = sub { $e = $_[0] };
    sub T::TIEARRAY  { bless [] => "T" }
    sub T::STORE     { $_[0]->[ $_[1] ] = $_[2] }
    sub T::FETCH     { $_[0]->[ $_[1] ] }
    sub T::FETCHSIZE { @{$_[0]} }
    tie my @ary => "T";
    $ary[0] = *DATA;
    is ($ary[0], '*main::DATA');
    is ($e, '');
    my $x = readline $ary[0];
    is($x, "rocks\n");
}

{
    # Need some sort of die or warn to get the global destruction text if the
    # bug is still present
    my $output = runperl(prog => <<'EOPROG');
package M;
$| = 1;
sub DESTROY {eval {die qq{Farewell $_[0]}}; print $@}
package main;

bless \$A::B, 'M';
*A:: = \*B::;
EOPROG
    like($output, qr/^Farewell M=SCALAR/, "DESTROY was called");
    unlike($output, qr/global destruction/,
           "unreferenced symbol tables should be cleaned up immediately");
}

# Possibly not the correct test file for these tests.
# There are certain space optimisations implemented via promotion rules to
# GVs

foreach (qw (oonk ga_shloip)) {
    ok(!exists $::{$_}, "no symbols of any sort to start with for $_");
}

# A string in place of the typeglob is promoted to the function prototype
$::{oonk} = "pie";
my $proto = eval 'prototype \&oonk';
die if $@;
is ($proto, "pie", "String is promoted to prototype");


# A reference to a value is used to generate a constant subroutine
foreach my $value (3, "Perl rules", \42, qr/whatever/, [1,2,3], {1=>2},
		   \*STDIN, \&ok, \undef, *STDOUT) {
    delete $::{oonk};
    $::{oonk} = \$value;
    $proto = eval 'prototype \&oonk';
    die if $@;
    is ($proto, '', "Prototype for a constant subroutine is empty");

    my $got = eval 'oonk';
    die if $@;
    is (ref $got, ref $value, "Correct type of value (" . ref($value) . ")");
    is ($got, $value, "Value is correctly set");
}

delete $::{oonk};
$::{oonk} = \"Value";

*{"ga_shloip"} = \&{"oonk"};

is (ref $::{ga_shloip}, 'SCALAR', "Export of proxy constant as is");
is (ref $::{oonk}, 'SCALAR', "Export doesn't affect original");
is (eval 'ga_shloip', "Value", "Constant has correct value");
is (ref $::{ga_shloip}, 'SCALAR',
    "Inlining of constant doesn't change represenatation");

delete $::{ga_shloip};

eval 'sub ga_shloip (); 1' or die $@;
is ($::{ga_shloip}, '', "Prototype is stored as an empty string");

# Check that a prototype expands.
*{"ga_shloip"} = \&{"oonk"};

is (ref $::{oonk}, 'SCALAR', "Export doesn't affect original");
is (eval 'ga_shloip', "Value", "Constant has correct value");
is (ref \$::{ga_shloip}, 'GLOB', "Symbol table has full typeglob");


@::zwot = ('Zwot!');

# Check that assignment to an existing typeglob works
{
  my $w = '';
  local $SIG{__WARN__} = sub { $w = $_[0] };
  *{"zwot"} = \&{"oonk"};
  is($w, '', "Should be no warning");
}

is (ref $::{oonk}, 'SCALAR', "Export doesn't affect original");
is (eval 'zwot', "Value", "Constant has correct value");
is (ref \$::{zwot}, 'GLOB', "Symbol table has full typeglob");
is (join ('!', @::zwot), 'Zwot!', "Existing array still in typeglob");

sub spritsits () {
    "Traditional";
}

# Check that assignment to an existing subroutine works
{
  my $w = '';
  local $SIG{__WARN__} = sub { $w = $_[0] };
  *{"spritsits"} = \&{"oonk"};
  like($w, qr/^Constant subroutine main::spritsits redefined/,
       "Redefining a constant sub should warn");
}

is (ref $::{oonk}, 'SCALAR', "Export doesn't affect original");
is (eval 'spritsits', "Value", "Constant has correct value");
is (ref \$::{spritsits}, 'GLOB', "Symbol table has full typeglob");

# Check that assignment to an existing typeglob works
{
  my $w = '';
  local $SIG{__WARN__} = sub { $w = $_[0] };
  *{"plunk"} = [];
  *{"plunk"} = \&{"oonk"};
  is($w, '', "Should be no warning");
}

is (ref $::{oonk}, 'SCALAR', "Export doesn't affect original");
is (eval 'plunk', "Value", "Constant has correct value");
is (ref \$::{plunk}, 'GLOB', "Symbol table has full typeglob");

my $gr = eval '\*plunk' or die;

{
  my $w = '';
  local $SIG{__WARN__} = sub { $w = $_[0] };
  *{$gr} = \&{"oonk"};
  is($w, '', "Redefining a constant sub to another constant sub with the same underlying value should not warn (It's just re-exporting, and that was always legal)");
}

is (ref $::{oonk}, 'SCALAR', "Export doesn't affect original");
is (eval 'plunk', "Value", "Constant has correct value");
is (ref \$::{plunk}, 'GLOB', "Symbol table has full typeglob");

# Non-void context should defeat the optimisation, and will cause the original
# to be promoted (what change 26482 intended)
my $result;
{
  my $w = '';
  local $SIG{__WARN__} = sub { $w = $_[0] };
  $result = *{"awkkkkkk"} = \&{"oonk"};
  is($w, '', "Should be no warning");
}

is (ref \$result, 'GLOB',
    "Non void assignment should still return a typeglob");

is (ref \$::{oonk}, 'GLOB', "This export does affect original");
is (eval 'plunk', "Value", "Constant has correct value");
is (ref \$::{plunk}, 'GLOB', "Symbol table has full typeglob");

delete $::{oonk};
$::{oonk} = \"Value";

sub non_dangling {
  my $w = '';
  local $SIG{__WARN__} = sub { $w = $_[0] };
  *{"zap"} = \&{"oonk"};
  is($w, '', "Should be no warning");
}

non_dangling();
is (ref $::{oonk}, 'SCALAR', "Export doesn't affect original");
is (eval 'zap', "Value", "Constant has correct value");
is (ref $::{zap}, 'SCALAR', "Exported target is also a PCS");

sub dangling {
  local $SIG{__WARN__} = sub { die $_[0] };
  *{"biff"} = \&{"oonk"};
}

dangling();
is (ref \$::{oonk}, 'GLOB', "This export does affect original");
is (eval 'biff', "Value", "Constant has correct value");
is (ref \$::{biff}, 'GLOB', "Symbol table has full typeglob");

{
    use vars qw($glook $smek $foof);
    # Check reference assignment isn't affected by the SV type (bug #38439)
    $glook = 3;
    $smek = 4;
    $foof = "halt and cool down";

    my $rv = \*smek;
    is($glook, 3);
    *glook = $rv;
    is($glook, 4);

    my $pv = "";
    $pv = \*smek;
    is($foof, "halt and cool down");
    *foof = $pv;
    is($foof, 4);
}

format =
.

foreach my $value ([1,2,3], {1=>2}, *STDOUT{IO}, \&ok, *STDOUT{FORMAT}) {
    # *STDOUT{IO} returns a reference to a PVIO. As it's blessed, ref returns
    # IO::Handle, which isn't what we want.
    my $type = $value;
    $type =~ s/.*=//;
    $type =~ s/\(.*//;
    delete $::{oonk};
    $::{oonk} = $value;
    $proto = eval 'prototype \&oonk';
    like ($@, qr/^Cannot convert a reference to $type to typeglob/,
	  "Cannot upgrade ref-to-$type to typeglob");
}

{
    no warnings qw(once uninitialized);
    my $g = \*clatter;
    my $r = eval {no strict; ${*{$g}{SCALAR}}};
    is ($@, '', "PERL_DONT_CREATE_GVSV shouldn't affect thingy syntax");

    $g = \*vowm;
    $r = eval {use strict; ${*{$g}{SCALAR}}};
    is ($@, '',
	"PERL_DONT_CREATE_GVSV shouldn't affect thingy syntax under strict");
}

{
    # Bug reported by broquaint on IRC
    *slosh::{HASH}->{ISA}=[];
    slosh->import;
    pass("gv_fetchmeth coped with the unexpected");

    # An audit found these:
    {
	package slosh;
	sub rip {
	    my $s = shift;
	    $s->SUPER::rip;
	}
    }
    eval {slosh->rip;};
    like ($@, qr/^Can't locate object method "rip"/, "Even with SUPER");

    is(slosh->isa('swoosh'), '');

    $CORE::GLOBAL::{"lock"}=[];
    eval "no warnings; lock";
    like($@, qr/^Not enough arguments for lock/,
       "Can't trip up general keyword overloading");

    $CORE::GLOBAL::{"readline"}=[];
    eval "<STDOUT> if 0";
    is($@, '', "Can't trip up readline overloading");

    $CORE::GLOBAL::{"readpipe"}=[];
    eval "`` if 0";
    is($@, '', "Can't trip up readpipe overloading");
}

{
    die if exists $::{BONK};
    $::{BONK} = \"powie";
    *{"BONK"} = \&{"BONK"};
    eval 'is(BONK(), "powie",
             "Assigment works when glob created midway (bug 45607)"); 1'
	or die $@;
}

# For now these tests are here, but they would probably be better in a file for
# tests for croaks. (And in turn, that probably deserves to be in a different
# directory. Gerard Goossen has a point about the layout being unclear

sub coerce_integer {
    no warnings 'numeric';
    $_[0] |= 0;
}
sub coerce_number {
    no warnings 'numeric';
    $_[0] += 0;
}
sub coerce_string {
    $_[0] .= '';
}

foreach my $type (qw(integer number string)) {
    my $prog = "coerce_$type(*STDERR)";
    is (scalar eval "$prog; 1", undef, "$prog failed...");
    like ($@, qr/Can't coerce GLOB to $type in/,
	  "with the correct error message");
}

# RT #60954 anonymous glob should be defined, and not coredump when
# stringified. The behaviours are:
#
#        defined($glob)    "$glob"
# 5.8.8     false           "" with uninit warning
# 5.10.0    true            (coredump)
# 5.12.0    true            ""

{
    my $io_ref = *STDOUT{IO};
    my $glob = *$io_ref;
    ok(defined $glob, "RT #60954 anon glob should be defined");

    my $warn = '';
    local $SIG{__WARN__} = sub { $warn = $_[0] };
    use warnings;
    my $str = "$glob";
    is($warn, '', "RT #60954 anon glob stringification shouln't warn");
    is($str,  '', "RT #60954 anon glob stringification should be empty");
}

# [perl #71254] - Assigning a glob to a variable that has a current
# match position. (We are testing that Perl_magic_setmglob respects globs'
# special used of SvSCREAM.)
{
    $m = 2; $m=~s/./0/gems; $m= *STDERR;
    is(
        "$m", "*main::STDERR",
        '[perl #71254] assignment of globs to vars with pos'
    );
}

# [perl #72740] - indirect object syntax, heuristically imputed due to
# the non-existence of a function, should not cause a stash entry to be
# created for the non-existent function.
{
	package RT72740a;
	my $f = bless({}, RT72740b);
	sub s1 { s2 $f; }
	our $s4;
	sub s3 { s4 $f; }
}
{
	package RT72740b;
	sub s2 { "RT72740b::s2" }
	sub s4 { "RT72740b::s4" }
}
ok(exists($RT72740a::{s1}), "RT72740a::s1 exists");
ok(!exists($RT72740a::{s2}), "RT72740a::s2 does not exist");
ok(exists($RT72740a::{s3}), "RT72740a::s3 exists");
ok(exists($RT72740a::{s4}), "RT72740a::s4 exists");
is(RT72740a::s1(), "RT72740b::s2", "RT72740::s1 parsed correctly");
is(RT72740a::s3(), "RT72740b::s4", "RT72740::s3 parsed correctly");

__END__
Perl
Rules
perl
rocks
