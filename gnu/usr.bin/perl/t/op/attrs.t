#!./perl

# Regression tests for attributes.pm and the C< : attrs> syntax.

BEGIN {
    if ($ENV{PERL_CORE_MINITEST}) {
	print "1..0 # skip: miniperl can't load attributes\n";
	exit 0;
    }
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

use warnings;

plan 92;

$SIG{__WARN__} = sub { die @_ };

sub eval_ok ($;$) {
    eval shift;
    is( $@, '', @_);
}

our $anon1; eval_ok '$anon1 = sub : method { $_[0]++ }';

eval 'sub e1 ($) : plugh ;';
like $@, qr/^Invalid CODE attributes?: ["']?plugh["']? at/;

eval 'sub e2 ($) : plugh(0,0) xyzzy ;';
like $@, qr/^Invalid CODE attributes: ["']?plugh\(0,0\)["']? /;

eval 'sub e3 ($) : plugh(0,0 xyzzy ;';
like $@, qr/Unterminated attribute parameter in attribute list at/;

eval 'sub e4 ($) : plugh + xyzzy ;';
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

ok !defined(attributes::get(\PVBM)), 
    'PVBMs don\'t segfault attributes::get';

{
    #  [perl #49472] Attributes + Unkown Error
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
