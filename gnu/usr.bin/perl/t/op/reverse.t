#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

plan tests => 23;

is(reverse("abc"), "cba");

$_ = "foobar";
is(reverse(), "raboof");

{
    my @a = ("foo", "bar");
    my @b = reverse @a;

    is($b[0], $a[1]);
    is($b[1], $a[0]);
}

{
    my @a = (1, 2, 3, 4);
    @a = reverse @a;
    is("@a", "4 3 2 1");

    delete $a[1];
    @a = reverse @a;
    ok(!exists $a[2]);
    is($a[0] . $a[1] . $a[3], '124');

    @a = (5, 6, 7, 8, 9);
    @a = reverse @a;
    is("@a", "9 8 7 6 5");

    delete $a[3];
    @a = reverse @a;
    ok(!exists $a[1]);
    is($a[0] . $a[2] . $a[3] . $a[4], '5789');

    delete $a[2];
    @a = reverse @a;
    ok(!exists $a[2] && !exists $a[3]);
    is($a[0] . $a[1] . $a[4], '985');

    my @empty;
    @empty = reverse @empty;
    is("@empty", "");
}

use Tie::Array;

{
    tie my @a, 'Tie::StdArray';

    @a = (1, 2, 3, 4);
    @a = reverse @a;
    is("@a", "4 3 2 1");

    delete $a[1];
    @a = reverse @a;
    ok(!exists $a[2]);
    is($a[0] . $a[1] . $a[3], '124');

    @a = (5, 6, 7, 8, 9);
    @a = reverse @a;
    is("@a", "9 8 7 6 5");

    delete $a[3];
    @a = reverse @a;
    ok(!exists $a[1]);
    is($a[0] . $a[2] . $a[3] . $a[4], '5789');

    delete $a[2];
    @a = reverse @a;
    ok(!exists $a[2] && !exists $a[3]);
    is($a[0] . $a[1] . $a[4], '985');

    tie my @empty, "Tie::StdArray";
    @empty = reverse @empty;
    is(scalar(@empty), 0);
}

{
    # Unicode.

    my $a = "\x{263A}\x{263A}x\x{263A}y\x{263A}";
    my $b = scalar reverse($a);
    my $c = scalar reverse($b);
    is($a, $c);
}
