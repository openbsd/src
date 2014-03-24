#!./perl

#
# various typeglob tests
#

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

use warnings;

plan( tests => 245 );

# type coercion on assignment
$foo = 'foo';
$bar = *main::foo;
$bar = $foo;
is(ref(\$bar), 'SCALAR');
$foo = *main::bar;

# type coercion (not) on misc ops

ok($foo);
is(ref(\$foo), 'GLOB');

unlike ($foo, qr/abcd/);
is(ref(\$foo), 'GLOB');

is($foo, '*main::bar');
is(ref(\$foo), 'GLOB');

{
 no warnings;
 ${\*$foo} = undef;
 is(ref(\$foo), 'GLOB', 'no type coercion when assigning to *{} retval');
 $::{phake} = *bar;
 is(
   \$::{phake}, \*{"phake"},
   'symbolic *{} returns symtab entry when FAKE'
 );
 ${\*{"phake"}} = undef;
 is(
   ref(\$::{phake}), 'GLOB',
  'no type coercion when assigning to retval of symbolic *{}'
 );
 $::{phaque} = *bar;
 eval '
   is(
     \$::{phaque}, \*phaque,
     "compile-time *{} returns symtab entry when FAKE"
   );
   ${\*phaque} = undef;
 ';
 is(
   ref(\$::{phaque}), 'GLOB',
  'no type coercion when assigning to retval of compile-time *{}'
 );
}

# type coercion on substitutions that match
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
is ("@{sub { *_{ARRAY} }->(1..3)}", "1 2 3",
    'returning *_{ARRAY} from sub');
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

is *x{NAME}, 'x', '*foo{NAME}';
is *x{PACKAGE}, 'main', '*foo{PACKAGE}';
{ no warnings 'once'; *x = *Foo::y; }
is *x, '*Foo::y', 'glob stringifies as assignee after glob-to-glob assign';
is *x{NAME}, 'x', 'but *foo{NAME} still returns the original name';
is *x{PACKAGE}, 'main', 'and *foo{PACKAGE} the original package';

{
    # test if defined() doesn't create any new symbols

    my $a = "SYM000";
    ok(!defined *{$a});

    {
	no warnings 'deprecated';
	ok(!defined @{$a});
    }
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
    ok(defined *{$a});
    ok(defined ${$a});

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
    my %v;
    sub f { $_[0] = 0; $_[0] = "a"; $_[0] = *DATA }
    f($v{v});
    is ($v{v}, '*main::DATA');
    is (ref\$v{v}, 'GLOB', 'lvalue assignment preserves globs');
    my $x = readline $v{v};
    is ($x, "perl\n");
    is ($e, '', '__DIE__ handler never called');
}

{
    my $e = '';
    # GLOB assignment to tied element
    local $SIG{__DIE__} = sub { $e = $_[0] };
    sub T::TIEARRAY  { bless [] => "T" }
    sub T::STORE     { $_[0]->[ $_[1] ] = $_[2] }
    sub T::FETCH     { $_[0]->[ $_[1] ] }
    sub T::FETCHSIZE { @{$_[0]} }
    tie my @ary => "T";
    $ary[0] = *DATA;
    is ($ary[0], '*main::DATA');
    is (
      ref\tied(@ary)->[0], 'GLOB',
     'tied elem assignment preserves globs'
    );
    is ($e, '', '__DIE__ handler not called');
    my $x = readline $ary[0];
    is($x, "rocks\n");
    is ($e, '', '__DIE__ handler never called');
}

{
    # Need some sort of die or warn to get the global destruction text if the
    # bug is still present
    my $output = runperl(prog => <<'EOPROG');
package M;
$| = 1;
sub DESTROY {eval {die qq{Farewell $_[0]}}; print $@}
package main;

bless \$A::B, q{M};
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
    "Inlining of constant doesn't change representation");

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
             "Assignment works when glob created midway (bug 45607)"); 1'
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

# RT #65582 anonymous glob should be defined, and not coredump when
# stringified. The behaviours are:
#
#        defined($glob)    "$glob"                   $glob .= ...
# 5.8.8     false           "" with uninit warning   "" with uninit warning
# 5.10.0    true            (coredump)               (coredump)
# 5.1[24]   true            ""                       "" with uninit warning
# 5.16      true            "*__ANON__::..."         "*__ANON__::..."

{
    my $io_ref = *STDOUT{IO};
    my $glob = *$io_ref;
    ok(defined $glob, "RT #65582 anon glob should be defined");

    my $warn = '';
    local $SIG{__WARN__} = sub { $warn = $_[0] };
    use warnings;
    my $str = "$glob";
    is($warn, '', "RT #65582 anon glob stringification shouldn't warn");
    is($str,  '*__ANON__::__ANONIO__',
	"RT #65582/#96326 anon glob stringification");
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

# [perl #71686] Globs that are in symbol table can be un-globbed
$sym = undef;
$::{fake} = *sym;
is (eval 'local *::fake = \"chuck"; $fake', 'chuck',
	"Localized glob didn't coerce into a RV");
is ($@, '', "Can localize FAKE glob that's present in stash");
is (scalar $::{fake}, "*main::sym",
	"Localized FAKE glob's value was correctly restored");

# [perl #1804] *$x assignment when $x is a copy of another glob
# And [perl #77508] (same thing with list assignment)
{
    no warnings 'once';
    my $x = *_random::glob_that_is_not_used_elsewhere;
    *$x = sub{};
    is(
      "$x", '*_random::glob_that_is_not_used_elsewhere',
      '[perl #1804] *$x assignment when $x is FAKE',
    );
    $x = *_random::glob_that_is_not_used_elsewhere;
    (my $dummy, *$x) = (undef,[]);
    is(
      "$x", '*_random::glob_that_is_not_used_elsewhere',
      '[perl #77508] *$x list assignment when $x is FAKE',
    ) or require Devel::Peek, Devel::Peek::Dump($x);
}

# [perl #76540]
# this caused panics or 'Attempt to free unreferenced scalar'
# (its a compile-time issue, so the die lets us skip the prints)
{
    my @warnings;
    local $SIG{__WARN__} = sub { push @warnings, @_ };

    eval <<'EOF';
BEGIN { $::{FOO} = \'bar' }
die "made it";
print FOO, "\n";
print FOO, "\n";
EOF

    like($@, qr/made it/, "#76540 - no panic");
    ok(!@warnings, "#76540 - no 'Attempt to free unreferenced scalar'");
}

# [perl #77362] various bugs related to globs as PVLVs
{
 no warnings qw 'once void';
 my %h; # We pass a key of this hash to the subroutine to get a PVLV.
 sub { for(shift) {
  # Set up our glob-as-PVLV
  $_ = *hon;

  # Bad symbol for array
  ok eval{ @$_; 1 }, 'PVLV glob slots can be autovivified' or diag $@;

  # This should call TIEHANDLE, not TIESCALAR
  *thext::TIEHANDLE = sub{};
  ok eval{ tie *$_, 'thext'; 1 }, 'PVLV globs can be tied as handles'
   or diag $@;

  # Assigning undef to the glob should not overwrite it...
  {
   my $w;
   local $SIG{__WARN__} = sub { $w = shift };
   *$_ = undef;
   is $_, "*main::hon", 'PVLV: assigning undef to the glob does nothing';
   like $w, qr\Undefined value assigned to typeglob\,
    'PVLV: assigning undef to the glob warns';
  }

  # Neither should reference assignment.
  *$_ = [];
  is $_, "*main::hon", "PVLV: arrayref assignment assigns to the AV slot";

  # Concatenation should still work.
  ok eval { $_ .= 'thlew' }, 'PVLV concatenation does not die' or diag $@;
  is $_, '*main::honthlew', 'PVLV concatenation works';

  # And we should be able to overwrite it with a string, number, or refer-
  # ence, too, if we omit the *.
  $_ = *hon; $_ = 'tzor';
  is $_, 'tzor', 'PVLV: assigning a string over a glob';
  $_ = *hon; $_ = 23;
  is $_, 23, 'PVLV: assigning an integer over a glob';
  $_ = *hon; $_ = 23.23;
  is $_, 23.23, 'PVLV: assigning a float over a glob';
  $_ = *hon; $_ = \my $sthat;
  is $_, \$sthat, 'PVLV: assigning a reference over a glob';

  # This bug was found by code inspection. Could this ever happen in
  # real life? :-)
  # This duplicates a file handle, accessing it through a PVLV glob, the
  # glob having been removed from the symbol table, so a stringified form
  # of it does not work. This checks that sv_2io does not stringify a PVLV.
  $_ = *quin;
  open *quin, "test.pl"; # test.pl is as good a file as any
  delete $::{quin};
  ok eval { open my $zow, "<&", $_ }, 'PVLV: sv_2io stringifieth not'
   or diag $@;

  # Similar tests to make sure sv_2cv etc. do not stringify.
  *$_ = sub { 1 };
  ok eval { &$_ }, "PVLV glob can be called as a sub" or diag $@;
  *flelp = sub { 2 };
  $_ = 'flelp';
  is eval { &$_ }, 2, 'PVLV holding a string can be called as a sub'
   or diag $@;

  # Coderef-to-glob assignment when the glob is no longer accessible
  # under its name: These tests are to make sure the OPpASSIGN_CV_TO_GV
  # optimisation takes PVLVs into account, which is why the RHSs have to be
  # named subs.
  use constant gheen => 'quare';
  $_ = *ming;
  delete $::{ming};
  *$_ = \&gheen;
  is eval { &$_ }, 'quare',
   'PVLV: constant assignment when the glob is detached from the symtab'
    or diag $@;
  $_ = *bength;
  delete $::{bength};
  *gheck = sub { 'lon' };
  *$_ = \&gheck;
  is eval { &$_ }, 'lon',
   'PVLV: coderef assignment when the glob is detached from the symtab'
    or diag $@;

SKIP: {
    skip_if_miniperl("no dynamic loading on miniperl, so can't load PerlIO::scalar", 1);
    # open should accept a PVLV as its first argument
    $_ = *hon;
    ok eval { open $_,'<', \my $thlext }, 'PVLV can be the first arg to open'
	or diag $@;
  }

  # -t should not stringify
  $_ = *thlit; delete $::{thlit};
  *$_ = *STDOUT{IO};
  ok defined -t $_, 'PVLV: -t does not stringify';

  # neither should -T
  # but some systems don’t support this on file handles
  my $pass;
  ok
    eval {
     open my $quile, "<", 'test.pl';
     $_ = *$quile;
     $pass = -T $_;
     1
    } ? $pass : $@ =~ /not implemented on filehandles/,
   "PVLV: -T does not stringify";
  
  # Unopened file handle
  {
   my $w;
   local $SIG{__WARN__} = sub { $w .= shift };
   $_ = *vor;
   close $_;
   like $w, qr\unopened filehandle vor\,
    'PVLV globs get their names reported in unopened error messages';
  }

 }}->($h{k});
}

*aieee = 4;
pass('Can assign integers to typeglobs');
*aieee = 3.14;
pass('Can assign floats to typeglobs');
*aieee = 'pi';
pass('Can assign strings to typeglobs');

{
  package thrext;
  sub TIESCALAR{bless[]}
  sub STORE{ die "No!"}
  sub FETCH{ no warnings 'once'; *thrit }
  tie my $a, "thrext";
  () = "$a"; # do a fetch; now $a holds a glob
  eval { *$a = sub{} };
  untie $a;
  eval { $a = "bar" };
  ::is $a, "bar",
    "[perl #77812] Globs in tied scalars can be reified if STORE dies"
}

# These two crashed prior to 5.13.6. In 5.13.6 they were fatal errors. They
# were fixed in 5.13.7.
ok eval {
  my $glob = \*heen::ISA;
  delete $::{"heen::"};
  *$glob = *bar; 
}, "glob-to-*ISA assignment works when *ISA has lost its stash";
ok eval {
  my $glob = \*slare::ISA;
  delete $::{"slare::"};
  *$glob = []; 
}, "array-to-*ISA assignment works when *ISA has lost its stash";
# These two crashed in 5.13.6. They were likewise fixed in 5.13.7.
ok eval {
  sub greck;
  my $glob = do { no warnings "once"; \*phing::foo};
  delete $::{"phing::"};
  *$glob = *greck; 
}, "Assigning a glob-with-sub to a glob that has lost its stash works";
ok eval {
  sub pon::foo;
  my $glob = \*pon::foo;
  delete $::{"pon::"};
  *$glob = *foo; 
}, "Assigning a glob to a glob-with-sub that has lost its stash works";

{
  package Tie::Alias;
  sub TIESCALAR{ bless \\pop }
  sub FETCH { $${$_[0]} }
  sub STORE { $${$_[0]} = $_[1] }
  package main;
  tie my $alias, 'Tie::Alias', my $var;
  no warnings 'once';
  $var = *galobbe;
  {
    local *$alias = [];
    $var = 3;
    is $alias, 3, "[perl #77926] Glob reification during localisation";
  }
}

# This code causes gp_free to call a destructor when a glob is being
# restored on scope exit. The destructor used to see SVs with a refcount of
# zero inside the glob, which could result in crashes (though not in this
# test case, which just panics).
{
 no warnings 'once';
 my $survived;
 *Trit::DESTROY = sub {
   $thwext = 42;  # panic
   $survived = 1;
 };
 {
  local *thwext;
  $thwext = bless[],'Trit';
  ();
 }
 ok $survived,
  'no error when gp_free calls a destructor that assigns to the gv';
}

# *{undef}
eval { *{my $undef} = 3 };
like $@, qr/^Can't use an undefined value as a symbol reference at /,
  '*{ $undef } assignment';
eval { *{;undef} = 3 };
like $@, qr/^Can't use an undefined value as a symbol reference at /,
  '*{ ;undef } assignment';

# [perl #99142] defined &{"foo"} when there is a constant stub
# If I break your module, you get to have it mentioned in Perl's tests. :-)
package HTTP::MobileAttribute::Plugin::Locator {
    use constant LOCATOR_GPS => 1;
    ::ok defined &{__PACKAGE__."::LOCATOR_GPS"},
        'defined &{"name of constant"}';
    ::ok Internals::SvREFCNT(${__PACKAGE__."::"}{LOCATOR_GPS}),
       "stash elem for slot is not freed prematurely";
}

# Check that constants promoted to CVs point to the right GVs when the name
# contains a null.
package lrcg {
  use constant x => 3;
  # These two lines abuse the optimisation that copies the scalar ref from
  # one stash element to another, to get a constant with a null in its name
  *{"yz\0a"} = \&{"x"};
  my $ref = \&{"yz\0a"};
  ::ok !exists $lrcg::{yz},
    'constants w/nulls in their names point 2 the right GVs when promoted';
}

# Look away, please.
# This violates perl's internal structures by fiddling with stashes in a
# way that should never happen, but perl should not start trying to free
# unallocated memory as a result.  There is no ok() or is() because the
# panic that used to occur only occurred during global destruction, and
# only with PERL_DESTRUCT_LEVEL=2.  (The panic itself was sufficient for
# the harness to consider this test script to have failed.)
$::{aoeuaoeuaoeaoeu} = __PACKAGE__; # cow
() = *{"aoeuaoeuaoeaoeu"};

__END__
Perl
Rules
perl
rocks
