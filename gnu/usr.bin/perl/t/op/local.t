#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = qw(. ../lib);
    require './test.pl';
}
plan tests => 296;

my $list_assignment_supported = 1;

#mg.c says list assignment not supported on VMS, EPOC, and SYMBIAN.
$list_assignment_supported = 0 if ($^O eq 'VMS');


sub foo {
    local($a, $b) = @_;
    local($c, $d);
    $c = "c 3";
    $d = "d 4";
    { local($a,$c) = ("a 9", "c 10"); ($x, $y) = ($a, $c); }
    is($a, "a 1");
    is($b, "b 2");
    $c, $d;
}

$a = "a 5";
$b = "b 6";
$c = "c 7";
$d = "d 8";

my @res;
@res =  &foo("a 1","b 2");
is($res[0], "c 3");
is($res[1], "d 4");

is($a, "a 5");
is($b, "b 6");
is($c, "c 7");
is($d, "d 8");
is($x, "a 9");
is($y, "c 10");

# same thing, only with arrays and associative arrays

sub foo2 {
    local($a, @b) = @_;
    local(@c, %d);
    @c = "c 3";
    $d{''} = "d 4";
    { local($a,@c) = ("a 19", "c 20"); ($x, $y) = ($a, @c); }
    is($a, "a 1");
    is("@b", "b 2");
    $c[0], $d{''};
}

$a = "a 5";
@b = "b 6";
@c = "c 7";
$d{''} = "d 8";

@res = &foo2("a 1","b 2");
is($res[0], "c 3");
is($res[1], "d 4");

is($a, "a 5");
is("@b", "b 6");
is($c[0], "c 7");
is($d{''}, "d 8");
is($x, "a 19");
is($y, "c 20");


eval 'local($$e)';
like($@, qr/Can't localize through a reference/);

eval '$e = []; local(@$e)';
like($@, qr/Can't localize through a reference/);

eval '$e = {}; local(%$e)';
like($@, qr/Can't localize through a reference/);

# Array and hash elements

@a = ('a', 'b', 'c');
{
    local($a[1]) = 'foo';
    local($a[2]) = $a[2];
    is($a[1], 'foo');
    is($a[2], 'c');
    undef @a;
}
is($a[1], 'b');
is($a[2], 'c');
ok(!defined $a[0]);

@a = ('a', 'b', 'c');
{
    local($a[4]) = 'x';
    ok(!defined $a[3]);
    is($a[4], 'x');
}
is(scalar(@a), 3);
ok(!exists $a[3]);
ok(!exists $a[4]);

@a = ('a', 'b', 'c');
{
    local($a[5]) = 'z';
    $a[4] = 'y';
    ok(!defined $a[3]);
    is($a[4], 'y');
    is($a[5], 'z');
}
is(scalar(@a), 5);
ok(!defined $a[3]);
is($a[4], 'y');
ok(!exists $a[5]);

@a = ('a', 'b', 'c');
{
    local(@a[4,6]) = ('x', 'z');
    ok(!defined $a[3]);
    is($a[4], 'x');
    ok(!defined $a[5]);
    is($a[6], 'z');
}
is(scalar(@a), 3);
ok(!exists $a[3]);
ok(!exists $a[4]);
ok(!exists $a[5]);
ok(!exists $a[6]);

@a = ('a', 'b', 'c');
{
    local(@a[4,6]) = ('x', 'z');
    $a[5] = 'y';
    ok(!defined $a[3]);
    is($a[4], 'x');
    is($a[5], 'y');
    is($a[6], 'z');
}
is(scalar(@a), 6);
ok(!defined $a[3]);
ok(!defined $a[4]);
is($a[5], 'y');
ok(!exists $a[6]);

@a = ('a', 'b', 'c');
{
    local($a[1]) = "X";
    shift @a;
}
is($a[0].$a[1], "Xb");
{
    my $d = "@a";
    local @a = @a;
    is("@a", $d);
}

@a = ('a', 'b', 'c');
$a[4] = 'd';
{
    delete local $a[1];
    is(scalar(@a), 5);
    is($a[0], 'a');
    ok(!exists($a[1]));
    is($a[2], 'c');
    ok(!exists($a[3]));
    is($a[4], 'd');

    ok(!exists($a[888]));
    delete local $a[888];
    is(scalar(@a), 5);
    ok(!exists($a[888]));

    ok(!exists($a[999]));
    my ($d, $zzz) = delete local @a[4, 999];
    is(scalar(@a), 3);
    ok(!exists($a[4]));
    ok(!exists($a[999]));
    is($d, 'd');
    is($zzz, undef);

    my $c = delete local $a[2];
    is(scalar(@a), 1);
    ok(!exists($a[2]));
    is($c, 'c');

    $a[888] = 'yyy';
    $a[999] = 'zzz';
}
is(scalar(@a), 5);
is($a[0], 'a');
is($a[1], 'b');
is($a[2], 'c');
ok(!defined($a[3]));
is($a[4], 'd');
ok(!exists($a[5]));
ok(!exists($a[888]));
ok(!exists($a[999]));

%h = (a => 1, b => 2, c => 3, d => 4);
{
    delete local $h{b};
    is(scalar(keys(%h)), 3);
    is($h{a}, 1);
    ok(!exists($h{b}));
    is($h{c}, 3);
    is($h{d}, 4);

    ok(!exists($h{yyy}));
    delete local $h{yyy};
    is(scalar(keys(%h)), 3);
    ok(!exists($h{yyy}));

    ok(!exists($h{zzz}));
    my ($d, $zzz) = delete local @h{qw/d zzz/};
    is(scalar(keys(%h)), 2);
    ok(!exists($h{d}));
    ok(!exists($h{zzz}));
    is($d, 4);
    is($zzz, undef);

    my $c = delete local $h{c};
    is(scalar(keys(%h)), 1);
    ok(!exists($h{c}));
    is($c, 3);

    $h{yyy} = 888;
    $h{zzz} = 999;
}
is(scalar(keys(%h)), 4);
is($h{a}, 1);
is($h{b}, 2);
is($h{c}, 3);
ok($h{d}, 4);
ok(!exists($h{yyy}));
ok(!exists($h{zzz}));

%h = ('a' => { 'b' => 1 }, 'c' => 2);
{
    my $a = delete local $h{a};
    is(scalar(keys(%h)), 1);
    ok(!exists($h{a}));
    is($h{c}, 2);
    is(scalar(keys(%$a)), 1);

    my $b = delete local $a->{b};
    is(scalar(keys(%$a)), 0);
    is($b, 1);

    $a->{d} = 3;
}
is(scalar(keys(%h)), 2);
{
    my $a = $h{a};
    is(scalar(keys(%$a)), 2);
    is($a->{b}, 1);
    is($a->{d}, 3);
}
is($h{c}, 2);

%h = ('a' => 1, 'b' => 2, 'c' => 3);
{
    local($h{'a'}) = 'foo';
    local($h{'b'}) = $h{'b'};
    is($h{'a'}, 'foo');
    is($h{'b'}, 2);
    local($h{'c'});
    delete $h{'c'};
}
is($h{'a'}, 1);
is($h{'b'}, 2);
{
    my $d = join("\n", map { "$_=>$h{$_}" } sort keys %h);
    local %h = %h;
    is(join("\n", map { "$_=>$h{$_}" } sort keys %h), $d);
}
is($h{'c'}, 3);

# check for scope leakage
$a = 'outer';
if (1) { local $a = 'inner' }
is($a, 'outer');

# see if localization works when scope unwinds
local $m = 5;
eval {
    for $m (6) {
	local $m = 7;
	die "bye";
    }
};
is($m, 5);

# see if localization works on tied arrays
{
    package TA;
    sub TIEARRAY { bless [], $_[0] }
    sub STORE { print "# STORE [@_]\n"; $_[0]->[$_[1]] = $_[2] }
    sub FETCH { my $v = $_[0]->[$_[1]]; print "# FETCH [@_=$v]\n"; $v }
    sub EXISTS { print "# EXISTS [@_]\n"; exists $_[0]->[$_[1]]; }
    sub DELETE { print "# DELETE [@_]\n"; delete $_[0]->[$_[1]]; }
    sub CLEAR { print "# CLEAR [@_]\n"; @{$_[0]} = (); }
    sub FETCHSIZE { scalar(@{$_[0]}) }
    sub SHIFT { shift (@{$_[0]}) }
    sub EXTEND {}
}

tie @a, 'TA';
@a = ('a', 'b', 'c');
{
    local($a[1]) = 'foo';
    local($a[2]) = $a[2];
    is($a[1], 'foo');
    is($a[2], 'c');
    @a = ();
}
is($a[1], 'b');
is($a[2], 'c');
ok(!defined $a[0]);
{
    my $d = "@a";
    local @a = @a;
    is("@a", $d);
}

# local() should preserve the existenceness of tied array elements
@a = ('a', 'b', 'c');
{
    local($a[4]) = 'x';
    ok(!defined $a[3]);
    is($a[4], 'x');
}
is(scalar(@a), 3);
ok(!exists $a[3]);
ok(!exists $a[4]);

@a = ('a', 'b', 'c');
{
    local($a[5]) = 'z';
    $a[4] = 'y';
    ok(!defined $a[3]);
    is($a[4], 'y');
    is($a[5], 'z');
}
is(scalar(@a), 5);
ok(!defined $a[3]);
is($a[4], 'y');
ok(!exists $a[5]);

@a = ('a', 'b', 'c');
{
    local(@a[4,6]) = ('x', 'z');
    ok(!defined $a[3]);
    is($a[4], 'x');
    ok(!defined $a[5]);
    is($a[6], 'z');
}
is(scalar(@a), 3);
ok(!exists $a[3]);
ok(!exists $a[4]);
ok(!exists $a[5]);
ok(!exists $a[6]);

@a = ('a', 'b', 'c');
{
    local(@a[4,6]) = ('x', 'z');
    $a[5] = 'y';
    ok(!defined $a[3]);
    is($a[4], 'x');
    is($a[5], 'y');
    is($a[6], 'z');
}
is(scalar(@a), 6);
ok(!defined $a[3]);
ok(!defined $a[4]);
is($a[5], 'y');
ok(!exists $a[6]);

@a = ('a', 'b', 'c');
$a[4] = 'd';
{
    delete local $a[1];
    is(scalar(@a), 5);
    is($a[0], 'a');
    ok(!exists($a[1]));
    is($a[2], 'c');
    ok(!exists($a[3]));
    is($a[4], 'd');

    ok(!exists($a[888]));
    delete local $a[888];
    is(scalar(@a), 5);
    ok(!exists($a[888]));

    ok(!exists($a[999]));
    my ($d, $zzz) = delete local @a[4, 999];
    is(scalar(@a), 3);
    ok(!exists($a[4]));
    ok(!exists($a[999]));
    is($d, 'd');
    is($zzz, undef);

    my $c = delete local $a[2];
    is(scalar(@a), 1);
    ok(!exists($a[2]));
    is($c, 'c');

    $a[888] = 'yyy';
    $a[999] = 'zzz';
}
is(scalar(@a), 5);
is($a[0], 'a');
is($a[1], 'b');
is($a[2], 'c');
ok(!defined($a[3]));
is($a[4], 'd');
ok(!exists($a[5]));
ok(!exists($a[888]));
ok(!exists($a[999]));

# see if localization works on tied hashes
{
    package TH;
    sub TIEHASH { bless {}, $_[0] }
    sub STORE { print "# STORE [@_]\n"; $_[0]->{$_[1]} = $_[2] }
    sub FETCH { my $v = $_[0]->{$_[1]}; print "# FETCH [@_=$v]\n"; $v }
    sub EXISTS { print "# EXISTS [@_]\n"; exists $_[0]->{$_[1]}; }
    sub DELETE { print "# DELETE [@_]\n"; delete $_[0]->{$_[1]}; }
    sub CLEAR { print "# CLEAR [@_]\n"; %{$_[0]} = (); }
    sub FIRSTKEY { print "# FIRSTKEY [@_]\n"; keys %{$_[0]}; each %{$_[0]} }
    sub NEXTKEY { print "# NEXTKEY [@_]\n"; each %{$_[0]} }
}

tie %h, 'TH';
%h = ('a' => 1, 'b' => 2, 'c' => 3);

{
    local($h{'a'}) = 'foo';
    local($h{'b'}) = $h{'b'};
    local($h{'y'});
    local($h{'z'}) = 33;
    is($h{'a'}, 'foo');
    is($h{'b'}, 2);
    local($h{'c'});
    delete $h{'c'};
}
is($h{'a'}, 1);
is($h{'b'}, 2);
is($h{'c'}, 3);
# local() should preserve the existenceness of tied hash elements
ok(! exists $h{'y'});
ok(! exists $h{'z'});
TODO: {
    todo_skip("Localize entire tied hash");
    my $d = join("\n", map { "$_=>$h{$_}" } sort keys %h);
    local %h = %h;
    is(join("\n", map { "$_=>$h{$_}" } sort keys %h), $d);
}

%h = (a => 1, b => 2, c => 3, d => 4);
{
    delete local $h{b};
    is(scalar(keys(%h)), 3);
    is($h{a}, 1);
    ok(!exists($h{b}));
    is($h{c}, 3);
    is($h{d}, 4);

    ok(!exists($h{yyy}));
    delete local $h{yyy};
    is(scalar(keys(%h)), 3);
    ok(!exists($h{yyy}));

    ok(!exists($h{zzz}));
    my ($d, $zzz) = delete local @h{qw/d zzz/};
    is(scalar(keys(%h)), 2);
    ok(!exists($h{d}));
    ok(!exists($h{zzz}));
    is($d, 4);
    is($zzz, undef);

    my $c = delete local $h{c};
    is(scalar(keys(%h)), 1);
    ok(!exists($h{c}));
    is($c, 3);

    $h{yyy} = 888;
    $h{zzz} = 999;
}
is(scalar(keys(%h)), 4);
is($h{a}, 1);
is($h{b}, 2);
is($h{c}, 3);
ok($h{d}, 4);
ok(!exists($h{yyy}));
ok(!exists($h{zzz}));

@a = ('a', 'b', 'c');
{
    local($a[1]) = "X";
    shift @a;
}
is($a[0].$a[1], "Xb");

# now try the same for %SIG

$SIG{TERM} = 'foo';
$SIG{INT} = \&foo;
$SIG{__WARN__} = $SIG{INT};
{
    local($SIG{TERM}) = $SIG{TERM};
    local($SIG{INT}) = $SIG{INT};
    local($SIG{__WARN__}) = $SIG{__WARN__};
    is($SIG{TERM}, 'main::foo');
    is($SIG{INT}, \&foo);
    is($SIG{__WARN__}, \&foo);
    local($SIG{INT});
    delete $SIG{__WARN__};
}
is($SIG{TERM}, 'main::foo');
is($SIG{INT}, \&foo);
is($SIG{__WARN__}, \&foo);
{
    my $d = join("\n", map { "$_=>$SIG{$_}" } sort keys %SIG);
    local %SIG = %SIG;
    is(join("\n", map { "$_=>$SIG{$_}" } sort keys %SIG), $d);
}

# and for %ENV

$ENV{_X_} = 'a';
$ENV{_Y_} = 'b';
$ENV{_Z_} = 'c';
{
    local($ENV{_A_});
    local($ENV{_B_}) = 'foo';
    local($ENV{_X_}) = 'foo';
    local($ENV{_Y_}) = $ENV{_Y_};
    is($ENV{_X_}, 'foo');
    is($ENV{_Y_}, 'b');
    local($ENV{_Z_});
    delete $ENV{_Z_};
}
is($ENV{_X_}, 'a');
is($ENV{_Y_}, 'b');
is($ENV{_Z_}, 'c');
# local() should preserve the existenceness of %ENV elements
ok(! exists $ENV{_A_});
ok(! exists $ENV{_B_});

SKIP: {
    skip("Can't make list assignment to \%ENV on this system")
	unless $list_assignment_supported;
    my $d = join("\n", map { "$_=>$ENV{$_}" } sort keys %ENV);
    local %ENV = %ENV;
    is(join("\n", map { "$_=>$ENV{$_}" } sort keys %ENV), $d);
}

# does implicit localization in foreach skip magic?

$_ = "o 0,o 1,";
my $iter = 0;
while (/(o.+?),/gc) {
    is($1, "o $iter");
    foreach (1..1) { $iter++ }
    if ($iter > 2) { fail("endless loop"); last; }
}

{
    package UnderScore;
    sub TIESCALAR { bless \my $self, shift }
    sub FETCH { die "read  \$_ forbidden" }
    sub STORE { die "write \$_ forbidden" }
    tie $_, __PACKAGE__;
    my @tests = (
	"Nesting"     => sub { print '#'; for (1..3) { print }
			       print "\n" },			1,
	"Reading"     => sub { print },				0,
	"Matching"    => sub { $x = /badness/ },		0,
	"Concat"      => sub { $_ .= "a" },			0,
	"Chop"        => sub { chop },				0,
	"Filetest"    => sub { -x },				0,
	"Assignment"  => sub { $_ = "Bad" },			0,
	# XXX whether next one should fail is debatable
	"Local \$_"   => sub { local $_  = 'ok?'; print },	0,
	"for local"   => sub { for("#ok?\n"){ print } },	1,
    );
    while ( ($name, $code, $ok) = splice(@tests, 0, 3) ) {
	eval { &$code };
        main::ok(($ok xor $@), "Underscore '$name'");
    }
    untie $_;
}

{
    # BUG 20001205.22
    my %x;
    $x{a} = 1;
    { local $x{b} = 1; }
    ok(! exists $x{b});
    { local @x{c,d,e}; }
    ok(! exists $x{c});
}

# local() and readonly magic variables

eval { local $1 = 1 };
like($@, qr/Modification of a read-only value attempted/);

eval { for ($1) { local $_ = 1 } };
like($@, qr/Modification of a read-only value attempted/);

# make sure $1 is still read-only
eval { for ($1) { local $_ = 1 } };
like($@, qr/Modification of a read-only value attempted/);

# The s/// adds 'g' magic to $_, but it should remain non-readonly
eval { for("a") { for $x (1,2) { local $_="b"; s/(.*)/+$1/ } } };
is($@, "");

# RT #4342 Special local() behavior for $[
{
    no warnings 'deprecated';
    local $[ = 1;
    ok(1 == $[, 'lexcical scope of local $[');
    f();
}

sub f { ok(0 == $[); }

# sub localisation
{
	package Other;

	sub f1 { "f1" }
	sub f2 { "f2" }

	no warnings "redefine";
	{
		local *f1 = sub  { "g1" };
		::ok(f1() eq "g1", "localised sub via glob");
	}
	::ok(f1() eq "f1", "localised sub restored");
	{
		local $Other::{"f1"} = sub { "h1" };
		::ok(f1() eq "h1", "localised sub via stash");
	}
	::ok(f1() eq "f1", "localised sub restored");
	{
		local @Other::{qw/ f1 f2 /} = (sub { "j1" }, sub { "j2" });
		::ok(f1() eq "j1", "localised sub via stash slice");
		::ok(f2() eq "j2", "localised sub via stash slice");
	}
	::ok(f1() eq "f1", "localised sub restored");
	::ok(f2() eq "f2", "localised sub restored");
}

# Localising unicode keys (bug #38815)
{
    my %h;
    $h{"\243"} = "pound";
    $h{"\302\240"} = "octects";
    is(scalar keys %h, 2);
    {
	my $unicode = chr 256;
	my $ambigous = "\240" . $unicode;
	chop $ambigous;
	local $h{$unicode} = 256;
	local $h{$ambigous} = 160;

	is(scalar keys %h, 4);
	is($h{"\243"}, "pound");
	is($h{$unicode}, 256);
	is($h{$ambigous}, 160);
	is($h{"\302\240"}, "octects");
    }
    is(scalar keys %h, 2);
    is($h{"\243"}, "pound");
    is($h{"\302\240"}, "octects");
}

# And with slices
{
    my %h;
    $h{"\243"} = "pound";
    $h{"\302\240"} = "octects";
    is(scalar keys %h, 2);
    {
	my $unicode = chr 256;
	my $ambigous = "\240" . $unicode;
	chop $ambigous;
	local @h{$unicode, $ambigous} = (256, 160);

	is(scalar keys %h, 4);
	is($h{"\243"}, "pound");
	is($h{$unicode}, 256);
	is($h{$ambigous}, 160);
	is($h{"\302\240"}, "octects");
    }
    is(scalar keys %h, 2);
    is($h{"\243"}, "pound");
    is($h{"\302\240"}, "octects");
}

# [perl #39012] localizing @_ element then shifting frees element too # soon

{
    my $x;
    my $y = bless [], 'X39012';
    sub X39012::DESTROY { $x++ }
    sub { local $_[0]; shift }->($y);
    ok(!$x,  '[perl #39012]');
    
}

# when localising a hash element, the key should be copied, not referenced

{
    my %h=('k1' => 111);
    my $k='k1';
    {
	local $h{$k}=222;

	is($h{'k1'},222);
	$k='k2';
    }
    ok(! exists($h{'k2'}));
    is($h{'k1'},111);
}
{
    my %h=('k1' => 111);
    our $k = 'k1';  # try dynamic too
    {
	local $h{$k}=222;
	is($h{'k1'},222);
	$k='k2';
    }
    ok(! exists($h{'k2'}));
    is($h{'k1'},111);
}

like( runperl(stderr => 1,
              prog => 'use constant foo => q(a);' .
                      'index(q(a), foo);' .
                      'local *g=${::}{foo};print q(ok);'), "ok", "[perl #52740]");

# Keep this test last, as it can SEGV
{
    local *@;
    pass("Localised *@");
    eval {1};
    pass("Can eval with *@ localised");
}

