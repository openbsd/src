BEGIN {
    chdir 't' if -d 't';
    @INC = qw(../lib .);
    require "test.pl";
}

plan tests => 10;

sub MyUniClass {
  <<END;
0030	004F
END
}

sub Other::Class {
  <<END;
0040	005F
END
}

sub A::B::Intersection {
  <<END;
+main::MyUniClass
&Other::Class
END
}

sub test_regexp ($$) {
  # test that given string consists of N-1 chars matching $qr1, and 1
  # char matching $qr2
  my ($str, $blk) = @_;

  # constructing these objects here makes the last test loop go much faster
  my $qr1 = qr/(\p{$blk}+)/;
  if ($str =~ $qr1) {
    is($1, substr($str, 0, -1));		# all except last char
  }
  else {
    fail('first N-1 chars did not match');
  }

  my $qr2 = qr/(\P{$blk}+)/;
  if ($str =~ $qr2) {
    is($1, substr($str, -1));			# only last char
  }
  else {
    fail('last char did not match');
  }
}

use strict;

my $str;

if (ord('A') == 193) {
    $str = join "", map chr($_), 0x40, 0x5A, 0x7F, 0x7B, 0x5B, 0x6C, 0x50, 0x7D, 0x4D, 0x5D, 0x5C, 0x4E, 0x6B, 0x60, 0x4B, 0x61, 0xF0 .. 0xF9, 0x7A, 0x5E, 0x4C, 0x7E, 0x6E, 0x6F, 0x7C, 0xC1 .. 0xC9, 0xD1 .. 0xD9, 0xE2 .. 0xE9, 0xAD, 0xE0, 0xBD, 0x5F, 0x6D, 0x79, 0x81 .. 0x89, 0x91 .. 0x96; # IBM-1047
} else {
    $str = join "", map chr($_), 0x20 .. 0x6F;
}

# make sure it finds built-in class
is(($str =~ /(\p{Letter}+)/)[0], 'ABCDEFGHIJKLMNOPQRSTUVWXYZ');
is(($str =~ /(\p{l}+)/)[0], 'ABCDEFGHIJKLMNOPQRSTUVWXYZ');

# make sure it finds user-defined class
is(($str =~ /(\p{MyUniClass}+)/)[0], '0123456789:;<=>?@ABCDEFGHIJKLMNO');

# make sure it finds class in other package
is(($str =~ /(\p{Other::Class}+)/)[0], '@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_');

# make sure it finds class in other OTHER package
is(($str =~ /(\p{A::B::Intersection}+)/)[0], '@ABCDEFGHIJKLMNO');

# lib/unicore/Bc/AL.pl
$str = "\x{070D}\x{070E}\x{070F}\x{0710}\x{0711}";
is(($str =~ /(\P{BidiClass: ArabicLetter}+)/)[0], "\x{070F}");
is(($str =~ /(\P{BidiClass: AL}+)/)[0], "\x{070F}");
is(($str =~ /(\P{BC :ArabicLetter}+)/)[0], "\x{070F}");
is(($str =~ /(\P{bc=AL}+)/)[0], "\x{070F}");

# make sure InGreek works
$str = "[\x{038B}\x{038C}\x{038D}]";

is(($str =~ /(\p{InGreek}+)/)[0], "\x{038B}\x{038C}\x{038D}");

# The other tests that are based on looking at the generated files are now
# in t/re/uniprops.t
