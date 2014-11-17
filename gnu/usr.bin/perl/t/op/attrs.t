#!./perl

# Regression tests for attributes.pm and the C< : attrs> syntax.

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
    skip_all_if_miniperl("miniperl can't load attributes");
}

use warnings;

$SIG{__WARN__} = sub { die @_ };

sub eval_ok ($;$) {
    eval shift;
    is( $@, '', @_);
}

fresh_perl_is 'use attributes; print "ok"', 'ok', {},
   'attributes.pm can load without warnings.pm already loaded';

our $anon1; eval_ok '$anon1 = sub : method { $_[0]++ }';

eval 'sub e1 ($) : plugh ;';
like $@, qr/^Invalid CODE attributes?: ["']?plugh["']? at/;

eval 'sub e2 ($) : plugh(0,0) xyzzy ;';
like $@, qr/^Invalid CODE attributes: ["']?plugh\(0,0\)["']? /;

eval 'sub e3 ($) : plugh(0,0 xyzzy ;';
like $@, qr/Unterminated attribute parameter in attribute list at/;

eval 'sub e4 ($) : plugh + XYZZY ;';
like $@, qr/Invalid separator character '[+]' in attribute list at/;

eval_ok 'my main $x : = 0;';
eval_ok 'my $x : = 0;';
eval_ok 'my $x ;';
eval_ok 'my ($x) : = 0;';
eval_ok 'my ($x) ;';
eval_ok 'my ($x) : ;';
eval_ok 'my ($x,$y) : = 0;';
eval_ok 'my ($x,$y) ;';
eval_ok 'my ($x,$y) : ;';

eval 'my ($x,$y) : plugh;';
like $@, qr/^Invalid SCALAR attribute: ["']?plugh["']? at/;

# bug #16080
eval '{my $x : plugh}';
like $@, qr/^Invalid SCALAR attribute: ["']?plugh["']? at/;
eval '{my ($x,$y) : plugh(})}';
like $@, qr/^Invalid SCALAR attribute: ["']?plugh\(}\)["']? at/;

# More syntax tests from the attributes manpage
eval 'my $x : switch(10,foo(7,3))  :  expensive;';
like $@, qr/^Invalid SCALAR attributes: ["']?switch\(10,foo\(7,3\)\) : expensive["']? at/;
eval q/my $x : Ugly('\(") :Bad;/;
like $@, qr/^Invalid SCALAR attributes: ["']?Ugly\('\\\("\) : Bad["']? at/;
eval 'my $x : _5x5;';
like $@, qr/^Invalid SCALAR attribute: ["']?_5x5["']? at/;
eval 'my $x : locked method;';
like $@, qr/^Invalid SCALAR attributes: ["']?locked : method["']? at/;
eval 'my $x : switch(10,foo();';
like $@, qr/^Unterminated attribute parameter in attribute list at/;
eval q/my $x : Ugly('(');/;
like $@, qr/^Unterminated attribute parameter in attribute list at/;
eval 'my $x : 5x5;';
like $@, qr/error/;
eval 'my $x : Y2::north;';
like $@, qr/Invalid separator character ':' in attribute list at/;

sub A::MODIFY_SCALAR_ATTRIBUTES { return }
eval 'my A $x : plugh;';
like $@, qr/^SCALAR package attribute may clash with future reserved word: ["']?plugh["']? at/;

eval 'my A $x : plugh plover;';
like $@, qr/^SCALAR package attributes may clash with future reserved words: ["']?plugh["']? /;

no warnings 'reserved';
eval 'my A $x : plugh;';
is $@, '';

eval 'package Cat; my Cat @socks;';
like $@, '';

eval 'my Cat %nap;';
like $@, '';

sub X::MODIFY_CODE_ATTRIBUTES { die "$_[0]" }
sub X::foo { 1 }
*Y::bar = \&X::foo;
*Y::bar = \&X::foo;	# second time for -w
eval 'package Z; sub Y::bar : foo';
like $@, qr/^X at /;

@attrs = eval 'attributes::get $anon1';
is "@attrs", "method";

sub Z::DESTROY { }
sub Z::FETCH_CODE_ATTRIBUTES { return 'Z' }
my $thunk = eval 'bless +sub : method { 1 }, "Z"';
is ref($thunk), "Z";

@attrs = eval 'attributes::get $thunk';
is "@attrs", "method Z";

# Test attributes on predeclared subroutines:
eval 'package A; sub PS : lvalue';
@attrs = eval 'attributes::get \&A::PS';
is "@attrs", "lvalue";

# Test attributes on predeclared subroutines, after definition
eval 'package A; sub PS : lvalue; sub PS { }';
@attrs = eval 'attributes::get \&A::PS';
is "@attrs", "lvalue";

# Test ability to modify existing sub's (or XSUB's) attributes.
eval 'package A; sub X { $_[0] } sub X : method';
@attrs = eval 'attributes::get \&A::X';
is "@attrs", "method";

# Above not with just 'pure' built-in attributes.
sub Z::MODIFY_CODE_ATTRIBUTES { (); }
eval 'package Z; sub L { $_[0] } sub L : Z method';
@attrs = eval 'attributes::get \&Z::L';
is "@attrs", "method Z";

# Begin testing attributes that tie

{
    package Ttie;
    sub DESTROY {}
    sub TIESCALAR { my $x = $_[1]; bless \$x, $_[0]; }
    sub FETCH { ${$_[0]} }
    sub STORE {
	::pass;
	${$_[0]} = $_[1]*2;
    }
    package Tloop;
    sub MODIFY_SCALAR_ATTRIBUTES { tie ${$_[1]}, 'Ttie', -1; (); }
}

eval_ok '
    package Tloop;
    for my $i (0..2) {
	my $x : TieLoop = $i;
	$x != $i*2 and ::is $x, $i*2;
    }
';

# bug #15898
eval 'our ${""} : foo = 1';
like $@, qr/Can't declare scalar dereference in "our"/;
eval 'my $$foo : bar = 1';
like $@, qr/Can't declare scalar dereference in "my"/;


my @code = qw(lvalue method);
my @other = qw(shared);
my @deprecated = qw(locked unique);
my %valid;
$valid{CODE} = {map {$_ => 1} @code};
$valid{SCALAR} = {map {$_ => 1} @other};
$valid{ARRAY} = $valid{HASH} = $valid{SCALAR};
my %deprecated;
$deprecated{CODE} = { locked => 1 };
$deprecated{ARRAY} = $deprecated{HASH} = $deprecated{SCALAR} = { unique => 1 };

our ($scalar, @array, %hash);
foreach my $value (\&foo, \$scalar, \@array, \%hash) {
    my $type = ref $value;
    foreach my $negate ('', '-') {
	foreach my $attr (@code, @other, @deprecated) {
	    my $attribute = $negate . $attr;
	    eval "use attributes __PACKAGE__, \$value, '$attribute'";
	    if ($deprecated{$type}{$attr}) {
		like $@, qr/^Attribute "$attr" is deprecated at \(eval \d+\)/,
		    "$type attribute $attribute deprecated";
	    } elsif ($valid{$type}{$attr}) {
		if ($attribute eq '-shared') {
		    like $@, qr/^A variable may not be unshared/;
		} else {
		    is( $@, '', "$type attribute $attribute");
		}
	    } else {
		like $@, qr/^Invalid $type attribute: $attribute/,
		    "Bogus $type attribute $attribute should fail";
	    }
	}
    }
}

# this will segfault if it fails
sub PVBM () { 'foo' }
{ my $dummy = index 'foo', PVBM }

ok !defined(eval 'attributes::get(\PVBM)'), 
    'PVBMs don\'t segfault attributes::get';

{
    #  [perl #49472] Attributes + Unknown Error
    eval '
	use strict;
	sub MODIFY_CODE_ATTRIBUTE{}
	sub f:Blah {$nosuchvar};
    ';

    my $err = $@;
    like ($err, qr/Global symbol "\$nosuchvar" requires /, 'perl #49472');
}

# Test that code attributes always get applied to the same CV that
# we're left with at the end (bug#66970).
{
	package bug66970;
	our $c;
	sub MODIFY_CODE_ATTRIBUTES { $c = $_[1]; () }
	$c=undef; eval 'sub t0 :Foo';
	main::ok $c == \&{"t0"};
	$c=undef; eval 'sub t1 :Foo { }';
	main::ok $c == \&{"t1"};
	$c=undef; eval 'sub t2';
	our $t2a = \&{"t2"};
	$c=undef; eval 'sub t2 :Foo';
	main::ok $c == \&{"t2"} && $c == $t2a;
	$c=undef; eval 'sub t3';
	our $t3a = \&{"t3"};
	$c=undef; eval 'sub t3 :Foo { }';
	main::ok $c == \&{"t3"} && $c == $t3a;
	$c=undef; eval 'sub t4 :Foo';
	our $t4a = \&{"t4"};
	our $t4b = $c;
	$c=undef; eval 'sub t4 :Foo';
	main::ok $c == \&{"t4"} && $c == $t4b && $c == $t4a;
	$c=undef; eval 'sub t5 :Foo';
	our $t5a = \&{"t5"};
	our $t5b = $c;
	$c=undef; eval 'sub t5 :Foo { }';
	main::ok $c == \&{"t5"} && $c == $t5b && $c == $t5a;
}

my @tests = grep {/^[^#]/} split /\n/, <<'EOT';
# This one is fine as an empty attribute list
my $holy_Einstein : = '';
# This one is deprecated
my $krunch := 4;
our $FWISK_FWISK_FWIZZACH_FWACH_ZACHITTY_ZICH_SHAZZATZ_FWISK := '';
state $thump := 'Trumpets';
# Lather rinse repeat in my usual obsessive style
my @holy_perfect_pitch : = ();
my @zok := ();
our @GUKGUK := ();
# state @widget_mark := ();
my %holy_seditives : = ();
my %bang := ();
our %GIGAZING := ();
# state %hex := ();
my $holy_giveaways : = '';
my $eee_yow := [];
our $TWOYYOYYOING_THUK_UGH := 1 == 1;
state $octothorn := 'Tinky Winky';
my @holy_Taj_Mahal : = ();
my @touche := ();
our @PLAK_DAK_THUK_FRIT := ();
# state @hash_mark := ();
my %holy_priceless_collection_of_Etruscan_snoods : = ();
my %wham_eth := ();
our %THWUK := ();
# state %octalthorpe := ();
my $holy_sewer_pipe : = '';
my $thunk := undef;
our $BLIT := time;
state $crunch := 'Laa Laa';
my @glurpp := ();
my @holy_harem : = ();
our @FABADAP := ();
# state @square := ();
my %holy_pin_cushions : = ();
my %swoosh := ();
our %RRRRR := ();
# state %scratchmark := ();
EOT

foreach my $test (@tests) {
    use feature 'state';
    eval $test;
    if ($test =~ /:=/) {
	like $@, qr/Use of := for an empty attribute list is not allowed/,
	    "Parse error for q{$test}";
    } else {
	is $@, '', "No error for q{$test}";
    }
}

# [perl #68560] Calling closure prototypes (only accessible via :attr)
{
  package brength;
  my $proto;
  sub MODIFY_CODE_ATTRIBUTES { $proto = $_[1]; _: }
  eval q{
     my $x;
     () = sub :a0 { $x };
  };
  package main;
  eval { $proto->() };               # used to crash in pp_entersub
  like $@, qr/^Closure prototype called/,
     "Calling closure proto with (no) args";
  eval { () = &$proto };             # used to crash in pp_leavesub
  like $@, qr/^Closure prototype called/,
     'Calling closure proto with no @_ that returns a lexical';
}

# Referencing closure prototypes
{
  package buckbuck;
  my @proto;
  sub MODIFY_CODE_ATTRIBUTES { push @proto, $_[1], \&{$_[1]}; _: }
  my $id;
  () = sub :buck {$id};
  &::is(@proto, 'referencing closure prototype');
}

# [perl #68658] Attributes on stately variables
{
  package thwext;
  sub MODIFY_SCALAR_ATTRIBUTES { () }
  my $i = 0;
  my $x_values = '';
  eval 'sub foo { use 5.01; state $x :A0 = $i++; $x_values .= $x }';
  foo(); foo();
  package main;
  is $x_values, '00', 'state with attributes';
}

{
  package ningnangnong;
  sub MODIFY_SCALAR_ATTRIBUTES{}
  sub MODIFY_ARRAY_ATTRIBUTES{  }
  sub MODIFY_HASH_ATTRIBUTES{    }
  my ($cows, @go, %bong) : teapots = qw[ jibber jabber joo ];
  ::is $cows, 'jibber', 'list assignment to scalar with attrs';
  ::is "@go", 'jabber joo', 'list assignment to array with attrs';
}

{
  my $w;
  local $SIG{__WARN__} = sub { $w = shift };
  sub  ent         {}
  sub lent :lvalue {}
  my $posmsg =
      'lvalue attribute applied to already-defined subroutine at '
     .'\(eval';
  my $negmsg =
      'lvalue attribute removed from already-defined subroutine at '
     .'\(eval';
  eval 'use attributes __PACKAGE__, \&ent, "lvalue"';
  like $w, qr/^$posmsg/, 'lvalue attr warning on def sub';
  is join("",&attributes::get(\&ent)), "lvalue",':lvalue applied anyway';
  $w = '';
  eval 'use attributes __PACKAGE__, \&lent, "lvalue"; 1' or die;
  is $w, "", 'no lvalue warning on def lvalue sub';
  eval 'use attributes __PACKAGE__, \&lent, "-lvalue"';
  like $w, qr/^$negmsg/, '-lvalue attr warning on def sub';
  is join("",&attributes::get(\&lent)), "",
       'lvalue attribute removed anyway';
  $w = '';
  eval 'use attributes __PACKAGE__, \&lent, "-lvalue"; 1' or die;
  is $w, "", 'no -lvalue warning on def non-lvalue sub';
  no warnings 'misc';
  eval 'use attributes __PACKAGE__, \&lent, "lvalue"';
  is $w, "", 'no lvalue warnings under no warnings misc';
  eval 'use attributes __PACKAGE__, \&ent, "-lvalue"';
  is $w, "", 'no -lvalue warnings under no warnings misc';
}

unlike runperl(
         prog => 'BEGIN {$^H{a}=b} sub foo:bar{1}',
         stderr => 1,
       ),
       qr/Unbalanced/,
      'attribute errors do not cause op trees to leak';

done_testing();
