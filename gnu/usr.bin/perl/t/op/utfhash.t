#!./perl -w

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';

    plan(tests => 97);
}

use strict;

# Two hashes one will all keys 8-bit possible (initially), other
# with a utf8 requiring key from the outset.

my %hash8 = ( "\xff" => 0xff,
              "\x7f" => 0x7f,
            );
my %hashu = ( "\xff" => 0xff,
              "\x7f" => 0x7f,
              "\x{1ff}" => 0x1ff,
            );

# Check that we can find the 8-bit things by various litterals
is($hash8{"\x{00ff}"},0xFF);
is($hash8{"\x{007f}"},0x7F);
is($hash8{"\xff"},0xFF);
is($hash8{"\x7f"},0x7F);
is($hashu{"\x{00ff}"},0xFF);
is($hashu{"\x{007f}"},0x7F);
is($hashu{"\xff"},0xFF);
is($hashu{"\x7f"},0x7F);

# Now try same thing with variables forced into various forms.
foreach my $a ("\x7f","\xff")
 {
  utf8::upgrade($a);
  is($hash8{$a},ord($a));
  is($hashu{$a},ord($a));
  utf8::downgrade($a);
  is($hash8{$a},ord($a));
  is($hashu{$a},ord($a));
  my $b = $a.chr(100);
  chop($b);
  is($hash8{$b},ord($b));
  is($hashu{$b},ord($b));
 }

# Check we have not got an spurious extra keys
is(join('',sort { ord $a <=> ord $b } keys %hash8),"\x7f\xff");
is(join('',sort { ord $a <=> ord $b } keys %hashu),"\x7f\xff\x{1ff}");

# Now add a utf8 key to the 8-bit hash
$hash8{chr(0x1ff)} = 0x1ff;

# Check we have not got an spurious extra keys
is(join('',sort { ord $a <=> ord $b } keys %hash8),"\x7f\xff\x{1ff}");

foreach my $a ("\x7f","\xff","\x{1ff}")
 {
  utf8::upgrade($a);
  is($hash8{$a},ord($a));
  my $b = $a.chr(100);
  chop($b);
  is($hash8{$b},ord($b));
 }

# and remove utf8 from the other hash
is(delete $hashu{chr(0x1ff)},0x1ff);
is(join('',sort keys %hashu),"\x7f\xff");

foreach my $a ("\x7f","\xff")
 {
  utf8::upgrade($a);
  is($hashu{$a},ord($a));
  utf8::downgrade($a);
  is($hashu{$a},ord($a));
  my $b = $a.chr(100);
  chop($b);
  is($hashu{$b},ord($b));
 }



{
  print "# Unicode hash keys and \\w\n";
  # This is not really a regex test but regexes bring
  # out the issue nicely.
  use strict;
  my $u3 = "f\x{df}\x{100}";
  my $u2 = substr($u3,0,2);
  my $u1 = substr($u2,0,1);
  my $u0 = chr (0xdf)x4; # Make this 4 chars so that all lengths are distinct.

  my @u = ($u0, $u1, $u2, $u3);

  while (@u) {
    my %u = (map {( $_, $_)} @u);
    my $keys = scalar @u;
    $keys .= ($keys == 1) ? " key" : " keys";

    for (keys %u) {
        my $l = 0 + /^\w+$/;
        my $r = 0 + $u{$_} =~ /^\w+$/;
	is ($l, $r, "\\w on keys with $keys, key of length " . length $_);
    }

    my $more;
    do {
      $more = 0;
      # Want to do this direct, rather than copying to a temporary variable
      # The first time each will return key and value at the start of the hash.
      # each will return () after we've done the last pair. $more won't get
      # set then, and the do will exit.
      for (each %u) {
        $more = 1;
        my $l = 0 + /^\w+$/;
        my $r = 0 + $u{$_} =~ /^\w+$/;
        is ($l, $r, "\\w on each, with $keys, key of length " . length $_);
      }
    } while ($more);

    for (%u) {
      my $l = 0 + /^\w+$/;
      my $r = 0 + $u{$_} =~ /^\w+$/;
      is ($l, $r, "\\w on hash with $keys, key of length " . length $_);
    }
    pop @u;
    undef %u;
  }
}

{
  my $utf8_sz = my $bytes_sz = "\x{df}";
  $utf8_sz .= chr 256;
  chop ($utf8_sz);

  my (%bytes_first, %utf8_first);

  $bytes_first{$bytes_sz} = $bytes_sz;

  for (keys %bytes_first) {
    my $l = 0 + /^\w+$/;
    my $r = 0 + $bytes_first{$_} =~ /^\w+$/;
    is ($l, $r, "\\w on each, bytes");
  }

  $bytes_first{$utf8_sz} = $utf8_sz;

  for (keys %bytes_first) {
    my $l = 0 + /^\w+$/;
    my $r = 0 + $bytes_first{$_} =~ /^\w+$/;
    is ($l, $r, "\\w on each, bytes now utf8");
  }

  $utf8_first{$utf8_sz} = $utf8_sz;

  for (keys %utf8_first) {
    my $l = 0 + /^\w+$/;
    my $r = 0 + $utf8_first{$_} =~ /^\w+$/;
    is ($l, $r, "\\w on each, utf8");
  }

  $utf8_first{$bytes_sz} = $bytes_sz;

  for (keys %utf8_first) {
    my $l = 0 + /^\w+$/;
    my $r = 0 + $utf8_first{$_} =~ /^\w+$/;
    is ($l, $r, "\\w on each, utf8 now bytes");
  }

}

{
  # See if utf8 barewords work [perl #22969]
  use utf8;
  my %hash = (тест => 123);
  is($hash{тест}, $hash{'тест'});
  is($hash{тест}, 123);
  is($hash{'тест'}, 123);
  %hash = (тест => 123);
  is($hash{тест}, $hash{'тест'});
  is($hash{тест}, 123);
  is($hash{'тест'}, 123);
}
