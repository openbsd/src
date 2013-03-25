package Module::Signature; # mocked
use strict;
use warnings;
our $VERSION = 999;

sub sign {
  open my $fh, ">", "SIGNATURE";
  print {$fh} "SIGNATURE";
}

1;
