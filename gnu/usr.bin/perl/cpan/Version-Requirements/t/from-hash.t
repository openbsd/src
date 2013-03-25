use strict;
use warnings;

use Version::Requirements;

use Test::More 0.88;

sub dies_ok (&@) {
  my ($code, $qr, $comment) = @_;

  my $lived = eval { $code->(); 1 };

  if ($lived) {
    fail("$comment: did not die");
  } else {
    like($@, $qr, $comment);
  }
}

{
  my $string_hash = {
    Left   => 10,
    Shared => '>= 2, <= 9, != 7',
    Right  => 18,
  };

  my $req = Version::Requirements->from_string_hash($string_hash);

  is_deeply(
    $req->as_string_hash,
    $string_hash,
    "we can load from a string hash",
  );
}

{
  my $string_hash = {
    Left   => 10,
    Shared => '= 2',
    Right  => 18,
  };

  dies_ok { Version::Requirements->from_string_hash($string_hash) }
    qr/illegal/,
    "we die when we can't understand a version spec";
}

done_testing;
