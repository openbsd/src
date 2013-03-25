use strict;
use warnings;

use Version::Requirements;

use Test::More 0.88;

{
  my $req = Version::Requirements->new->add_minimum(Foo => 1);

  ok(  $req->accepts_module(Foo => 1));
  ok(! $req->accepts_module(Foo => 0));
}

{
  my $req = Version::Requirements->new->add_maximum(Foo => 1);

  ok(  $req->accepts_module(Foo => 1));
  ok(! $req->accepts_module(Foo => 2));
}

{
  my $req = Version::Requirements->new->add_exclusion(Foo => 1);

  ok(  $req->accepts_module(Foo => 0));
  ok(! $req->accepts_module(Foo => 1));
}

done_testing;
