#!./perl -w

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

plan tests => 5819;

use strict;
use warnings;
use Config;

my $Is_EBCDIC = (defined $Config{ebcdic} && $Config{ebcdic} eq 'define');
my $Perl = which_perl();

sub encode_list {
  my @result = map {_qq($_)} @_;
  if (@result == 1) {
    return @result;
  }
  return '(' . join (', ', @result) . ')';
}


sub list_eq ($$) {
  my ($l, $r) = @_;
  return 0 unless @$l == @$r;
  for my $i (0..$#$l) {
    if (defined $l->[$i]) {
      return 0 unless defined ($r->[$i]) && $l->[$i] eq $r->[$i];
    } else {
      return 0 if defined $r->[$i]
    }
  }
  return 1;
}

##############################################################################
#
# Here starteth the tests
#

{
    my $format = "c2 x5 C C x s d i l a6";
    # Need the expression in here to force ary[5] to be numeric.  This avoids
    # test2 failing because ary2 goes str->numeric->str and ary doesn't.
    my @ary = (1,-100,127,128,32767,987.654321098 / 100.0,12345,123456,
               "abcdef");
    my $foo = pack($format,@ary);
    my @ary2 = unpack($format,$foo);

    is($#ary, $#ary2);

    my $out1=join(':',@ary);
    my $out2=join(':',@ary2);
    # Using long double NVs may introduce greater accuracy than wanted.
    $out1 =~ s/:9\.87654321097999\d*:/:9.87654321098:/;
    $out2 =~ s/:9\.87654321097999\d*:/:9.87654321098:/;
    is($out1, $out2);

    like($foo, qr/def/);
}
# How about counting bits?

{
    my $x;
    is( ($x = unpack("%32B*", "\001\002\004\010\020\040\100\200\377")), 16 );

    is( ($x = unpack("%32b69", "\001\002\004\010\020\040\100\200\017")), 12 );

    is( ($x = unpack("%32B69", "\001\002\004\010\020\040\100\200\017")), 9 );
}

{
    my $sum = 129; # ASCII
    $sum = 103 if $Is_EBCDIC;

    my $x;
    is( ($x = unpack("%32B*", "Now is the time for all good blurfl")), $sum );

    my $foo;
    open(BIN, $Perl) || die "Can't open $Perl: $!\n";
    sysread BIN, $foo, 8192;
    close BIN;

    $sum = unpack("%32b*", $foo);
    my $longway = unpack("b*", $foo);
    is( $sum, $longway =~ tr/1/1/ );
}

{
  my $x;
  is( ($x = unpack("I",pack("I", 0xFFFFFFFF))), 0xFFFFFFFF );
}

{
    # check 'w'
    my @x = (5,130,256,560,32000,3097152,268435455,1073741844, 2**33,
             '4503599627365785','23728385234614992549757750638446');
    my $x = pack('w*', @x);
    my $y = pack 'H*', '0581028200843081fa0081bd8440ffffff7f8480808014A0808'.
                       '0800087ffffffffffdb19caefe8e1eeeea0c2e1e3e8ede1ee6e';

    is($x, $y);

    my @y = unpack('w*', $y);
    my $a;
    while ($a = pop @x) {
        my $b = pop @y;
        is($a, $b);
    }

    @y = unpack('w2', $x);

    is(scalar(@y), 2);
    is($y[1], 130);
    $x = pack('w*', 5000000000); $y = '';
    eval {
    use Math::BigInt;
    $y = pack('w*', Math::BigInt::->new(5000000000));
    };
    is($x, $y);

    $x = pack 'w', ~0;
    $y = pack 'w', (~0).'';
    is($x, $y);
    is(unpack ('w',$x), ~0);
    is(unpack ('w',$y), ~0);

    $x = pack 'w', ~0 - 1;
    $y = pack 'w', (~0) - 2;

    if (~0 - 1 == (~0) - 2) {
        is($x, $y, "NV arithmetic");
    } else {
        isnt($x, $y, "IV/NV arithmetic");
    }
    cmp_ok(unpack ('w',$x), '==', ~0 - 1);
    cmp_ok(unpack ('w',$y), '==', ~0 - 2);

    # These should spot that pack 'w' is using NV, not double, on platforms
    # where IVs are smaller than doubles, and harmlessly pass elsewhere.
    # (tests for change 16861)
    my $x0 = 2**54+3;
    my $y0 = 2**54-2;

    $x = pack 'w', $x0;
    $y = pack 'w', $y0;

    if ($x0 == $y0) {
        is($x, $y, "NV arithmetic");
    } else {
        isnt($x, $y, "IV/NV arithmetic");
    }
    cmp_ok(unpack ('w',$x), '==', $x0);
    cmp_ok(unpack ('w',$y), '==', $y0);
}


{
  # test exceptions
  my $x;
  eval { $x = unpack 'w', pack 'C*', 0xff, 0xff};
  like($@, qr/^Unterminated compressed integer/);

  eval { $x = unpack 'w', pack 'C*', 0xff, 0xff, 0xff, 0xff};
  like($@, qr/^Unterminated compressed integer/);

  eval { $x = unpack 'w', pack 'C*', 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
  like($@, qr/^Unterminated compressed integer/);
}

#
# test the "p" template

# literals
is(unpack("p",pack("p","foo")), "foo");

# scalars
is(unpack("p",pack("p",239)), 239);

# temps
sub foo { my $a = "a"; return $a . $a++ . $a++ }
{
  use warnings;
  my $warning;
  local $SIG{__WARN__} = sub {
      $warning = $_[0];
  };
  my $junk = pack("p", &foo);

  like($warning, qr/temporary val/);
}

# undef should give null pointer
like(pack("p", undef), qr/^\0+/);

# Check for optimizer bug (e.g.  Digital Unix GEM cc with -O4 on DU V4.0B gives
#                                4294967295 instead of -1)
#				 see #ifdef __osf__ in pp.c pp_unpack
is((unpack("i",pack("i",-1))), -1);

# test the pack lengths of s S i I l L
# test the pack lengths of n N v V
my @lengths = qw(s 2 S 2 i -4 I -4 l 4 L 4 n 2 N 4 v 2 V 4);
while (my ($format, $expect) = splice @lengths, 0, 2) {
  my $len = length(pack($format, 0));
  if ($expect > 0) {
    is($expect, $len, "format '$format'");
  } else {
    $expect = -$expect;
    ok ($len >= $expect, "format '$format'") ||
      print "# format '$format' has length $len, expected >= $expect\n";
  }
}


# test unpack-pack lengths
my @templates = qw(c C i I s S l L n N v V f d q Q);

foreach my $t (@templates) {
    SKIP: {
        my @t = eval { unpack("$t*", pack("$t*", 12, 34)) };

        # quads not supported everywhere
        skip "Quads not supported", 4 if $@ =~ /Invalid type in pack/;
        is( $@, '' );

        is(scalar @t, 2);

        SKIP: {
            skip "$t not expected to work for some reason", 2 if $t =~ /[nv]/i;

            is($t[0], 12);
            is($t[1], 34);
        }
    }
}

{
    # uuencode/decode

    # Note that first uuencoding known 'text' data and then checking the
    # binary values of the uuencoded version would not be portable between
    # character sets.  Uuencoding is meant for encoding binary data, not
    # text data.

    my $in = pack 'C*', 0 .. 255;

    # just to be anal, we do some random tr/`/ /
    my $uu = <<'EOUU';
M` $"`P0%!@<("0H+# T.#Q`1$A,4%187&!D:&QP='A\@(2(C)"4F)R@I*BLL
M+2XO,#$R,S0U-C<X.3H[/#T^/T!!0D-$149'2$E*2TQ-3D]045)35%565UA9
M6EM<75Y?8&%B8V1E9F=H:6IK;&UN;W!Q<G-T=79W>'EZ>WQ]?G^`@8*#A(6&
MAXB)BHN,C8Z/D)&2DY25EI>8F9J;G)V>GZ"AHJ.DI::GJ*FJJZRMKJ^PL;*S
MM+6VM[BYNKN\O;Z_P,'"P\3%QL?(R<K+S,W.S]#1TM/4U=;7V-G:V]S=WM_@
?X>+CY.7FY^CIZNOL[>[O\/'R\_3U]O?X^?K[_/W^_P `
EOUU

    $_ = $uu;
    tr/ /`/;

    is(pack('u', $in), $_);

    is(unpack('u', $uu), $in);

    $in = "\x1f\x8b\x08\x08\x58\xdc\xc4\x35\x02\x03\x4a\x41\x50\x55\x00\xf3\x2a\x2d\x2e\x51\x48\xcc\xcb\x2f\xc9\x48\x2d\x52\x08\x48\x2d\xca\x51\x28\x2d\x4d\xce\x4f\x49\x2d\xe2\x02\x00\x64\x66\x60\x5c\x1a\x00\x00\x00";
    $uu = <<'EOUU';
M'XL("%C<Q#4"`TI!4%4`\RHM+E%(S,LOR4@M4@A(+<I1*"U-SD])+>("`&1F
&8%P:````
EOUU

    is(unpack('u', $uu), $in);

# This is identical to the above except that backquotes have been
# changed to spaces

    $uu = <<'EOUU';
M'XL("%C<Q#4" TI!4%4 \RHM+E%(S,LOR4@M4@A(+<I1*"U-SD])+>(" &1F
&8%P:
EOUU

    # ' # Grr
    is(unpack('u', $uu), $in);

}

# test the ascii template types (A, a, Z)

foreach (
['p', 'A*',  "foo\0bar\0 ", "foo\0bar\0 "],
['p', 'A11', "foo\0bar\0 ", "foo\0bar\0   "],
['u', 'A*',  "foo\0bar \0", "foo\0bar"],
['u', 'A8',  "foo\0bar \0", "foo\0bar"],
['p', 'a*',  "foo\0bar\0 ", "foo\0bar\0 "],
['p', 'a11', "foo\0bar\0 ", "foo\0bar\0 \0\0"],
['u', 'a*',  "foo\0bar \0", "foo\0bar \0"],
['u', 'a8',  "foo\0bar \0", "foo\0bar "],
['p', 'Z*',  "foo\0bar\0 ", "foo\0bar\0 \0"],
['p', 'Z11', "foo\0bar\0 ", "foo\0bar\0 \0\0"],
['p', 'Z3',  "foo",         "fo\0"],
['u', 'Z*',  "foo\0bar \0", "foo"],
['u', 'Z8',  "foo\0bar \0", "foo"],
) 
{
    my ($what, $template, $in, $out) = @$_;
    my $got = $what eq 'u' ? (unpack $template, $in) : (pack $template, $in);
    unless (is($got, $out)) {
        my $un = $what eq 'u' ? 'un' : '';
        print "# ${un}pack ('$template', "._qq($in).') gave '._qq($out).
            ' not '._qq($got)."\n";
    }
}

# packing native shorts/ints/longs

is(length(pack("s!", 0)), $Config{shortsize});
is(length(pack("i!", 0)), $Config{intsize});
is(length(pack("l!", 0)), $Config{longsize});
ok(length(pack("s!", 0)) <= length(pack("i!", 0)));
ok(length(pack("i!", 0)) <= length(pack("l!", 0)));
is(length(pack("i!", 0)), length(pack("i", 0)));

sub numbers {
  my $format = shift;
  return numbers_with_total ($format, undef, @_);
}

sub numbers_with_total {
  my $format = shift;
  my $total = shift;
  if (!defined $total) {
    foreach (@_) {
      $total += $_;
    }
  }
  foreach (@_) {
    SKIP: {
        my $out = eval {unpack($format, pack($format, $_))};
        skip "cannot pack '$format' on this perl", 2 if
          $@ =~ /Invalid type in pack: '$format'/;

        is($@, '');
        is($out, $_);
    }
  }

  my $skip_if_longer_than = ~0; # "Infinity"
  if (~0 - 1 == ~0) {
    # If we're running with -DNO_PERLPRESERVE_IVUV and NVs don't preserve all
    # UVs (in which case ~0 is NV, ~0-1 will be the same NV) then we can't
    # correctly in perl calculate UV totals for long checksums, as pp_unpack
    # is using UV maths, and we've only got NVs.
    $skip_if_longer_than = $Config{nv_preserves_uv_bits};
  }

  foreach ('', 1, 2, 3, 15, 16, 17, 31, 32, 33, 53, 54, 63, 64, 65) {
    SKIP: {
      my $sum = eval {unpack "%$_$format*", pack "$format*", @_};
      skip "cannot pack '$format' on this perl", 3
        if $@ =~ /Invalid type in pack: '$format'/;

      is($@, '');
      ok(defined $sum);

      my $len = $_; # Copy, so that we can reassign ''
      $len = 16 unless length $len;

      SKIP: {
        skip "cannot test checksums over $skip_if_longer_than bits", 1
          if $len > $skip_if_longer_than;

        # Our problem with testing this portably is that the checksum code in
        # pp_unpack is able to cast signed to unsigned, and do modulo 2**n
        # arithmetic in unsigned ints, which perl has no operators to do.
        # (use integer; does signed ints, which won't wrap on UTS, which is just
        # fine with ANSI, but not with most people's assumptions.
        # This is why we need to supply the totals for 'Q' as there's no way in
        # perl to calculate them, short of unpack '%0Q' (is that documented?)
        # ** returns NVs; make sure it's IV.
        my $max = 1 + 2 * (int (2 ** ($len-1))-1); # The max possible checksum
        my $max_p1 = $max + 1;
        my ($max_is_integer, $max_p1_is_integer);
        $max_p1_is_integer = 1 unless $max_p1 + 1 == $max_p1;
        $max_is_integer = 1 if $max - 1 < ~0;

        my $calc_sum;
        if (ref $total) {
            $calc_sum = &$total($len);
        } else {
            $calc_sum = $total;
            # Shift into range by some multiple of the total
            my $mult = int ($total / $max_p1);
            # Need this to make sure that -1 + (~0+1) is ~0 (ie still integer)
            $calc_sum = $total - $mult;
            $calc_sum -= $mult * $max;
            if ($calc_sum < 0) {
                $calc_sum += 1;
                $calc_sum += $max;
            }
        }
        if ($calc_sum == $calc_sum - 1 && $calc_sum == $max_p1) {
            # we're into floating point (either by getting out of the range of
            # UV arithmetic, or because we're doing a floating point checksum) 
            # and our calculation of the checksum has become rounded up to
            # max_checksum + 1
            $calc_sum = 0;
        }

        if ($calc_sum == $sum) { # HAS to be ==, not eq (so no is()).
            ok ("unpack '%$_$format' gave $sum");
        } else {
            my $delta = 1.000001;
            if ($format =~ tr /dDfF//
                && ($calc_sum <= $sum * $delta && $calc_sum >= $sum / $delta)) {
                pass ("unpack '%$_$format' gave $sum, expected $calc_sum");
            } else {
                my $text = ref $total ? &$total($len) : $total;
                fail;
                print "# For list (" . join (", ", @_) . ") (total $text)"
                    . " packed with $format unpack '%$_$format' gave $sum,"
                    . " expected $calc_sum\n";
            }
        }
      }
    }   
  }
}

numbers ('c', -128, -1, 0, 1, 127);
numbers ('C', 0, 1, 127, 128, 255);
numbers ('s', -32768, -1, 0, 1, 32767);
numbers ('S', 0, 1, 32767, 32768, 65535);
numbers ('i', -2147483648, -1, 0, 1, 2147483647);
numbers ('I', 0, 1, 2147483647, 2147483648, 4294967295);
numbers ('l', -2147483648, -1, 0, 1, 2147483647);
numbers ('L', 0, 1, 2147483647, 2147483648, 4294967295);
numbers ('s!', -32768, -1, 0, 1, 32767);
numbers ('S!', 0, 1, 32767, 32768, 65535);
numbers ('i!', -2147483648, -1, 0, 1, 2147483647);
numbers ('I!', 0, 1, 2147483647, 2147483648, 4294967295);
numbers ('l!', -2147483648, -1, 0, 1, 2147483647);
numbers ('L!', 0, 1, 2147483647, 2147483648, 4294967295);
numbers ('n', 0, 1, 32767, 32768, 65535);
numbers ('v', 0, 1, 32767, 32768, 65535);
numbers ('N', 0, 1, 2147483647, 2147483648, 4294967295);
numbers ('V', 0, 1, 2147483647, 2147483648, 4294967295);
# All these should have exact binary representations:
numbers ('f', -1, 0, 0.5, 42, 2**34);
numbers ('d', -(2**34), -1, 0, 1, 2**34);
## These don't, but 'd' is NV.  XXX wrong, it's double
#numbers ('d', -1, 0, 1, 1-exp(-1), -exp(1));

numbers_with_total ('q', -1,
                    -9223372036854775808, -1, 0, 1,9223372036854775807);
# This total is icky, but the true total is 2**65-1, and need a way to generate
# the epxected checksum on any system including those where NVs can preserve
# 65 bits. (long double is 128 bits on sparc, so they certainly can)
# or where rounding is down not up on binary conversion (crays)
numbers_with_total ('Q', sub {
                      my $len = shift;
                      $len = 65 if $len > 65; # unmasked total is 2**65-1 here
                      my $total = 1 + 2 * (int (2**($len - 1)) - 1);
                      return 0 if $total == $total - 1; # Overflowed integers
                      return $total; # NVs still accurate to nearest integer
                    },
                    0, 1,9223372036854775807, 9223372036854775808,
                    18446744073709551615);

# pack nvNV byteorders

is(pack("n", 0xdead), "\xde\xad");
is(pack("v", 0xdead), "\xad\xde");
is(pack("N", 0xdeadbeef), "\xde\xad\xbe\xef");
is(pack("V", 0xdeadbeef), "\xef\xbe\xad\xde");

{
  # /

  my ($x, $y, $z);
  eval { ($x) = unpack '/a*','hello' };
  like($@, qr!/ must follow a numeric type!);
  undef $x;
  eval { $x = unpack '/a*','hello' };
  like($@, qr!/ must follow a numeric type!);

  undef $x;
  eval { ($z,$x,$y) = unpack 'a3/A C/a* C/Z', "003ok \003yes\004z\000abc" };
  is($@, '');
  is($z, 'ok');
  is($x, 'yes');
  is($y, 'z');
  undef $z;
  eval { $z = unpack 'a3/A C/a* C/Z', "003ok \003yes\004z\000abc" };
  is($@, '');
  is($z, 'ok');


  undef $x;
  eval { ($x) = pack '/a*','hello' };
  like($@,  qr!Invalid type in pack: '/'!);
  undef $x;
  eval { $x = pack '/a*','hello' };
  like($@,  qr!Invalid type in pack: '/'!);

  $z = pack 'n/a* N/Z* w/A*','string','hi there ','etc';
  my $expect = "\000\006string\0\0\0\012hi there \000\003etc";
  is($z, $expect);

  undef $x;
  $expect = 'hello world';
  eval { ($x) = unpack ("w/a", chr (11) . "hello world!")};
  is($x, $expect);
  is($@, '');

  undef $x;
  # Doing this in scalar context used to fail.
  eval { $x = unpack ("w/a", chr (11) . "hello world!")};
  is($@, '');
  is($x, $expect);

  foreach (
           ['a/a*/a*', '212ab345678901234567','ab3456789012'],
           ['a/a*/a*', '3012ab345678901234567', 'ab3456789012'],
           ['a/a*/b*', '212ab', $Is_EBCDIC ? '100000010100' : '100001100100'],
  ) 
  {
    my ($pat, $in, $expect) = @$_;
    undef $x;
    eval { ($x) = unpack $pat, $in };
    is($@, '');
    is($x, $expect) || 
      printf "# list unpack ('$pat', '$in') gave %s, expected '$expect'\n",
             encode_list ($x);

    undef $x;
    eval { $x = unpack $pat, $in };
    is($@, '');
    is($x, $expect) ||
      printf "# scalar unpack ('$pat', '$in') gave %s, expected '$expect'\n",
             encode_list ($x);
  }

  # / with #

  my $pattern = <<'EOU';
 a3/A			# Count in ASCII
 C/a*			# Count in a C char
 C/Z			# Count in a C char but skip after \0
EOU

  $x = $y = $z =undef;
  eval { ($z,$x,$y) = unpack $pattern, "003ok \003yes\004z\000abc" };
  is($@, '');
  is($z, 'ok');
  is($x, 'yes');
  is($y, 'z');
  undef $x;
  eval { $z = unpack $pattern, "003ok \003yes\004z\000abc" };
  is($@, '');
  is($z, 'ok');

  $pattern = <<'EOP';
  n/a*			# Count as network short
  w/A*			# Count a  BER integer
EOP
  $expect = "\000\006string\003etc";
  $z = pack $pattern,'string','etc';
  is($z, $expect);
}


SKIP: {
    skip("(EBCDIC and) version strings are bad idea", 2) if $Is_EBCDIC;

    is("1.20.300.4000", sprintf "%vd", pack("U*",1,20,300,4000));
    is("1.20.300.4000", sprintf "%vd", pack("  U*",1,20,300,4000));
}
isnt(v1.20.300.4000, sprintf "%vd", pack("C0U*",1,20,300,4000));

my $rslt = $Is_EBCDIC ? "156 67" : "199 162";
is(join(" ", unpack("C*", chr(0x1e2))), $rslt);

# does pack U create Unicode?
is(ord(pack('U', 300)), 300);

# does unpack U deref Unicode?
is((unpack('U', chr(300)))[0], 300);

# is unpack U the reverse of pack U for Unicode string?
is("@{[unpack('U*', pack('U*', 100, 200, 300))]}", "100 200 300");

# is unpack U the reverse of pack U for byte string?
is("@{[unpack('U*', pack('U*', 100, 200))]}", "100 200");


SKIP: {
    skip "Not for EBCDIC", 4 if $Is_EBCDIC;

    # does unpack C unravel pack U?
    is("@{[unpack('C*', pack('U*', 100, 200))]}", "100 195 136");

    # does pack U0C create Unicode?
    is("@{[pack('U0C*', 100, 195, 136)]}", v100.v200);

    # does pack C0U create characters?
    is("@{[pack('C0U*', 100, 200)]}", pack("C*", 100, 195, 136));

    # does unpack U0U on byte data warn?
    {
        local $SIG{__WARN__} = sub { $@ = "@_" };
        my @null = unpack('U0U', chr(255));
        like($@, /^Malformed UTF-8 character /);
    }
}

{
  my $p = pack 'i*', -2147483648, ~0, 0, 1, 2147483647;
  my (@a);
  # bug - % had to be at the start of the pattern, no leading whitespace or
  # comments. %i! didn't work at all.
  foreach my $pat ('%32i*', ' %32i*', "# Muhahahaha\n%32i*", '%32i*  ',
                   '%32i!*', ' %32i!*', "\n#\n#\n\r \t\f%32i!*", '%32i!*#') {
    @a = unpack $pat, $p;
    is($a[0], 0xFFFFFFFF) || print "# $pat\n";
    @a = scalar unpack $pat, $p;
    is($a[0], 0xFFFFFFFF) || print "# $pat\n";
  }


  $p = pack 'I*', 42, 12;
  # Multiline patterns in scalar context failed.
  foreach my $pat ('I', <<EOPOEMSNIPPET, 'I#I', 'I # I', 'I # !!!') {
# On the Ning Nang Nong
# Where the Cows go Bong!
# And the Monkeys all say Boo!
I
EOPOEMSNIPPET
    @a = unpack $pat, $p;
    is(scalar @a, 1);
    is($a[0], 42);
    @a = scalar unpack $pat, $p;
    is(scalar @a, 1);
    is($a[0], 42);
  }

  # shorts (of all flavours) didn't calculate checksums > 32 bits with floating
  # point, so a pathologically long pattern would wrap at 32 bits.
  my $pat = "\xff\xff"x65538; # Start with it long, to save any copying.
  foreach (4,3,2,1,0) {
    my $len = 65534 + $_;
    is(unpack ("%33n$len", $pat), 65535 * $len);
  }
}


# pack x X @
foreach (
         ['x', "N", "\0"],
         ['x4', "N", "\0"x4],
         ['xX', "N", ""],
         ['xXa*', "Nick", "Nick"],
         ['a5Xa5', "cameL", "llama", "camellama"],
         ['@4', 'N', "\0"x4],
         ['a*@8a*', 'Camel', 'Dromedary', "Camel\0\0\0Dromedary"],
         ['a*@4a', 'Perl rules', '!', 'Perl!'],
) 
{
  my ($template, @in) = @$_;
  my $out = pop @in;
  my $got = eval {pack $template, @in};
  is($@, '');
  is($out, $got) ||
    printf "# pack ('$template', %s) gave %s expected %s\n",
           encode_list (@in), encode_list ($got), encode_list ($out);
}

# unpack x X @
foreach (
         ['x', "N"],
         ['xX', "N"],
         ['xXa*', "Nick", "Nick"],
         ['a5Xa5', "camellama", "camel", "llama"],
         ['@3', "ice"],
         ['@2a2', "water", "te"],
         ['a*@1a3', "steam", "steam", "tea"],
) 
{
  my ($template, $in, @out) = @$_;
  my @got = eval {unpack $template, $in};
  is($@, '');
  ok (list_eq (\@got, \@out)) ||
    printf "# list unpack ('$template', %s) gave %s expected %s\n",
           _qq($in), encode_list (@got), encode_list (@out);

  my $got = eval {unpack $template, $in};
  is($@, '');
  @out ? is( $got, $out[0] ) # 1 or more items; should get first
       : ok( !defined $got ) # 0 items; should get undef
    or printf "# scalar unpack ('$template', %s) gave %s expected %s\n",
              _qq($in), encode_list ($got), encode_list ($out[0]);
}

{
    my $t = 'Z*Z*';
    my ($u, $v) = qw(foo xyzzy);
    my $p = pack($t, $u, $v);
    my @u = unpack($t, $p);
    is(scalar @u, 2);
    is($u[0], $u);
    is($u[1], $v);
}

{
    is((unpack("w/a*", "\x02abc"))[0], "ab");

    # "w/a*" should be seen as one unit

    is(scalar unpack("w/a*", "\x02abc"), "ab");
}

{
    # from Wolfgang Laun: fix in change #13163

    my $s = 'ABC' x 10;
    my $t = '*';
    my $x = ord($t);
    my $buf = pack( 'Z*/A* C',  $s, $x );
    my $y;

    my $h = $buf;
    $h =~ s/[^[:print:]]/./g;
    ( $s, $y ) = unpack( "Z*/A* C", $buf );
    is($h, "30.ABCABCABCABCABCABCABCABCABCABC$t");
    is(length $buf, 34);
    is($s, "ABCABCABCABCABCABCABCABCABCABC");
    is($y, $x);
}

{
    # from Wolfgang Laun: fix in change #13288

    eval { my $t=unpack("P*", "abc") };
    like($@, qr/P must have an explicit size/);
}

{   # Grouping constructs
    my (@a, @b);
    @a = unpack '(SL)',   pack 'SLSLSL', 67..90;
    is("@a", "67 68");
    @a = unpack '(SL)3',   pack 'SLSLSL', 67..90;
    @b = (67..72);
    is("@a", "@b");
    @a = unpack '(SL)3',   pack 'SLSLSLSL', 67..90;
    is("@a", "@b");
    @a = unpack '(SL)[3]', pack 'SLSLSLSL', 67..90;
    is("@a", "@b");
    @a = unpack '(SL)[2] SL', pack 'SLSLSLSL', 67..90;
    is("@a", "@b");
    @a = unpack 'A/(SL)',  pack 'ASLSLSLSL', 3, 67..90;
    is("@a", "@b");
    @a = unpack 'A/(SL)SL',  pack 'ASLSLSLSL', 2, 67..90;
    is("@a", "@b");
    @a = unpack '(SL)*',   pack 'SLSLSLSL', 67..90;
    @b = (67..74);
    is("@a", "@b");
    @a = unpack '(SL)*SL',   pack 'SLSLSLSL', 67..90;
    is("@a", "@b");
    eval { @a = unpack '(*SL)',   '' };
    like($@, qr/\(\)-group starts with a count/);
    eval { @a = unpack '(3SL)',   '' };
    like($@, qr/\(\)-group starts with a count/);
    eval { @a = unpack '([3]SL)',   '' };
    like($@, qr/\(\)-group starts with a count/);
    eval { @a = pack '(*SL)' };
    like($@, qr/\(\)-group starts with a count/);
    @a = unpack '(SL)3 SL',   pack '(SL)4', 67..74;
    is("@a", "@b");
    @a = unpack '(SL)3 SL',   pack '(SL)[4]', 67..74;
    is("@a", "@b");
    @a = unpack '(SL)3 SL',   pack '(SL)*', 67..74;
    is("@a", "@b");
}

{  # Repeat count [SUBEXPR]
   my @codes = qw( x A Z a c C B b H h s v n S i I l V N L p P f F d
		   s! S! i! I! l! L! j J);
   my $G;
   if (eval { pack 'q', 1 } ) {
     push @codes, qw(q Q);
   } else {
     push @codes, qw(c C);	# Keep the count the same
   }
   if (eval { pack 'D', 1 } ) {
     push @codes, 'D';
   } else {
     push @codes, 'd';	# Keep the count the same
   }

   my %val;
   @val{@codes} = map { / [Xx]  (?{ undef })
			| [AZa] (?{ 'something' })
			| C     (?{ 214 })
			| c     (?{ 114 })
			| [Bb]  (?{ '101' })
			| [Hh]  (?{ 'b8' })
			| [svnSiIlVNLqQjJ]  (?{ 10111 })
			| [FfDd]  (?{ 1.36514538e67 })
			| [pP]  (?{ "try this buffer" })
			/x; $^R } @codes;
   my @end = (0x12345678, 0x23456781, 0x35465768, 0x15263748);
   my $end = "N4";

   for my $type (@codes) {
     my @list = $val{$type};
     @list = () unless defined $list[0];
     for my $count ('', '3', '[11]') {
       my $c = 1;
       $c = $1 if $count =~ /(\d+)/;
       my @list1 = @list;
       @list1 = (@list1) x $c unless $type =~ /[XxAaZBbHhP]/;
       for my $groupend ('', ')2', ')[8]') {
	   my $groupbegin = ($groupend ? '(' : '');
	   $c = 1;
	   $c = $1 if $groupend =~ /(\d+)/;
	   my @list2 = (@list1) x $c;

	   my $junk1 = "$groupbegin $type$count $groupend";
	   # print "# junk1=$junk1\n";
	   my $p = pack $junk1, @list2;
	   my $half = int( (length $p)/2 );
	   for my $move ('', "X$half", "X!$half", 'x1', 'x!8', "x$half") {
	     my $junk = "$junk1 $move";
	     # print "# junk='$junk', list=(@list2)\n";
	     $p = pack "$junk $end", @list2, @end;
	     my @l = unpack "x[$junk] $end", $p;
	     is(scalar @l, scalar @end);
	     is("@l", "@end", "skipping x[$junk]");
	   }
       }
     }
   }
}

# / is recognized after spaces in scalar context
# XXXX no spaces are allowed in pack...  In pack only before the slash...
is(scalar unpack('A /A Z20', pack 'A/A* Z20', 'bcde', 'xxxxx'), 'bcde');
is(scalar unpack('A /A /A Z20', '3004bcde'), 'bcde');

{ # X! and x!
  my $t = 'C[3]  x!8 C[2]';
  my @a = (0x73..0x77);
  my $p = pack($t, @a);
  is($p, "\x73\x74\x75\0\0\0\0\0\x76\x77");
  my @b = unpack $t, $p;
  is(scalar @b, scalar @a);
  is("@b", "@a", 'x!8');
  $t = 'x[5] C[6] X!8 C[2]';
  @a = (0x73..0x7a);
  $p = pack($t, @a);
  is($p, "\0\0\0\0\0\x73\x74\x75\x79\x7a");
  @b = unpack $t, $p;
  @a = (0x73..0x75, 0x79, 0x7a, 0x79, 0x7a);
  is(scalar @b, scalar @a);
  is("@b", "@a");
}

{ # struct {char c1; double d; char cc[2];}
  my $t = 'C x![d] d C[2]';
  my @a = (173, 1.283476517e-45, 42, 215);
  my $p = pack $t, @a;
  ok( length $p);
  my @b = unpack "$t X[$t] $t", $p;	# Extract, step back, extract again
  is(scalar @b, 2 * scalar @a);
  $b = "@b";
  $b =~ s/(?:17000+|16999+)\d+(e-45) /17$1 /gi; # stringification is gamble
  is($b, "@a @a");

  my $warning;
  local $SIG{__WARN__} = sub {
      $warning = $_[0];
  };
  @b = unpack "x[C] x[$t] X[$t] X[C] $t", "$p\0";

  is($warning, undef);
  is(scalar @b, scalar @a);
  $b = "@b";
  $b =~ s/(?:17000+|16999+)\d+(e-45) /17$1 /gi; # stringification is gamble
  is($b, "@a");
}

is(length(pack("j", 0)), $Config{ivsize});
is(length(pack("J", 0)), $Config{uvsize});
is(length(pack("F", 0)), $Config{nvsize});

numbers ('j', -2147483648, -1, 0, 1, 2147483647);
numbers ('J', 0, 1, 2147483647, 2147483648, 4294967295);
numbers ('F', -(2**34), -1, 0, 1, 2**34);
SKIP: {
    my $t = eval { unpack("D*", pack("D", 12.34)) };

    skip "Long doubles not in use", 56 if $@ =~ /Invalid type in pack/;

    is(length(pack("D", 0)), $Config{longdblsize});
    numbers ('D', -(2**34), -1, 0, 1, 2**34);
}

# Maybe this knowledge needs to be "global" for all of pack.t
# Or a "can checksum" which would effectively be all the number types"
my %cant_checksum = map {$_=> 1} qw(A Z u w);
# not a b B h H
foreach my $template (qw(A Z c C s S i I l L n N v V q Q j J f d F D u U w)) {
  SKIP: {
    my $packed = eval {pack "${template}4", 1, 4, 9, 16};
    if ($@) {
      die unless $@ =~ /Invalid type in pack: '$template'/;
      skip ("$template not supported on this perl",
            $cant_checksum{$template} ? 4 : 8);
    }
    my @unpack4 = unpack "${template}4", $packed;
    my @unpack = unpack "${template}*", $packed;
    my @unpack1 = unpack "${template}", $packed;
    my @unpack1s = scalar unpack "${template}", $packed;
    my @unpack4s = scalar unpack "${template}4", $packed;
    my @unpacks = scalar unpack "${template}*", $packed;

    my @tests = ( ["${template}4 vs ${template}*", \@unpack4, \@unpack],
                  ["scalar ${template} ${template}", \@unpack1s, \@unpack1],
                  ["scalar ${template}4 vs ${template}", \@unpack4s, \@unpack1],
                  ["scalar ${template}* vs ${template}", \@unpacks, \@unpack1],
                );

    unless ($cant_checksum{$template}) {
      my @unpack4_c = unpack "\%${template}4", $packed;
      my @unpack_c = unpack "\%${template}*", $packed;
      my @unpack1_c = unpack "\%${template}", $packed;
      my @unpack1s_c = scalar unpack "\%${template}", $packed;
      my @unpack4s_c = scalar unpack "\%${template}4", $packed;
      my @unpacks_c = scalar unpack "\%${template}*", $packed;

      push @tests,
        ( ["% ${template}4 vs ${template}*", \@unpack4_c, \@unpack_c],
          ["% scalar ${template} ${template}", \@unpack1s_c, \@unpack1_c],
          ["% scalar ${template}4 vs ${template}*", \@unpack4s_c, \@unpack_c],
          ["% scalar ${template}* vs ${template}*", \@unpacks_c, \@unpack_c],
        );
    }
    foreach my $test (@tests) {
      ok (list_eq ($test->[1], $test->[2]), $test->[0]) ||
        printf "# unpack gave %s expected %s\n",
          encode_list (@{$test->[1]}), encode_list (@{$test->[2]});
    }
  }
}
