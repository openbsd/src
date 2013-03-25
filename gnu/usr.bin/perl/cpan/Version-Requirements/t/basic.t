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
  my $req = Version::Requirements->new;

  $req->add_minimum('Foo::Bar' => 10);
  $req->add_minimum('Foo::Bar' => 0);
  $req->add_minimum('Foo::Bar' => 2);

  $req->add_minimum('Foo::Baz' => version->declare('v1.2.3'));

  $req->add_minimum('Foo::Undef' => undef);

  is_deeply(
    $req->as_string_hash,
    {
      'Foo::Bar'   => 10,
      'Foo::Baz'   => 'v1.2.3',
      'Foo::Undef' => 0,
    },
    "some basic minimums",
  );

  ok($req->is_simple, "just minimums? simple");
}

{
  my $req = Version::Requirements->new;
  $req->add_maximum(Foo => 1);
  is_deeply($req->as_string_hash, { Foo => '<= 1' }, "max only");

  ok(! $req->is_simple, "maximums? not simple");
}

{
  my $req = Version::Requirements->new;
  $req->add_exclusion(Foo => 1);
  $req->add_exclusion(Foo => 2);

  # Why would you ever do this?? -- rjbs, 2010-02-20
  is_deeply($req->as_string_hash, { Foo => '!= 1, != 2' }, "excl only");
}

{
  my $req = Version::Requirements->new;

  $req->add_minimum(Foo => 1);
  $req->add_maximum(Foo => 2);

  is_deeply(
    $req->as_string_hash,
    {
      Foo => '>= 1, <= 2',
    },
    "min and max",
  );

  $req->add_maximum(Foo => 3);

  is_deeply(
    $req->as_string_hash,
    {
      Foo => '>= 1, <= 2',
    },
    "exclusions already outside range do not matter",
  );

  $req->add_exclusion(Foo => 1.5);

  is_deeply(
    $req->as_string_hash,
    {
      Foo => '>= 1, <= 2, != 1.5',
    },
    "exclusions",
  );

  $req->add_minimum(Foo => 1.6);

  is_deeply(
    $req->as_string_hash,
    {
      Foo => '>= 1.6, <= 2',
    },
    "exclusions go away when made irrelevant",
  );
}

{
  my $req = Version::Requirements->new;

  $req->add_minimum(Foo => 1);
  $req->add_exclusion(Foo => 1);
  $req->add_maximum(Foo => 2);

  is_deeply(
    $req->as_string_hash,
    {
      Foo => '> 1, <= 2',
    },
    "we can exclude an endpoint",
  );
}

{
  my $req = Version::Requirements->new;
  $req->add_minimum(Foo => 1);

  $req->add_exclusion(Foo => 1);

  dies_ok { $req->add_maximum(Foo => 1); }
    qr/excluded all/,
    "can't exclude all values" ;
}

{
  my $req = Version::Requirements->new;
  $req->add_minimum(Foo => 1);
  dies_ok {$req->exact_version(Foo => 0.5); }
    qr/outside of range/,
    "can't add outside-range exact spec to range";
}

{
  my $req = Version::Requirements->new;
  $req->add_minimum(Foo => 1);
  dies_ok { $req->add_maximum(Foo => 0.5); }
    qr/minimum exceeds maximum/,
    "maximum must exceed (or equal) minimum";

  $req = Version::Requirements->new;
  $req->add_maximum(Foo => 0.5);
  dies_ok { $req->add_minimum(Foo => 1); }
    qr/minimum exceeds maximum/,
    "maximum must exceed (or equal) minimum";
}

{
  my $req = Version::Requirements->new;

  $req->add_minimum(Foo => 1);
  $req->add_maximum(Foo => 1);

  $req->add_maximum(Foo => 2); # ignored
  $req->add_minimum(Foo => 0); # ignored
  $req->add_exclusion(Foo => .5); # ignored

  is_deeply(
    $req->as_string_hash,
    {
      'Foo' => '== 1',
    },
    "if min==max, becomes exact requirement",
  );
}

{
  my $req = Version::Requirements->new;
  $req->add_minimum(Foo => 1);
  $req->add_exclusion(Foo => 0);
  $req->add_maximum(Foo => 3);
  $req->add_exclusion(Foo => 4);

  $req->add_exclusion(Foo => 2);
  $req->add_exclusion(Foo => 2);

  is_deeply(
    $req->as_string_hash,
    {
      Foo => '>= 1, <= 3, != 2',
    },
    'test exclusion-skipping',
  );
}

sub foo_1 {
  my $req = Version::Requirements->new;
  $req->exact_version(Foo => 1);
  return $req;
}

{
  my $req = foo_1;

  $req->exact_version(Foo => 1); # ignored

  is_deeply($req->as_string_hash, { Foo => '== 1' }, "exact requirement");

  dies_ok { $req->exact_version(Foo => 2); }
    qr/unequal/,
    "can't exactly specify differing versions" ;

  $req = foo_1;
  $req->add_minimum(Foo => 0); # ignored
  $req->add_maximum(Foo => 2); # ignored

  dies_ok { $req->add_maximum(Foo => 0); } qr/maximum below/, "max < fixed";

  $req = foo_1;
  dies_ok { $req->add_minimum(Foo => 2); } qr/minimum above/, "min > fixed";

  $req = foo_1;
  $req->add_exclusion(Foo => 8); # ignored
  dies_ok { $req->add_exclusion(Foo => 1); } qr/excluded exact/, "!= && ==";
}

done_testing;
