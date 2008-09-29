#!perl
use strict;
use warnings;

# This test file exists to show that Time::Piece can be subclassed and that its
# methods will return objects of the class on which they're called.

use Test::More 'no_plan';

BEGIN { use_ok('Time::Piece'); }

my $class = 'Time::Piece::Twin';

for my $method (qw(new localtime gmtime)) {
  my $piece = $class->$method;
  isa_ok($piece, $class, "timepiece made via $method");
}

{
  my $piece = $class->strptime("2005-01-01", "%Y-%m-%d");
  isa_ok($piece, $class, "timepiece made via strptime");
}

{
  my $piece = $class->new;
  isa_ok($piece, $class, "timepiece made via new (again)");

  my $sum = $piece + 86_400;
  isa_ok($sum, $class, "tomorrow via addition operator");

  my $diff = $piece - 86_400;
  isa_ok($diff, $class, "yesterday via subtraction operator");
}

{
  # let's verify that we can use gmtime from T::P without the export magic
  my $piece = Time::Piece::gmtime;
  isa_ok($piece, "Time::Piece", "object created via full-qualified gmtime");
  isnt(ref $piece, 'Time::Piece::Twin', "it's not a Twin");
}

## below is our doppelgaenger package
{
  package Time::Piece::Twin;
  use base qw(Time::Piece);
  # this package is identical, but will be ->isa('Time::Piece::Twin');
}

{
  my $class = "Time::Piece::NumString";
  my $piece = $class->strptime ("2006", "%Y");
  is (2007 - $piece, 1,
      "subtract attempts stringify for unrecognized objects.");
}

## Below is a package which only changes the stringify function.
{
  package Time::Piece::NumString;
  use base qw(Time::Piece);
  use overload '""' => \&_stringify;
  sub _stringify
  {
    my $self = shift;
    return $self->strftime ("%Y");
  }
}
