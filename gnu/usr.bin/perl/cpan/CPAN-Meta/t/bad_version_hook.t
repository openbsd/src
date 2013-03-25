use strict;
use warnings;

use CPAN::Meta::Requirements;
use version;

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

sub _fixit { return version->new(42) }

{
  my $req = CPAN::Meta::Requirements->new( {bad_version_hook => \&_fixit} );

  $req->add_minimum('Foo::Bar' => 10);
  $req->add_minimum('Foo::Baz' => 'invalid_version');

  is_deeply(
    $req->as_string_hash,
    {
      'Foo::Bar'   => 10,
      'Foo::Baz'   => 42,
    },
    "hook fixes invalid version",
  );
}

{
  my $req = CPAN::Meta::Requirements->new( {bad_version_hook => sub { 0 }} );

  dies_ok { $req->add_minimum('Foo::Baz' => 'invalid_version') }
    qr/Invalid version/,
    "dies if hook doesn't return version object";

}


done_testing;
