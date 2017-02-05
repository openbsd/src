#!perl

# test inf/NaN handling all in one place
# Thanx to Jarkko for the excellent explanations and the tables

use strict;
use warnings;
use lib 't';

use Test::More tests => 2052;

use Math::BigInt;
use Math::BigFloat;
use Math::BigInt::Subclass;
use Math::BigFloat::Subclass;

my @biclasses = qw/ Math::BigInt   Math::BigInt::Subclass   /;
my @bfclasses = qw/ Math::BigFloat Math::BigFloat::Subclass /;

my (@args, $x, $y, $z);

# +

foreach (qw/

    -inf:-inf:-inf
    -1:-inf:-inf
    -0:-inf:-inf
    0:-inf:-inf
    1:-inf:-inf
    inf:-inf:NaN
    NaN:-inf:NaN

    -inf:-1:-inf
    -1:-1:-2
    -0:-1:-1
    0:-1:-1
    1:-1:0
    inf:-1:inf
    NaN:-1:NaN

    -inf:0:-inf
    -1:0:-1
    -0:0:0
    0:0:0
    1:0:1
    inf:0:inf
    NaN:0:NaN

    -inf:1:-inf
    -1:1:0
    -0:1:1
    0:1:1
    1:1:2
    inf:1:inf
    NaN:1:NaN

    -inf:inf:NaN
    -1:inf:inf
    -0:inf:inf
    0:inf:inf
    1:inf:inf
    inf:inf:inf
    NaN:inf:NaN

    -inf:NaN:NaN
    -1:NaN:NaN
    -0:NaN:NaN
    0:NaN:NaN
    1:NaN:NaN
    inf:NaN:NaN
    NaN:NaN:NaN

  /)
{
    @args = split /:/, $_;
    for my $class (@biclasses, @bfclasses) {
        $x = $class->new($args[0]);
        $y = $class->new($args[1]);
        $args[2] = '0' if $args[2] eq '-0'; # Math::Big(Int|Float) has no -0
        my $r = $x->badd($y);

        is($x->bstr(), $args[2], "x $class $args[0] + $args[1]");
        is($x->bstr(), $args[2], "r $class $args[0] + $args[1]");
    }
}

# -

foreach (qw/

    -inf:-inf:NaN
    -1:-inf:inf
    -0:-inf:inf
    0:-inf:inf
    1:-inf:inf
    inf:-inf:inf
    NaN:-inf:NaN

    -inf:-1:-inf
    -1:-1:0
    -0:-1:1
    0:-1:1
    1:-1:2
    inf:-1:inf
    NaN:-1:NaN

    -inf:0:-inf
    -1:0:-1
    -0:0:-0
    0:0:0
    1:0:1
    inf:0:inf
    NaN:0:NaN

    -inf:1:-inf
    -1:1:-2
    -0:1:-1
    0:1:-1
    1:1:0
    inf:1:inf
    NaN:1:NaN

    -inf:inf:-inf
    -1:inf:-inf
    -0:inf:-inf
    0:inf:-inf
    1:inf:-inf
    inf:inf:NaN
    NaN:inf:NaN

    -inf:NaN:NaN
    -1:NaN:NaN
    -0:NaN:NaN
    0:NaN:NaN
    1:NaN:NaN
    inf:NaN:NaN
    NaN:NaN:NaN

  /)
{
    @args = split /:/, $_;
    for my $class (@biclasses, @bfclasses) {
        $x = $class->new($args[0]);
        $y = $class->new($args[1]);
        $args[2] = '0' if $args[2] eq '-0'; # Math::Big(Int|Float) has no -0
        my $r = $x->bsub($y);

        is($x->bstr(), $args[2], "x $class $args[0] - $args[1]");
        is($r->bstr(), $args[2], "r $class $args[0] - $args[1]");
    }
}

# *

foreach (qw/

    -inf:-inf:inf
    -1:-inf:inf
    -0:-inf:NaN
    0:-inf:NaN
    1:-inf:-inf
    inf:-inf:-inf
    NaN:-inf:NaN

    -inf:-1:inf
    -1:-1:1
    -0:-1:0
    0:-1:-0
    1:-1:-1
    inf:-1:-inf
    NaN:-1:NaN

    -inf:0:NaN
    -1:0:-0
    -0:0:-0
    0:0:0
    1:0:0
    inf:0:NaN
    NaN:0:NaN

    -inf:1:-inf
    -1:1:-1
    -0:1:-0
    0:1:0
    1:1:1
    inf:1:inf
    NaN:1:NaN

    -inf:inf:-inf
    -1:inf:-inf
    -0:inf:NaN
    0:inf:NaN
    1:inf:inf
    inf:inf:inf
    NaN:inf:NaN

    -inf:NaN:NaN
    -1:NaN:NaN
    -0:NaN:NaN
    0:NaN:NaN
    1:NaN:NaN
    inf:NaN:NaN
    NaN:NaN:NaN

    /)
{
    @args = split /:/, $_;
    for my $class (@biclasses, @bfclasses) {
        $x = $class->new($args[0]);
        $y = $class->new($args[1]);
        $args[2] = '0' if $args[2] eq '-0'; # Math::Big(Int|Float) has no -0
        my $r = $x->bmul($y);

        is($x->bstr(), $args[2], "x $class $args[0] * $args[1]");
        is($r->bstr(), $args[2], "r $class $args[0] * $args[1]");
    }
}

# /

foreach (qw/

    -inf:-inf:NaN
    -1:-inf:0
    -0:-inf:0
    0:-inf:-0
    1:-inf:-1
    inf:-inf:NaN
    NaN:-inf:NaN

    -inf:-1:inf
    -1:-1:1
    -0:-1:0
    0:-1:-0
    1:-1:-1
    inf:-1:-inf
    NaN:-1:NaN

    -inf:0:-inf
    -1:0:-inf
    -0:0:NaN
    0:0:NaN
    1:0:inf
    inf:0:inf
    NaN:0:NaN

    -inf:1:-inf
    -1:1:-1
    -0:1:-0
    0:1:0
    1:1:1
    inf:1:inf
    NaN:1:NaN

    -inf:inf:NaN
    -1:inf:-1
    -0:inf:-0
    0:inf:0
    1:inf:0
    inf:inf:NaN
    NaN:inf:NaN

    -inf:NaN:NaN
    -1:NaN:NaN
    -0:NaN:NaN
    0:NaN:NaN
    1:NaN:NaN
    inf:NaN:NaN
    NaN:NaN:NaN

    /)
{
    @args = split /:/, $_;
    for my $class (@biclasses, @bfclasses) {
        $x = $class->new($args[0]);
        $y = $class->new($args[1]);
        $args[2] = '0' if $args[2] eq '-0'; # Math::Big(Int|Float) has no -0

        my $t = $x->copy();
        my $tmod = $t->copy();

        # bdiv in scalar context
        unless ($class =~ /^Math::BigFloat/) {
            my $r = $x->bdiv($y);
            is($x->bstr(), $args[2], "x $class $args[0] / $args[1]");
            is($r->bstr(), $args[2], "r $class $args[0] / $args[1]");
        }

        # bmod and bdiv in list context
        my ($d, $rem) = $t->bdiv($y);

        # bdiv in list context
        is($t->bstr(), $args[2], "t $class $args[0] / $args[1]");
        is($d->bstr(), $args[2], "d $class $args[0] / $args[1]");

        # bmod
        my $m = $tmod->bmod($y);

        # bmod() agrees with bdiv?
        is($m->bstr(), $rem->bstr(), "m $class $args[0] % $args[1]");
        # bmod() return agrees with set value?
        is($tmod->bstr(), $m->bstr(), "o $class $args[0] % $args[1]");
    }
}

# /

foreach (qw/

    -inf:-inf:NaN
    -1:-inf:0
    -0:-inf:0
    0:-inf:-0
    1:-inf:-0
    inf:-inf:NaN
    NaN:-inf:NaN

    -inf:-1:inf
    -1:-1:1
    -0:-1:0
    0:-1:-0
    1:-1:-1
    inf:-1:-inf
    NaN:-1:NaN

    -inf:0:-inf
    -1:0:-inf
    -0:0:NaN
    0:0:NaN
    1:0:inf
    inf:0:inf
    NaN:0:NaN

    -inf:1:-inf
    -1:1:-1
    -0:1:-0
    0:1:0
    1:1:1
    inf:1:inf
    NaN:1:NaN

    -inf:inf:NaN
    -1:inf:-0
    -0:inf:-0
    0:inf:0
    1:inf:0
    inf:inf:NaN
    NaN:inf:NaN

    -inf:NaN:NaN
    -1:NaN:NaN
    -0:NaN:NaN
    0:NaN:NaN
    1:NaN:NaN
    inf:NaN:NaN
    NaN:NaN:NaN

    /)
{
    @args = split /:/, $_;
    for my $class (@bfclasses) {
        $x = $class->new($args[0]);
        $y = $class->new($args[1]);
        $args[2] = '0' if $args[2] eq '-0'; # Math::Big(Int|Float) has no -0

        my $t = $x->copy();
        my $tmod = $t->copy();

        # bdiv in scalar context
        my $r = $x->bdiv($y);
        is($x->bstr(), $args[2], "x $class $args[0] / $args[1]");
        is($r->bstr(), $args[2], "r $class $args[0] / $args[1]");
    }
}

#############################################################################
# overloaded comparisons

foreach my $c (@biclasses, @bfclasses) {
    my $x = $c->bnan();
    my $y = $c->bnan();         # test with two different objects, too
    my $z = $c->bzero();

    is($x == $y, '', 'NaN == NaN: ""');
    is($x != $y, 1,  'NaN != NaN: 1');

    is($x == $x, '', 'NaN == NaN: ""');
    is($x != $x, 1,  'NaN != NaN: 1');

    is($z != $x, 1,  '0 != NaN: 1');
    is($z == $x, '', '0 == NaN: ""');

    is($z < $x,  '', '0 < NaN: ""');
    is($z <= $x, '', '0 <= NaN: ""');
    is($z >= $x, '', '0 >= NaN: ""');
    #is($z > $x,  '', '0 > NaN: ""');   # Bug! Todo: fix it!
}

# All done.
