#!./perl -w

# Regression tests for attributes.pm and the C< : attrs> syntax.

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

sub NTESTS () ;

my ($test, $ntests);
BEGIN {$ntests=0}
$test=0;
my $failed = 0;

print "1..".NTESTS."\n";

$SIG{__WARN__} = sub { die @_ };

sub mytest {
    my $bad = '';
    if (!$@ ne !$_[0] || $_[0] && $@ !~ $_[0]) {
	if ($@) {
	    my $x = $@;
	    $x =~ s/\n.*\z//s;
	    print "# Got: $x\n"
	}
	else {
	    print "# Got unexpected success\n";
	}
	if ($_[0]) {
	    print "# Expected: $_[0]\n";
	}
	else {
	    print "# Expected success\n";
	}
	$failed = 1;
	$bad = 'not ';
    }
    elsif (@_ == 3 && $_[1] ne $_[2]) {
	print "# Got: $_[1]\n";
	print "# Expected: $_[2]\n";
	$failed = 1;
	$bad = 'not ';
    }
    print $bad."ok ".++$test."\n";
}

eval 'sub t1 ($) : locked { $_[0]++ }';
mytest;
BEGIN {++$ntests}

eval 'sub t2 : locked { $_[0]++ }';
mytest;
BEGIN {++$ntests}

eval 'sub t3 ($) : locked ;';
mytest;
BEGIN {++$ntests}

eval 'sub t4 : locked ;';
mytest;
BEGIN {++$ntests}

my $anon1;
eval '$anon1 = sub ($) : locked:method { $_[0]++ }';
mytest;
BEGIN {++$ntests}

my $anon2;
eval '$anon2 = sub : locked : method { $_[0]++ }';
mytest;
BEGIN {++$ntests}

my $anon3;
eval '$anon3 = sub : method { $_[0]->[1] }';
mytest;
BEGIN {++$ntests}

eval 'sub e1 ($) : plugh ;';
mytest qr/^Invalid CODE attributes?: ["']?plugh["']? at/;
BEGIN {++$ntests}

eval 'sub e2 ($) : plugh(0,0) xyzzy ;';
mytest qr/^Invalid CODE attributes: ["']?plugh\(0,0\)["']? /;
BEGIN {++$ntests}

eval 'sub e3 ($) : plugh(0,0 xyzzy ;';
mytest qr/Unterminated attribute parameter in attribute list at/;
BEGIN {++$ntests}

eval 'sub e4 ($) : plugh + xyzzy ;';
mytest qr/Invalid separator character '[+]' in attribute list at/;
BEGIN {++$ntests}

eval 'my main $x : = 0;';
mytest;
BEGIN {++$ntests}

eval 'my $x : = 0;';
mytest;
BEGIN {++$ntests}

eval 'my $x ;';
mytest;
BEGIN {++$ntests}

eval 'my ($x) : = 0;';
mytest;
BEGIN {++$ntests}

eval 'my ($x) ;';
mytest;
BEGIN {++$ntests}

eval 'my ($x) : ;';
mytest;
BEGIN {++$ntests}

eval 'my ($x,$y) : = 0;';
mytest;
BEGIN {++$ntests}

eval 'my ($x,$y) ;';
mytest;
BEGIN {++$ntests}

eval 'my ($x,$y) : ;';
mytest;
BEGIN {++$ntests}

eval 'my ($x,$y) : plugh;';
mytest qr/^Invalid SCALAR attribute: ["']?plugh["']? at/;
BEGIN {++$ntests}

sub A::MODIFY_SCALAR_ATTRIBUTES { return }
eval 'my A $x : plugh;';
mytest qr/^SCALAR package attribute may clash with future reserved word: ["']?plugh["']? at/;
BEGIN {++$ntests}

eval 'my A $x : plugh plover;';
mytest qr/^SCALAR package attributes may clash with future reserved words: ["']?plugh["']? /;
BEGIN {++$ntests}

eval 'package Cat; my Cat @socks;';
mytest qr/^Can't declare class for non-scalar \@socks in "my"/;
BEGIN {++$ntests}

sub X::MODIFY_CODE_ATTRIBUTES { die "$_[0]" }
sub X::foo { 1 }
*Y::bar = \&X::foo;
*Y::bar = \&X::foo;	# second time for -w
eval 'package Z; sub Y::bar : foo';
mytest qr/^X at /;
BEGIN {++$ntests}

eval 'package Z; sub Y::baz : locked {}';
my @attrs = eval 'attributes::get \&Y::baz';
mytest '', "@attrs", "locked";
BEGIN {++$ntests}

@attrs = eval 'attributes::get $anon1';
mytest '', "@attrs", "locked method";
BEGIN {++$ntests}

sub Z::DESTROY { }
sub Z::FETCH_CODE_ATTRIBUTES { return 'Z' }
my $thunk = eval 'bless +sub : method locked { 1 }, "Z"';
mytest '', ref($thunk), "Z";
BEGIN {++$ntests}

@attrs = eval 'attributes::get $thunk';
mytest '', "@attrs", "locked method Z";
BEGIN {++$ntests}

# Test ability to modify existing sub's (or XSUB's) attributes.
eval 'package A; sub X { $_[0] } sub X : lvalue';
@attrs = eval 'attributes::get \&A::X';
mytest '', "@attrs", "lvalue";
BEGIN {++$ntests}

# Above not with just 'pure' built-in attributes.
sub Z::MODIFY_CODE_ATTRIBUTES { (); }
eval 'package Z; sub L { $_[0] } sub L : Z lvalue';
@attrs = eval 'attributes::get \&Z::L';
mytest '', "@attrs", "lvalue Z";
BEGIN {++$ntests}


# Begin testing attributes that tie

{
    package Ttie;
    sub DESTROY {}
    sub TIESCALAR { my $x = $_[1]; bless \$x, $_[0]; }
    sub FETCH { ${$_[0]} }
    sub STORE {
	#print "# In Ttie::STORE\n";
	::mytest '';
	${$_[0]} = $_[1]*2;
    }
    package Tloop;
    sub MODIFY_SCALAR_ATTRIBUTES { tie ${$_[1]}, 'Ttie', -1; (); }
}

eval '
    package Tloop;
    for my $i (0..2) {
	my $x : TieLoop = $i;
	$x != $i*2 and ::mytest "", $x, $i*2;
    }
';
mytest;
BEGIN {$ntests += 4}

# Other tests should be added above this line

sub NTESTS () { $ntests }

exit $failed;
