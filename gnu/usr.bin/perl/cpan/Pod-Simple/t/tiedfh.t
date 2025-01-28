# Testing tied output filehandle
use strict;
use warnings;
use Test::More tests => 6;

use Pod::Simple::TiedOutFH;

print "# Sanity test of Perl and Pod::Simple::TiedOutFH\n";

{
  my $x = 'abc';
  my $out = Pod::Simple::TiedOutFH->handle_on($x);
  print $out "Puppies\n";
  print $out "rrrrr";
  print $out "uffuff!";
  is $x, "abcPuppies\nrrrrruffuff!";
  undef $out;
  is $x, "abcPuppies\nrrrrruffuff!";
}

# Now test that we can have two different strings.
{
  my $x1 = 'abc';
  my $x2 = 'xyz';
  my $out1 = Pod::Simple::TiedOutFH->handle_on($x1);
  my $out2 = Pod::Simple::TiedOutFH->handle_on($x2);

  print $out1 "Puppies\n";
  print $out2 "Kitties\n";
  print $out2 "mmmmm";
  print $out1 "rrrrr";
  print $out2 "iaooowwlllllllrrr!\n";
  print $out1 "uffuff!";

  is $x1, "abcPuppies\nrrrrruffuff!",              "out1 test";
  is $x2, "xyzKitties\nmmmmmiaooowwlllllllrrr!\n", "out2 test";

  undef $out1;
  undef $out2;

  is $x1, "abcPuppies\nrrrrruffuff!",              "out1 test";
  is $x2, "xyzKitties\nmmmmmiaooowwlllllllrrr!\n", "out2 test";
}
