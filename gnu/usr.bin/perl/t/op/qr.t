#!./perl -w

use strict;

require './test.pl';

plan(tests => 18);

sub r {
    return qr/Good/;
}

my $a = r();
isa_ok($a, 'Regexp');
my $b = r();
isa_ok($b, 'Regexp');

my $b1 = $b;

isnt($a + 0, $b + 0, 'Not the same object');

bless $b, 'Pie';

isa_ok($b, 'Pie');
isa_ok($a, 'Regexp');
isa_ok($b1, 'Pie');

my $c = r();
like("$c", qr/Good/);
my $d = r();
like("$d", qr/Good/);

my $d1 = $d;

isnt($c + 0, $d + 0, 'Not the same object');

$$d = 'Bad';

like("$c", qr/Good/);
is($$d, 'Bad');
is($$d1, 'Bad');

# Assignment to an implicitly blessed Regexp object retains the class
# (No different from direct value assignment to any other blessed SV

isa_ok($d, 'Regexp');
like("$d", qr/\ARegexp=SCALAR\(0x[0-9a-f]+\)\z/);

# As does an explicitly blessed Regexp object.

my $e = bless qr/Faux Pie/, 'Stew';

isa_ok($e, 'Stew');
$$e = 'Fake!';

is($$e, 'Fake!');
isa_ok($e, 'Stew');
like("$e", qr/\Stew=SCALAR\(0x[0-9a-f]+\)\z/);
