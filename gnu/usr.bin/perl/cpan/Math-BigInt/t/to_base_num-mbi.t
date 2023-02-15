# -*- mode: perl; -*-

use strict;
use warnings;

use Test::More tests => 14;

my $class;

BEGIN { $class = 'Math::BigInt'; }
BEGIN { use_ok($class); }

# For simplicity, we use the same data in the test programs for to_base_num() and
# from_base_num().

my @data =
  (
   [ 0, 2, [ 0 ] ],
   [ 1, 2, [ 1 ] ],
   [ 2, 2, [ 1, 0 ] ],
   [ 3, 2, [ 1, 1, ] ],
   [ 4, 2, [ 1, 0, 0 ] ],

   [ 0, 10, [ 0 ] ],
   [ 1, 10, [ 1 ] ],
   [ 12, 10, [ 1, 2 ] ],
   [ 123, 10, [ 1, 2, 3 ] ],
   [ 1230, 10, [ 1, 2, 3, 0 ] ],

   [ "123456789", 100, [ 1, 23, 45, 67, 89 ] ],

   [ "1234567890" x 3,
     "987654321",
     [ "128", "142745769", "763888804", "574845669" ]],

   [ "1234567890" x 5,
     "987654321" x 3,
     [ "12499999874843750102814", "447551941015330718793208596" ]],
  );

for (my $i = 0 ; $i <= $#data ; ++ $i) {
    my @in = ($data[$i][0], $data[$i][1]);
    my $out = $data[$i][2];

    my ($x, $xo, $y);
    my $test = qq|\$x = $class -> new("$in[0]");|;
    $test .= qq| \$xo = \$x -> copy();|;
    $test .= qq| \$y = \$x -> to_base_num("$in[1]")|;

    eval $test;
    die "\nThe following test died when eval()'ed. This indicates a ",
      "broken test\n\n    $test\n\nThe error message was\n\n    $@\n"
      if $@;

    subtest $test, sub {
        plan tests => 4,

        is($x, $xo, "invocand object was not changed");
        is(ref($y), 'ARRAY', "output arg is an ARRAY ref");
        ok(! grep(ref() ne $class, @$y), "every array element is a $class");
        is_deeply($y, $out, 'every array element has the right value');
    };
}
