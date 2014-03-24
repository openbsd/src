use strict;
use warnings;

use CPAN::Meta::Requirements;

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

  my $req = CPAN::Meta::Requirements->from_string_hash($string_hash);

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

  dies_ok { CPAN::Meta::Requirements->from_string_hash($string_hash) }
    qr/Can't convert/,
    "we die when we can't understand a version spec";
}

{
  my $string_hash = {
    Left   => 10,
    Shared => undef,
    Right  => 18,
  };

  my $warning;
  local $SIG{__WARN__} = sub { $warning = join("\n",@_) };

  my $req = CPAN::Meta::Requirements->from_string_hash($string_hash);

  is(
    $req->as_string_hash->{Shared}, 0,
    "undef requirement treated as '0'",
  );

  like ($warning, qr/Undefined requirement.*treated as '0'/, "undef requirement warns");

}

done_testing;
