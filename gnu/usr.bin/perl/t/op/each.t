#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

plan tests => 59;

$h{'abc'} = 'ABC';
$h{'def'} = 'DEF';
$h{'jkl','mno'} = "JKL\034MNO";
$h{'a',2,3,4,5} = join("\034",'A',2,3,4,5);
$h{'a'} = 'A';
$h{'b'} = 'B';
$h{'c'} = 'C';
$h{'d'} = 'D';
$h{'e'} = 'E';
$h{'f'} = 'F';
$h{'g'} = 'G';
$h{'h'} = 'H';
$h{'i'} = 'I';
$h{'j'} = 'J';
$h{'k'} = 'K';
$h{'l'} = 'L';
$h{'m'} = 'M';
$h{'n'} = 'N';
$h{'o'} = 'O';
$h{'p'} = 'P';
$h{'q'} = 'Q';
$h{'r'} = 'R';
$h{'s'} = 'S';
$h{'t'} = 'T';
$h{'u'} = 'U';
$h{'v'} = 'V';
$h{'w'} = 'W';
$h{'x'} = 'X';
$h{'y'} = 'Y';
$h{'z'} = 'Z';

@keys = keys %h;
@values = values %h;

is ($#keys, 29, "keys");
is ($#values, 29, "values");

$i = 0;		# stop -w complaints

while (($key,$value) = each(%h)) {
    if ($key eq $keys[$i] && $value eq $values[$i]
        && (('a' lt 'A' && $key lt $value) || $key gt $value)) {
	$key =~ y/a-z/A-Z/;
	$i++ if $key eq $value;
    }
}

is ($i, 30, "each count");

@keys = ('blurfl', keys(%h), 'dyick');
is ($#keys, 31, "added a key");

$size = ((split('/',scalar %h))[1]);
keys %h = $size * 5;
$newsize = ((split('/',scalar %h))[1]);
is ($newsize, $size * 8, "resize");
keys %h = 1;
$size = ((split('/',scalar %h))[1]);
is ($size, $newsize, "same size");
%h = (1,1);
$size = ((split('/',scalar %h))[1]);
is ($size, $newsize, "still same size");
undef %h;
%h = (1,1);
$size = ((split('/',scalar %h))[1]);
is ($size, 8, "size 8");

# test scalar each
%hash = 1..20;
$total = 0;
$total += $key while $key = each %hash;
is ($total, 100, "test scalar each");

for (1..3) { @foo = each %hash }
keys %hash;
$total = 0;
$total += $key while $key = each %hash;
is ($total, 100, "test scalar keys resets iterator");

for (1..3) { @foo = each %hash }
$total = 0;
$total += $key while $key = each %hash;
isnt ($total, 100, "test iterator of each is being maintained");

for (1..3) { @foo = each %hash }
values %hash;
$total = 0;
$total += $key while $key = each %hash;
is ($total, 100, "test values keys resets iterator");

$size = (split('/', scalar %hash))[1];
keys(%hash) = $size / 2;
is ($size, (split('/', scalar %hash))[1]);
keys(%hash) = $size + 100;
isnt ($size, (split('/', scalar %hash))[1]);

is (keys(%hash), 10, "keys (%hash)");

{
    no warnings 'deprecated';
    is (keys(hash), 10, "keys (hash)");
}

$i = 0;
%h = (a => A, b => B, c=> C, d => D, abc => ABC);
{
    no warnings 'deprecated';
    @keys = keys(h);
    @values = values(h);
    while (($key, $value) = each(h)) {
	if ($key eq $keys[$i] && $value eq $values[$i] && $key eq lc($value)) {
		$i++;
	}
    }
}
is ($i, 5);

@tests = (&next_test, &next_test, &next_test);
{
    package Obj;
    sub DESTROY { print "ok $::tests[1] # DESTROY called\n"; }
    {
	my $h = { A => bless [], __PACKAGE__ };
        while (my($k,$v) = each %$h) {
	    print "ok $::tests[0]\n" if $k eq 'A' and ref($v) eq 'Obj';
	}
    }
    print "ok $::tests[2]\n";
}

# Check for Unicode hash keys.
%u = ("\x{12}", "f", "\x{123}", "fo", "\x{1234}",  "foo");
$u{"\x{12345}"}  = "bar";
@u{"\x{10FFFD}"} = "zap";

my %u2;
foreach (keys %u) {
    is (length(), 1, "Check length of " . _qq $_);
    $u2{$_} = $u{$_};
}
ok (eq_hash(\%u, \%u2), "copied unicode hash keys correctly?");

$a = "\xe3\x81\x82"; $A = "\x{3042}";
%b = ( $a => "non-utf8");
%u = ( $A => "utf8");

is (exists $b{$A}, '', "utf8 key in bytes hash");
is (exists $u{$a}, '', "bytes key in utf8 hash");
print "# $b{$_}\n" for keys %b; # Used to core dump before change #8056.
pass ("if we got here change 8056 worked");
print "# $u{$_}\n" for keys %u; # Used to core dump before change #8056.
pass ("change 8056 is thanks to Inaba Hiroto");

# on EBCDIC chars are mapped differently so pick something that needs encoding
# there too.
$d = pack("U*", 0xe3, 0x81, 0xAF);
{ use bytes; $ol = bytes::length($d) }
cmp_ok ($ol, '>', 3, "check encoding on EBCDIC");
%u = ($d => "downgrade");
for (keys %u) {
    is (length, 3, "check length"); 
    is ($_, pack("U*", 0xe3, 0x81, 0xAF), "check value");
}
{
    { use bytes; is (bytes::length($d), $ol) }
}

{
    my %u;
    my $u0 = pack("U0U", 0x00FF);
    my $b0 = "\xC3\xBF";          # 0xCB 0xBF is U+00FF in UTF-8
    my $u1 = pack("U0U", 0x0100);
    my $b1 = "\xC4\x80";          # 0xC4 0x80 is U+0100 in UTF-8

    $u{$u0} = 1;
    $u{$b0} = 2; 
    $u{$u1} = 3;
    $u{$b1} = 4;

    is(scalar keys %u, 4, "four different Unicode keys"); 
    is($u{$u0}, 1, "U+00FF        -> 1");
    is($u{$b0}, 2, "U+00C3 U+00BF -> 2");
    is($u{$u1}, 3, "U+0100        -> 3 ");
    is($u{$b1}, 4, "U+00C4 U+0080 -> 4");
}

# test for syntax errors
for my $k (qw(each keys values)) {
    eval $k;
    like($@, qr/^Not enough arguments for $k/, "$k demands argument");
}

{
    my %foo=(1..10);
    my ($k,$v);
    my $count=keys %foo;
    my ($k1,$v1)=each(%foo);
    my $yes = 0;
    if (%foo) { $yes++ }
    my ($k2,$v2)=each(%foo);
    my $rest=0;
    while (each(%foo)) {$rest++};
    is($yes,1,"if(%foo) was true");
    isnt($k1,$k2,"if(%foo) didnt mess with each (key)");
    isnt($v1,$v2,"if(%foo) didnt mess with each (value)");
    is($rest,3,"Got the expect number of keys");
    my $hsv=1 && %foo;
    like($hsv,'/',"Got bucket stats from %foo in scalar assignment context");
    my @arr=%foo&&%foo;
    is(@arr,10,"Got expected number of elements in list context");
}    
{
    our %foo=(1..10);
    my ($k,$v);
    my $count=keys %foo;
    my ($k1,$v1)=each(%foo);
    my $yes = 0;
    if (%foo) { $yes++ }
    my ($k2,$v2)=each(%foo);
    my $rest=0;
    while (each(%foo)) {$rest++};
    is($yes,1,"if(%foo) was true");
    isnt($k1,$k2,"if(%foo) didnt mess with each (key)");
    isnt($v1,$v2,"if(%foo) didnt mess with each (value)");
    is($rest,3,"Got the expect number of keys");
    my $hsv=1 && %foo;
    like($hsv,'/',"Got bucket stats from %foo in scalar assignment context");
    my @arr=%foo&&%foo;
    is(@arr,10,"Got expected number of elements in list context");
}    
{
    # make sure a deleted active iterator gets freed timely, even if the
    # hash is otherwise empty

    package Single;

    my $c = 0;
    sub DESTROY { $c++ };

    {
	my %h = ("a" => bless []);
	my ($k,$v) = each %h;
	delete $h{$k};
	::is($c, 0, "single key not yet freed");
    }
    ::is($c, 1, "single key now freed");
}

{
    # Make sure each() does not leave the iterator in an inconsistent state
    # (RITER set to >= 0, with EITER null) if the active iterator is
    # deleted, leaving the hash apparently empty.
    my %h;
    $h{1} = 2;
    each %h;
    delete $h{1};
    each %h;
    $h{1}=2;
    is join ("-", each %h), '1-2',
	'each on apparently empty hash does not leave RITER set';
}
{
    my $warned= 0;
    local $SIG{__WARN__}= sub {
        /\QUse of each() on hash after insertion without resetting hash iterator results in undefined behavior\E/
            and $warned++ for @_;
    };
    my %h= map { $_ => $_ } "A".."F";
    while (my ($k, $v)= each %h) {
        $h{"$k$k"}= $v;
    }
    ok($warned,"each() after insert produces warnings");
    no warnings 'internal';
    $warned= 0;
    %h= map { $_ => $_ } "A".."F";
    while (my ($k, $v)= each %h) {
        $h{"$k$k"}= $v;
    }
    ok(!$warned, "no warnings 'internal' silences each() after insert warnings");
}
