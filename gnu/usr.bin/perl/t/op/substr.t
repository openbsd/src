#!./perl -w

#P = start of string  Q = start of substr  R = end of substr  S = end of string

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}
use warnings ;

$a = 'abcdefxyz';
$SIG{__WARN__} = sub {
     if ($_[0] =~ /^substr outside of string/) {
          $w++;
     } elsif ($_[0] =~ /^Attempt to use reference as lvalue in substr/) {
          $w += 2;
     } elsif ($_[0] =~ /^Use of uninitialized value/) {
          $w += 3;
     } else {
          warn $_[0];
     }
};

require './test.pl';

plan(325);

$FATAL_MSG = qr/^substr outside of string/;

is(substr($a,0,3), 'abc');   # P=Q R S
is(substr($a,3,3), 'def');   # P Q R S
is(substr($a,6,999), 'xyz'); # P Q S R
$b = substr($a,999,999) ; # warn # P R Q S
is ($w--, 1);
eval{substr($a,999,999) = "" ; };# P R Q S
like ($@, $FATAL_MSG);
is(substr($a,0,-6), 'abc');  # P=Q R S
is(substr($a,-3,1), 'x');    # P Q R S

$[ = 1;

is(substr($a,1,3), 'abc' );  # P=Q R S
is(substr($a,4,3), 'def' );  # P Q R S
is(substr($a,7,999), 'xyz');# P Q S R
$b = substr($a,999,999) ; # warn # P R Q S
is($w--, 1);
eval{substr($a,999,999) = "" ; } ; # P R Q S
like ($@, $FATAL_MSG);
is(substr($a,1,-6), 'abc' );# P=Q R S
is(substr($a,-3,1), 'x' );  # P Q R S

$[ = 0;

substr($a,3,3) = 'XYZ';
is($a, 'abcXYZxyz' );
substr($a,0,2) = '';
is($a, 'cXYZxyz' );
substr($a,0,0) = 'ab';
is($a, 'abcXYZxyz' );
substr($a,0,0) = '12345678';
is($a, '12345678abcXYZxyz' );
substr($a,-3,3) = 'def';
is($a, '12345678abcXYZdef');
substr($a,-3,3) = '<';
is($a, '12345678abcXYZ<' );
substr($a,-1,1) = '12345678';
is($a, '12345678abcXYZ12345678' );

$a = 'abcdefxyz';

is(substr($a,6), 'xyz' );        # P Q R=S
is(substr($a,-3), 'xyz' );       # P Q R=S
$b = substr($a,999,999) ; # warning   # P R=S Q
is($w--, 1);
eval{substr($a,999,999) = "" ; } ;    # P R=S Q
like($@, $FATAL_MSG);
is(substr($a,0), 'abcdefxyz');  # P=Q R=S
is(substr($a,9), '');           # P Q=R=S
is(substr($a,-11), 'abcdefxyz'); # Q P R=S
is(substr($a,-9), 'abcdefxyz');  # P=Q R=S

$a = '54321';

$b = substr($a,-7, 1) ; # warn  # Q R P S
is($w--, 1);
eval{substr($a,-7, 1) = "" ; }; # Q R P S
like($@, $FATAL_MSG);
$b = substr($a,-7,-6) ; # warn  # Q R P S
is($w--, 1);
eval{substr($a,-7,-6) = "" ; }; # Q R P S
like($@, $FATAL_MSG);
is(substr($a,-5,-7), '');  # R P=Q S
is(substr($a, 2,-7), '');  # R P Q S
is(substr($a,-3,-7), '');  # R P Q S
is(substr($a, 2,-5), '');  # P=R Q S
is(substr($a,-3,-5), '');  # P=R Q S
is(substr($a, 2,-4), '');  # P R Q S
is(substr($a,-3,-4), '');  # P R Q S
is(substr($a, 5,-6), '');  # R P Q=S
is(substr($a, 5,-5), '');  # P=R Q S
is(substr($a, 5,-3), '');  # P R Q=S
$b = substr($a, 7,-7) ; # warn  # R P S Q
is($w--, 1);
eval{substr($a, 7,-7) = "" ; }; # R P S Q
like($@, $FATAL_MSG);
$b = substr($a, 7,-5) ; # warn  # P=R S Q
is($w--, 1);
eval{substr($a, 7,-5) = "" ; }; # P=R S Q
like($@, $FATAL_MSG);
$b = substr($a, 7,-3) ; # warn  # P Q S Q
is($w--, 1);
eval{substr($a, 7,-3) = "" ; }; # P Q S Q
like($@, $FATAL_MSG);
$b = substr($a, 7, 0) ; # warn  # P S Q=R
is($w--, 1);
eval{substr($a, 7, 0) = "" ; }; # P S Q=R
like($@, $FATAL_MSG);

is(substr($a,-7,2), '');   # Q P=R S
is(substr($a,-7,4), '54'); # Q P R S
is(substr($a,-7,7), '54321');# Q P R=S
is(substr($a,-7,9), '54321');# Q P S R
is(substr($a,-5,0), '');   # P=Q=R S
is(substr($a,-5,3), '543');# P=Q R S
is(substr($a,-5,5), '54321');# P=Q R=S
is(substr($a,-5,7), '54321');# P=Q S R
is(substr($a,-3,0), '');   # P Q=R S
is(substr($a,-3,3), '321');# P Q R=S
is(substr($a,-2,3), '21'); # P Q S R
is(substr($a,0,-5), '');   # P=Q=R S
is(substr($a,2,-3), '');   # P Q=R S
is(substr($a,0,0), '');    # P=Q=R S
is(substr($a,0,5), '54321');# P=Q R=S
is(substr($a,0,7), '54321');# P=Q S R
is(substr($a,2,0), '');    # P Q=R S
is(substr($a,2,3), '321'); # P Q R=S
is(substr($a,5,0), '');    # P Q=R=S
is(substr($a,5,2), '');    # P Q=S R
is(substr($a,-7,-5), '');  # Q P=R S
is(substr($a,-7,-2), '543');# Q P R S
is(substr($a,-5,-5), '');  # P=Q=R S
is(substr($a,-5,-2), '543');# P=Q R S
is(substr($a,-3,-3), '');  # P Q=R S
is(substr($a,-3,-1), '32');# P Q R S

$a = '';

is(substr($a,-2,2), '');   # Q P=R=S
is(substr($a,0,0), '');    # P=Q=R=S
is(substr($a,0,1), '');    # P=Q=S R
is(substr($a,-2,3), '');   # Q P=S R
is(substr($a,-2), '');     # Q P=R=S
is(substr($a,0), '');      # P=Q=R=S


is(substr($a,0,-1), '');   # R P=Q=S
$b = substr($a,-2, 0) ; # warn  # Q=R P=S
is($w--, 1);
eval{substr($a,-2, 0) = "" ; }; # Q=R P=S
like($@, $FATAL_MSG);

$b = substr($a,-2, 1) ; # warn  # Q R P=S
is($w--, 1);
eval{substr($a,-2, 1) = "" ; }; # Q R P=S
like($@, $FATAL_MSG);

$b = substr($a,-2,-1) ; # warn  # Q R P=S
is($w--, 1);
eval{substr($a,-2,-1) = "" ; }; # Q R P=S
like($@, $FATAL_MSG);

$b = substr($a,-2,-2) ; # warn  # Q=R P=S
is($w--, 1);
eval{substr($a,-2,-2) = "" ; }; # Q=R P=S
like($@, $FATAL_MSG);

$b = substr($a, 1,-2) ; # warn  # R P=S Q
is($w--, 1);
eval{substr($a, 1,-2) = "" ; }; # R P=S Q
like($@, $FATAL_MSG);

$b = substr($a, 1, 1) ; # warn  # P=S Q R
is($w--, 1);
eval{substr($a, 1, 1) = "" ; }; # P=S Q R
like($@, $FATAL_MSG);

$b = substr($a, 1, 0) ;# warn   # P=S Q=R
is($w--, 1);
eval{substr($a, 1, 0) = "" ; }; # P=S Q=R
like($@, $FATAL_MSG);

$b = substr($a,1) ; # warning   # P=R=S Q
is($w--, 1);
eval{substr($a,1) = "" ; };     # P=R=S Q
like($@, $FATAL_MSG);

my $a = 'zxcvbnm';
substr($a,2,0) = '';
is($a, 'zxcvbnm');
substr($a,7,0) = '';
is($a, 'zxcvbnm');
substr($a,5,0) = '';
is($a, 'zxcvbnm');
substr($a,0,2) = 'pq';
is($a, 'pqcvbnm');
substr($a,2,0) = 'r';
is($a, 'pqrcvbnm');
substr($a,8,0) = 'asd';
is($a, 'pqrcvbnmasd');
substr($a,0,2) = 'iop';
is($a, 'ioprcvbnmasd');
substr($a,0,5) = 'fgh';
is($a, 'fghvbnmasd');
substr($a,3,5) = 'jkl';
is($a, 'fghjklsd');
substr($a,3,2) = '1234';
is($a, 'fgh1234lsd');


# with lexicals (and in re-entered scopes)
for (0,1) {
  my $txt;
  unless ($_) {
    $txt = "Foo";
    substr($txt, -1) = "X";
    is($txt, "FoX");
  }
  else {
    substr($txt, 0, 1) = "X";
    is($txt, "X");
  }
}

$w = 0 ;
# coercion of references
{
  my $s = [];
  substr($s, 0, 1) = 'Foo';
  is (substr($s,0,7), "FooRRAY");
  is ($w,2);
  $w = 0;
}

# check no spurious warnings
is($w, 0);

# check new 4 arg replacement syntax
$a = "abcxyz";
$w = 0;
is(substr($a, 0, 3, ""), "abc");
is($a, "xyz");
is(substr($a, 0, 0, "abc"), "");
is($a, "abcxyz");
is(substr($a, 3, -1, ""), "xy");
is($a, "abcz");

is(substr($a, 3, undef, "xy"), "");
is($a, "abcxyz");
is($w, 3);

$w = 0;

is(substr($a, 3, 9999999, ""), "xyz");
is($a, "abc");
eval{substr($a, -99, 0, "") };
like($@, $FATAL_MSG);
eval{substr($a, 99, 3, "") };
like($@, $FATAL_MSG);

substr($a, 0, length($a), "foo");
is ($a, "foo");
is ($w, 0);

# using 4 arg substr as lvalue is a compile time error
eval 'substr($a,0,0,"") = "abc"';
like ($@, qr/Can't modify substr/);
is ($a, "foo");

$a = "abcdefgh";
is(sub { shift }->(substr($a, 0, 4, "xxxx")), 'abcd');
is($a, 'xxxxefgh');

{
    my $y = 10;
    $y = "2" . $y;
    is ($y, 210);
}

# utf8 sanity
{
    my $x = substr("a\x{263a}b",0);
    is(length($x), 3);
    $x = substr($x,1,1);
    is($x, "\x{263a}");
    $x = $x x 2;
    is(length($x), 2);
    substr($x,0,1) = "abcd";
    is($x, "abcd\x{263a}");
    is(length($x), 5);
    $x = reverse $x;
    is(length($x), 5);
    is($x, "\x{263a}dcba");

    my $z = 10;
    $z = "21\x{263a}" . $z;
    is(length($z), 5);
    is($z, "21\x{263a}10");
}

# replacement should work on magical values
require Tie::Scalar;
my %data;
tie $data{'a'}, 'Tie::StdScalar';  # makes $data{'a'} magical
$data{a} = "firstlast";
is(substr($data{'a'}, 0, 5, ""), "first");
is($data{'a'}, "last");

# more utf8

# The following two originally from Ignasi Roca.

$x = "\xF1\xF2\xF3";
substr($x, 0, 1) = "\x{100}"; # Ignasi had \x{FF}
is(length($x), 3);
is($x, "\x{100}\xF2\xF3");
is(substr($x, 0, 1), "\x{100}");
is(substr($x, 1, 1), "\x{F2}");
is(substr($x, 2, 1), "\x{F3}");

$x = "\xF1\xF2\xF3";
substr($x, 0, 1) = "\x{100}\x{FF}"; # Ignasi had \x{FF}
is(length($x), 4);
is($x, "\x{100}\x{FF}\xF2\xF3");
is(substr($x, 0, 1), "\x{100}");
is(substr($x, 1, 1), "\x{FF}");
is(substr($x, 2, 1), "\x{F2}");
is(substr($x, 3, 1), "\x{F3}");

# more utf8 lval exercise

$x = "\xF1\xF2\xF3";
substr($x, 0, 2) = "\x{100}\xFF";
is(length($x), 3);
is($x, "\x{100}\xFF\xF3");
is(substr($x, 0, 1), "\x{100}");
is(substr($x, 1, 1), "\x{FF}");
is(substr($x, 2, 1), "\x{F3}");

$x = "\xF1\xF2\xF3";
substr($x, 1, 1) = "\x{100}\xFF";
is(length($x), 4);
is($x, "\xF1\x{100}\xFF\xF3");
is(substr($x, 0, 1), "\x{F1}");
is(substr($x, 1, 1), "\x{100}");
is(substr($x, 2, 1), "\x{FF}");
is(substr($x, 3, 1), "\x{F3}");

$x = "\xF1\xF2\xF3";
substr($x, 2, 1) = "\x{100}\xFF";
is(length($x), 4);
is($x, "\xF1\xF2\x{100}\xFF");
is(substr($x, 0, 1), "\x{F1}");
is(substr($x, 1, 1), "\x{F2}");
is(substr($x, 2, 1), "\x{100}");
is(substr($x, 3, 1), "\x{FF}");

$x = "\xF1\xF2\xF3";
substr($x, 3, 1) = "\x{100}\xFF";
is(length($x), 5);
is($x, "\xF1\xF2\xF3\x{100}\xFF");
is(substr($x, 0, 1), "\x{F1}");
is(substr($x, 1, 1), "\x{F2}");
is(substr($x, 2, 1), "\x{F3}");
is(substr($x, 3, 1), "\x{100}");
is(substr($x, 4, 1), "\x{FF}");

$x = "\xF1\xF2\xF3";
substr($x, -1, 1) = "\x{100}\xFF";
is(length($x), 4);
is($x, "\xF1\xF2\x{100}\xFF");
is(substr($x, 0, 1), "\x{F1}");
is(substr($x, 1, 1), "\x{F2}");
is(substr($x, 2, 1), "\x{100}");
is(substr($x, 3, 1), "\x{FF}");

$x = "\xF1\xF2\xF3";
substr($x, -1, 0) = "\x{100}\xFF";
is(length($x), 5);
is($x, "\xF1\xF2\x{100}\xFF\xF3");
is(substr($x, 0, 1), "\x{F1}");
is(substr($x, 1, 1), "\x{F2}");
is(substr($x, 2, 1), "\x{100}");
is(substr($x, 3, 1), "\x{FF}");
is(substr($x, 4, 1), "\x{F3}");

$x = "\xF1\xF2\xF3";
substr($x, 0, -1) = "\x{100}\xFF";
is(length($x), 3);
is($x, "\x{100}\xFF\xF3");
is(substr($x, 0, 1), "\x{100}");
is(substr($x, 1, 1), "\x{FF}");
is(substr($x, 2, 1), "\x{F3}");

$x = "\xF1\xF2\xF3";
substr($x, 0, -2) = "\x{100}\xFF";
is(length($x), 4);
is($x, "\x{100}\xFF\xF2\xF3");
is(substr($x, 0, 1), "\x{100}");
is(substr($x, 1, 1), "\x{FF}");
is(substr($x, 2, 1), "\x{F2}");
is(substr($x, 3, 1), "\x{F3}");

$x = "\xF1\xF2\xF3";
substr($x, 0, -3) = "\x{100}\xFF";
is(length($x), 5);
is($x, "\x{100}\xFF\xF1\xF2\xF3");
is(substr($x, 0, 1), "\x{100}");
is(substr($x, 1, 1), "\x{FF}");
is(substr($x, 2, 1), "\x{F1}");
is(substr($x, 3, 1), "\x{F2}");
is(substr($x, 4, 1), "\x{F3}");

$x = "\xF1\xF2\xF3";
substr($x, 1, -1) = "\x{100}\xFF";
is(length($x), 4);
is($x, "\xF1\x{100}\xFF\xF3");
is(substr($x, 0, 1), "\x{F1}");
is(substr($x, 1, 1), "\x{100}");
is(substr($x, 2, 1), "\x{FF}");
is(substr($x, 3, 1), "\x{F3}");

$x = "\xF1\xF2\xF3";
substr($x, -1, -1) = "\x{100}\xFF";
is(length($x), 5);
is($x, "\xF1\xF2\x{100}\xFF\xF3");
is(substr($x, 0, 1), "\x{F1}");
is(substr($x, 1, 1), "\x{F2}");
is(substr($x, 2, 1), "\x{100}");
is(substr($x, 3, 1), "\x{FF}");
is(substr($x, 4, 1), "\x{F3}");

# And tests for already-UTF8 one

$x = "\x{101}\x{F2}\x{F3}";
substr($x, 0, 1) = "\x{100}";
is(length($x), 3);
is($x, "\x{100}\xF2\xF3");
is(substr($x, 0, 1), "\x{100}");
is(substr($x, 1, 1), "\x{F2}");
is(substr($x, 2, 1), "\x{F3}");

$x = "\x{101}\x{F2}\x{F3}";
substr($x, 0, 1) = "\x{100}\x{FF}";
is(length($x), 4);
is($x, "\x{100}\x{FF}\xF2\xF3");
is(substr($x, 0, 1), "\x{100}");
is(substr($x, 1, 1), "\x{FF}");
is(substr($x, 2, 1), "\x{F2}");
is(substr($x, 3, 1), "\x{F3}");

$x = "\x{101}\x{F2}\x{F3}";
substr($x, 0, 2) = "\x{100}\xFF";
is(length($x), 3);
is($x, "\x{100}\xFF\xF3");
is(substr($x, 0, 1), "\x{100}");
is(substr($x, 1, 1), "\x{FF}");
is(substr($x, 2, 1), "\x{F3}");

$x = "\x{101}\x{F2}\x{F3}";
substr($x, 1, 1) = "\x{100}\xFF";
is(length($x), 4);
is($x, "\x{101}\x{100}\xFF\xF3");
is(substr($x, 0, 1), "\x{101}");
is(substr($x, 1, 1), "\x{100}");
is(substr($x, 2, 1), "\x{FF}");
is(substr($x, 3, 1), "\x{F3}");

$x = "\x{101}\x{F2}\x{F3}";
substr($x, 2, 1) = "\x{100}\xFF";
is(length($x), 4);
is($x, "\x{101}\xF2\x{100}\xFF");
is(substr($x, 0, 1), "\x{101}");
is(substr($x, 1, 1), "\x{F2}");
is(substr($x, 2, 1), "\x{100}");
is(substr($x, 3, 1), "\x{FF}");

$x = "\x{101}\x{F2}\x{F3}";
substr($x, 3, 1) = "\x{100}\xFF";
is(length($x), 5);
is($x, "\x{101}\x{F2}\x{F3}\x{100}\xFF");
is(substr($x, 0, 1), "\x{101}");
is(substr($x, 1, 1), "\x{F2}");
is(substr($x, 2, 1), "\x{F3}");
is(substr($x, 3, 1), "\x{100}");
is(substr($x, 4, 1), "\x{FF}");

$x = "\x{101}\x{F2}\x{F3}";
substr($x, -1, 1) = "\x{100}\xFF";
is(length($x), 4);
is($x, "\x{101}\xF2\x{100}\xFF");
is(substr($x, 0, 1), "\x{101}");
is(substr($x, 1, 1), "\x{F2}");
is(substr($x, 2, 1), "\x{100}");
is(substr($x, 3, 1), "\x{FF}");

$x = "\x{101}\x{F2}\x{F3}";
substr($x, -1, 0) = "\x{100}\xFF";
is(length($x), 5);
is($x, "\x{101}\xF2\x{100}\xFF\xF3");
is(substr($x, 0, 1), "\x{101}");
is(substr($x, 1, 1), "\x{F2}");
is(substr($x, 2, 1), "\x{100}");
is(substr($x, 3, 1), "\x{FF}");
is(substr($x, 4, 1), "\x{F3}");

$x = "\x{101}\x{F2}\x{F3}";
substr($x, 0, -1) = "\x{100}\xFF";
is(length($x), 3);
is($x, "\x{100}\xFF\xF3");
is(substr($x, 0, 1), "\x{100}");
is(substr($x, 1, 1), "\x{FF}");
is(substr($x, 2, 1), "\x{F3}");

$x = "\x{101}\x{F2}\x{F3}";
substr($x, 0, -2) = "\x{100}\xFF";
is(length($x), 4);
is($x, "\x{100}\xFF\xF2\xF3");
is(substr($x, 0, 1), "\x{100}");
is(substr($x, 1, 1), "\x{FF}");
is(substr($x, 2, 1), "\x{F2}");
is(substr($x, 3, 1), "\x{F3}");

$x = "\x{101}\x{F2}\x{F3}";
substr($x, 0, -3) = "\x{100}\xFF";
is(length($x), 5);
is($x, "\x{100}\xFF\x{101}\x{F2}\x{F3}");
is(substr($x, 0, 1), "\x{100}");
is(substr($x, 1, 1), "\x{FF}");
is(substr($x, 2, 1), "\x{101}");
is(substr($x, 3, 1), "\x{F2}");
is(substr($x, 4, 1), "\x{F3}");

$x = "\x{101}\x{F2}\x{F3}";
substr($x, 1, -1) = "\x{100}\xFF";
is(length($x), 4);
is($x, "\x{101}\x{100}\xFF\xF3");
is(substr($x, 0, 1), "\x{101}");
is(substr($x, 1, 1), "\x{100}");
is(substr($x, 2, 1), "\x{FF}");
is(substr($x, 3, 1), "\x{F3}");

$x = "\x{101}\x{F2}\x{F3}";
substr($x, -1, -1) = "\x{100}\xFF";
is(length($x), 5);
is($x, "\x{101}\xF2\x{100}\xFF\xF3");
is(substr($x, 0, 1), "\x{101}");
is(substr($x, 1, 1), "\x{F2}");
is(substr($x, 2, 1), "\x{100}");
is(substr($x, 3, 1), "\x{FF}");
is(substr($x, 4, 1), "\x{F3}");

substr($x = "ab", 0, 0, "\x{100}\x{200}");
is($x, "\x{100}\x{200}ab");

substr($x = "\x{100}\x{200}", 0, 0, "ab");
is($x, "ab\x{100}\x{200}");

substr($x = "ab", 1, 0, "\x{100}\x{200}");
is($x, "a\x{100}\x{200}b");

substr($x = "\x{100}\x{200}", 1, 0, "ab");
is($x, "\x{100}ab\x{200}");

substr($x = "ab", 2, 0, "\x{100}\x{200}");
is($x, "ab\x{100}\x{200}");

substr($x = "\x{100}\x{200}", 2, 0, "ab");
is($x, "\x{100}\x{200}ab");

substr($x = "\xFFb", 0, 0, "\x{100}\x{200}");
is($x, "\x{100}\x{200}\xFFb");

substr($x = "\x{100}\x{200}", 0, 0, "\xFFb");
is($x, "\xFFb\x{100}\x{200}");

substr($x = "\xFFb", 1, 0, "\x{100}\x{200}");
is($x, "\xFF\x{100}\x{200}b");

substr($x = "\x{100}\x{200}", 1, 0, "\xFFb");
is($x, "\x{100}\xFFb\x{200}");

substr($x = "\xFFb", 2, 0, "\x{100}\x{200}");
is($x, "\xFFb\x{100}\x{200}");

substr($x = "\x{100}\x{200}", 2, 0, "\xFFb");
is($x, "\x{100}\x{200}\xFFb");

# [perl #20933]
{ 
    my $s = "ab";
    my @r; 
    $r[$_] = \ substr $s, $_, 1 for (0, 1);
    is(join("", map { $$_ } @r), "ab");
}

# [perl #23207]
{
    sub ss {
	substr($_[0],0,1) ^= substr($_[0],1,1) ^=
	substr($_[0],0,1) ^= substr($_[0],1,1);
    }
    my $x = my $y = 'AB'; ss $x; ss $y;
    is($x, $y);
}

# [perl #24605]
{
    my $x = "0123456789\x{500}";
    my $y = substr $x, 4;
    is(substr($x, 7, 1), "7");
}

# [perl #24200] string corruption with lvalue sub

{
    my $foo = "a";
    sub bar: lvalue { substr $foo, 0 }
    bar = "XXX";
    is(bar, 'XXX');
    $foo = '123456789';
    is(bar, '123456789');
}

# [perl #29149]
{
    my $text  = "0123456789\xED ";
    utf8::upgrade($text);
    my $pos = 5;
    pos($text) = $pos;
    my $a = substr($text, $pos, $pos);
    is(substr($text,$pos,1), $pos);

}

# [perl #23765]
{
    my $a = pack("C", 0xbf);
    substr($a, -1) &= chr(0xfeff);
    is($a, "\xbf");
}

# [perl #34976] incorrect caching of utf8 substr length
{
    my  $a = "abcd\x{100}";
    is(substr($a,1,2), 'bc');
    is(substr($a,1,1), 'b');
}
