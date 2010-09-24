BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}
plan tests=>71;

sub a : lvalue { my $a = 34; ${\(bless \$a)} }  # Return a temporary
sub b : lvalue { ${\shift} }

my $out = a(b());		# Check that temporaries are allowed.
is(ref $out, 'main'); # Not reached if error.

my @out = grep /main/, a(b()); # Check that temporaries are allowed.
cmp_ok(scalar @out, '==', 1); # Not reached if error.

my $in;

# Check that we can return localized values from subroutines:

sub in : lvalue { $in = shift; }
sub neg : lvalue {  #(num_str) return num_str
    local $_ = shift;
    s/^\+/-/;
    $_;
}
in(neg("+2"));


is($in, '-2');

sub get_lex : lvalue { $in }
sub get_st : lvalue { $blah }
sub id : lvalue { ${\shift} }
sub id1 : lvalue { $_[0] }
sub inc : lvalue { ${\++$_[0]} }

$in = 5;
$blah = 3;

get_st = 7;

cmp_ok($blah, '==', 7);

get_lex = 7;

cmp_ok($in, '==', 7);

++get_st;

cmp_ok($blah, '==', 8);

++get_lex;

cmp_ok($in, '==', 8);

id(get_st) = 10;

cmp_ok($blah, '==', 10);

id(get_lex) = 10;

cmp_ok($in, '==', 10);

++id(get_st);

cmp_ok($blah, '==', 11);

++id(get_lex);

cmp_ok($in, '==', 11);

id1(get_st) = 20;

cmp_ok($blah, '==', 20);

id1(get_lex) = 20;

cmp_ok($in, '==', 20);

++id1(get_st);

cmp_ok($blah, '==', 21);

++id1(get_lex);

cmp_ok($in, '==', 21);

inc(get_st);

cmp_ok($blah, '==', 22);

inc(get_lex);

cmp_ok($in, '==', 22);

inc(id(get_st));

cmp_ok($blah, '==', 23);

inc(id(get_lex));

cmp_ok($in, '==', 23);

++inc(id1(id(get_st)));

cmp_ok($blah, '==', 25);

++inc(id1(id(get_lex)));

cmp_ok($in, '==', 25);

@a = (1) x 3;
@b = (undef) x 2;
$#c = 3;			# These slots are not fillable.

# Explanation: empty slots contain &sv_undef.

=for disabled constructs

sub a3 :lvalue {@a}
sub b2 : lvalue {@b}
sub c4: lvalue {@c}

$_ = '';

eval <<'EOE' or $_ = $@;
  ($x, a3, $y, b2, $z, c4, $t) = (34 .. 78);
  1;
EOE

#@out = ($x, a3, $y, b2, $z, c4, $t);
#@in = (34 .. 41, (undef) x 4, 46);
#print "# `@out' ne `@in'\nnot " unless "@out" eq "@in";

like($_, qr/Can\'t return an uninitialized value from lvalue subroutine/);
print "ok 22\n";

=cut


my $var;

sub a::var : lvalue { $var }

"a"->var = 45;

cmp_ok($var, '==', 45);

my $oo;
$o = bless \$oo, "a";

$o->var = 47;

cmp_ok($var, '==', 47);

sub o : lvalue { $o }

o->var = 49;

cmp_ok($var, '==', 49);

sub nolv () { $x0, $x1 } # Not lvalue

$_ = '';

eval <<'EOE' or $_ = $@;
  nolv = (2,3);
  1;
EOE

like($_, qr/Can\'t modify non-lvalue subroutine call in scalar assignment/);

$_ = '';

eval <<'EOE' or $_ = $@;
  nolv = (2,3) if $_;
  1;
EOE

like($_, qr/Can\'t modify non-lvalue subroutine call in scalar assignment/);

$_ = '';

eval <<'EOE' or $_ = $@;
  &nolv = (2,3) if $_;
  1;
EOE

like($_, qr/Can\'t modify non-lvalue subroutine call in scalar assignment/);

$x0 = $x1 = $_ = undef;
$nolv = \&nolv;

eval <<'EOE' or $_ = $@;
  $nolv->() = (2,3) if $_;
  1;
EOE

ok(!defined $_) or diag "'$_', '$x0', '$x1'";

$x0 = $x1 = $_ = undef;
$nolv = \&nolv;

eval <<'EOE' or $_ = $@;
  $nolv->() = (2,3);
  1;
EOE

like($_, qr/Can\'t modify non-lvalue subroutine call/)
  or diag "'$_', '$x0', '$x1'";

sub lv0 : lvalue { }		# Converted to lv10 in scalar context

$_ = undef;
eval <<'EOE' or $_ = $@;
  lv0 = (2,3);
  1;
EOE

like($_, qr/Can't return undef from lvalue subroutine/);

sub lv10 : lvalue {}

$_ = undef;
eval <<'EOE' or $_ = $@;
  (lv0) = (2,3);
  1;
EOE

ok(!defined $_) or diag $_;

sub lv1u :lvalue { undef }

$_ = undef;
eval <<'EOE' or $_ = $@;
  lv1u = (2,3);
  1;
EOE

like($_, qr/Can't return undef from lvalue subroutine/);

$_ = undef;
eval <<'EOE' or $_ = $@;
  (lv1u) = (2,3);
  1;
EOE

# Fixed by change @10777
#print "# '$_'.\nnot "
#  unless /Can\'t return an uninitialized value from lvalue subroutine/;
# print "ok 34 # Skip: removed test\n";

$x = '1234567';

$_ = undef;
eval <<'EOE' or $_ = $@;
  sub lv1t : lvalue { index $x, 2 }
  lv1t = (2,3);
  1;
EOE

like($_, qr/Can\'t modify index in lvalue subroutine return/);

$_ = undef;
eval <<'EOE' or $_ = $@;
  sub lv2t : lvalue { shift }
  (lv2t) = (2,3);
  1;
EOE

like($_, qr/Can\'t modify shift in lvalue subroutine return/);

$xxx = 'xxx';
sub xxx () { $xxx }  # Not lvalue

$_ = undef;
eval <<'EOE' or $_ = $@;
  sub lv1tmp : lvalue { xxx }			# is it a TEMP?
  lv1tmp = (2,3);
  1;
EOE

like($_, qr/Can\'t modify non-lvalue subroutine call in lvalue subroutine return/);

$_ = undef;
eval <<'EOE' or $_ = $@;
  (lv1tmp) = (2,3);
  1;
EOE

like($_, qr/Can\'t return a temporary from lvalue subroutine/);

sub yyy () { 'yyy' } # Const, not lvalue

$_ = undef;
eval <<'EOE' or $_ = $@;
  sub lv1tmpr : lvalue { yyy }			# is it read-only?
  lv1tmpr = (2,3);
  1;
EOE

like($_, qr/Can\'t modify constant item in lvalue subroutine return/);

$_ = undef;
eval <<'EOE' or $_ = $@;
  (lv1tmpr) = (2,3);
  1;
EOE

like($_, qr/Can\'t return a readonly value from lvalue subroutine/);

sub lva : lvalue {@a}

$_ = undef;
@a = ();
$a[1] = 12;
eval <<'EOE' or $_ = $@;
  (lva) = (2,3);
  1;
EOE

is("'@a' $_", "'2 3' ");

$_ = undef;
@a = ();
$a[0] = undef;
$a[1] = 12;
eval <<'EOE' or $_ = $@;
  (lva) = (2,3);
  1;
EOE

is("'@a' $_", "'2 3' ");

$_ = undef;
@a = ();
$a[0] = undef;
$a[1] = 12;
eval <<'EOE' or $_ = $@;
  (lva) = (2,3);
  1;
EOE

is("'@a' $_", "'2 3' ");

sub lv1n : lvalue { $newvar }

$_ = undef;
eval <<'EOE' or $_ = $@;
  lv1n = (3,4);
  1;
EOE

is("'$newvar' $_", "'4' ");

sub lv1nn : lvalue { $nnewvar }

$_ = undef;
eval <<'EOE' or $_ = $@;
  (lv1nn) = (3,4);
  1;
EOE

is("'$nnewvar' $_", "'3' ");

$a = \&lv1nn;
$a->() = 8;
is($nnewvar, '8');

eval 'sub AUTOLOAD : lvalue { $newvar }';
foobar() = 12;
is($newvar, "12");

{
my %hash; my @array;
sub alv : lvalue { $array[1] }
sub alv2 : lvalue { $array[$_[0]] }
sub hlv : lvalue { $hash{"foo"} }
sub hlv2 : lvalue { $hash{$_[0]} }
$array[1] = "not ok 51\n";
alv() = "ok 50\n";
is(alv(), "ok 50\n");

alv2(20) = "ok 51\n";
is($array[20], "ok 51\n");

$hash{"foo"} = "not ok 52\n";
hlv() = "ok 52\n";
is($hash{foo}, "ok 52\n");

$hash{bar} = "not ok 53\n";
hlv("bar") = "ok 53\n";
is(hlv("bar"), "ok 53\n");

sub array : lvalue  { @array  }
sub array2 : lvalue { @array2 } # This is a global.
sub hash : lvalue   { %hash   }
sub hash2 : lvalue  { %hash2  } # So's this.
@array2 = qw(foo bar);
%hash2 = qw(foo bar);

(array()) = qw(ok 54);
is("@array", "ok 54");

(array2()) = qw(ok 55);
is("@array2", "ok 55");

(hash()) = qw(ok 56);
cmp_ok($hash{ok}, '==', 56);

(hash2()) = qw(ok 57);
cmp_ok($hash2{ok}, '==', 57);

@array = qw(a b c d);
sub aslice1 : lvalue { @array[0,2] };
(aslice1()) = ("ok", "already");
is("@array", "ok b already d");

@array2 = qw(a B c d);
sub aslice2 : lvalue { @array2[0,2] };
(aslice2()) = ("ok", "already");
is("@array2", "ok B already d");

%hash = qw(a Alpha b Beta c Gamma);
sub hslice : lvalue { @hash{"c", "b"} }
(hslice()) = ("CISC", "BogoMIPS");
is(join("/",@hash{"c","a","b"}), "CISC/Alpha/BogoMIPS");
}

$str = "Hello, world!";
sub sstr : lvalue { substr($str, 1, 4) }
sstr() = "i";
is($str, "Hi, world!");

$str = "Made w/ JavaScript";
sub veclv : lvalue { vec($str, 2, 32) }
if (ord('A') != 193) {
    veclv() = 0x5065726C;
}
else { # EBCDIC?
    veclv() = 0xD7859993;
}
is($str, "Made w/ PerlScript");

sub position : lvalue { pos }
@p = ();
$_ = "fee fi fo fum";
while (/f/g) {
    push @p, position;
    position() += 6;
}
is("@p", "1 8");

# Bug 20001223.002: split thought that the list had only one element
@ary = qw(4 5 6);
sub lval1 : lvalue { $ary[0]; }
sub lval2 : lvalue { $ary[1]; }
(lval1(), lval2()) = split ' ', "1 2 3 4";

is(join(':', @ary), "1:2:6");

# check that an element of a tied hash/array can be assigned to via lvalueness

package Tie_Hash;

our ($key, $val);
sub TIEHASH { bless \my $v => __PACKAGE__ }
sub STORE   { ($key, $val) = @_[1,2] }

package main;
sub lval_tie_hash : lvalue {
    tie my %t => 'Tie_Hash';
    $t{key};
}

eval { lval_tie_hash() = "value"; };

is($@, "", "element of tied hash");

is("$Tie_Hash::key-$Tie_Hash::val", "key-value");


package Tie_Array;

our @val;
sub TIEARRAY { bless \my $v => __PACKAGE__ }
sub STORE   { $val[ $_[1] ] = $_[2] }

package main;
sub lval_tie_array : lvalue {
    tie my @t => 'Tie_Array';
    $t[0];
}

eval { lval_tie_array() = "value"; };


is($@, "", "element of tied array");

is ($Tie_Array::val[0], "value");

TODO: {
    local $TODO = 'test explicit return of lval expr';

    # subs are corrupted copies from tests 1-~4
    sub bad_get_lex : lvalue { return $in };
    sub bad_get_st  : lvalue { return $blah }

    sub bad_id  : lvalue { return ${\shift} }
    sub bad_id1 : lvalue { return $_[0] }
    sub bad_inc : lvalue { return ${\++$_[0]} }

    $in = 5;
    $blah = 3;

    bad_get_st = 7;

    is( $blah, 7 );

    bad_get_lex = 7;

    is($in, 7, "yada");

    ++bad_get_st;

    is($blah, 8, "yada");
}

TODO: {
    local $TODO = "bug #23790";
    my @arr  = qw /one two three/;
    my $line = "zero";
    sub lval_array () : lvalue {@arr}

    for (lval_array) {
        $line .= $_;
    }

    is($line, "zeroonetwothree");
}

{
    package Foo;
    sub AUTOLOAD :lvalue { *{$AUTOLOAD} };
    package main;
    my $foo = bless {},"Foo";
    my $result;
    $foo->bar = sub { $result = "bar" };
    $foo->bar;
    is ($result, 'bar', "RT #41550");
}

fresh_perl_is(<<'----', <<'====', "lvalue can not be set after definition. [perl #68758]");
use warnings;
our $x;
sub foo { $x }
sub foo : lvalue;
foo = 3;
----
lvalue attribute ignored after the subroutine has been defined at - line 4.
Can't modify non-lvalue subroutine call in scalar assignment at - line 5, near "3;"
Execution of - aborted due to compilation errors.
====

{
    my $x;
    sub lval_decl : lvalue;
    sub lval_decl { $x }
    lval_decl = 5;
    is($x, 5, "subroutine declared with lvalue before definition retains lvalue. [perl #68758]");
}
