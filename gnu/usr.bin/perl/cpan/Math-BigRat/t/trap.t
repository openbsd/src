#!perl

# test that config( trap_nan => 1, trap_inf => 1) really works/dies

use strict;
use warnings;

use Test::More tests => 29;

use Math::BigRat;

my $mbi = 'Math::BigRat';
my ($cfg, $x);

foreach my $class ($mbi) {

    # can do and defaults are okay?
    can_ok($class, 'config');
    is($class->config()->{trap_nan}, 0, qq|$class->config()->{trap_nan}|);
    is($class->config()->{trap_inf}, 0, qq|$class->config()->{trap_inf}|);

    # can set?
    $cfg = $class->config( trap_nan => 1 );
    is($cfg->{trap_nan}, 1, q|$cfg->{trap_nan}|);

    # can set via hash ref?
    $cfg = $class->config( { trap_nan => 1 } );
    is($cfg->{trap_nan}, 1, q|$cfg->{trap_nan}|);

    # also test that new() still works normally
    eval("\$x = $class->new('42'); \$x->bnan();");
    like($@, qr/^Tried to set/, "\$x = $class->new('42'); \$x->bnan();");
    # after new() never modified
    is($x, 42, "\$x = $class->new('42'); \$x->bnan();");

    # can reset?
    $cfg = $class->config( trap_nan => 0 );
    is($cfg->{trap_nan}, 0, q|$cfg->{trap_nan}|);

    # can set?
    $cfg = $class->config( trap_inf => 1 );
    is($cfg->{trap_inf}, 1, q|$cfg->{trap_inf}|);
    eval("\$x = $class->new('4711'); \$x->binf();");
    like($@, qr/^Tried to set/, "\$x = $class->new('4711'); \$x->binf();");
    # after new() never modified
    is($x, 4711, "\$x = $class->new('4711'); \$x->binf();");

    # +$x/0 => +inf
    eval("\$x =\$class->new('4711'); \$x->bdiv(0);");
    like($@, qr/^Tried to set/, "\$x =\$class->new('4711'); \$x->bdiv(0);");
    # after new() never modified
    is($x, 4711, "\$x =\$class->new('4711'); \$x->bdiv(0);");

    # -$x/0 => -inf
    eval("\$x = $class->new('-0815'); \$x->bdiv(0);");
    like($@, qr/^Tried to set/, "\$x = $class->new('-0815'); \$x->bdiv(0);");
    # after new() never modified
    is($x, -815, "\$x = $class->new('-0815'); \$x->bdiv(0);");

    $cfg = $class->config( trap_nan => 1 );
    # 0/0 => NaN
    eval("\$x = $class->new('0'); \$x->bdiv(0);");
    like($@, qr/^Tried to set/, "\$x = $class->new('0'); \$x->bdiv(0);");
    # after new() never modified
    is($x, 0, "\$x = $class->new('0'); \$x->bdiv(0);");
}

##############################################################################
# BigRat

Math::BigRat->config(trap_nan => 1,
                     trap_inf => 1);

for my $trap (qw/ 0.1a +inf inf -inf /) {
    my $x = Math::BigRat->new('7/4');

    note("");           # this is just for some space in the output

    # In each of the cases below, $x is not modified, because the code dies.

    eval("\$x = $mbi->new('$trap');");
    is($x, '7/4', "\$x = $mbi->new('$trap');");

    eval("\$x = $mbi->new('$trap');");
    is($x, '7/4', "\$x = $mbi->new('$trap');");

    eval("\$x = $mbi->new('$trap/7');");
    is($x, '7/4', "\$x = $mbi->new('$trap/7');");
}

# all tests done
