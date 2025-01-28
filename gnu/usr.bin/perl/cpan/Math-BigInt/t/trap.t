# -*- mode: perl; -*-

# test that config ( trap_nan => 1, trap_inf => 1) really works/dies

use strict;
use warnings;

use Test::More tests => 90;

my $mbi = 'Math::BigInt';
my $mbf = 'Math::BigFloat';
my $mbr = 'Math::BigRat';

use_ok($mbi);
use_ok($mbf);
use_ok($mbr);

my $x;

foreach my $class ($mbi, $mbf, $mbr) {

    # can do?
    can_ok($class, 'config');

    ###########################################################################
    # Default values.
    ###########################################################################

    # defaults are okay?
    is($class->config("trap_nan"), 0, 'trap_nan defaults to 0');
    is($class->config("trap_inf"), 0, 'trap_inf defaults to 0');

    ###########################################################################
    # Trap NaN.
    ###########################################################################

    # can set?
    $class->config( trap_nan => 1 );
    is($class->config("trap_nan"), 1, qq|$class->config( trap_nan => 1 );|);

    # can reset?
    $class->config( trap_nan => 0 );
    is($class->config("trap_nan"), 0, qq|$class->config( trap_nan => 0 );|);

    # can set via hash ref?
    $class->config( { trap_nan => 1 } );
    is($class->config("trap_nan"), 1, qq|$class->config( { trap_nan => 1 } );|);

    # 0/0 => NaN
    $x = $class->new("0");
    eval { $x->bdiv(0); };
    like($@, qr/^Tried to /, qq|\$x = $class->new("0"); \$x->bdiv(0);|);

    # new() didn't modify $x
    is($x, 0, qq|\$x = $class->new("0"); \$x->bdiv(0);|);

    # also test that new() still works normally
    eval { $x = $class->new('42'); $x->bnan(); };
    like($@, qr/^Tried to /, 'died');
    is($x, 42, '$x after new() never modified');

    # can reset?
    $class->config( trap_nan => 0 );
    is($class->config("trap_nan"), 0, qq|$class->config( trap_nan => 0 );|);

    ###########################################################################
    # Trap inf.
    ###########################################################################

    # can set?
    $class->config( trap_inf => 1 );
    is($class->config("trap_inf"), 1, 'trap_inf enabled');

    eval { $x = $class->new('4711'); $x->binf(); };
    like($@, qr/^Tried to /, 'died');
    is($x, 4711, '$x after new() never modified');

    eval { $x = $class->new('inf'); };
    like($@, qr/^Tried to /, 'died');
    is($x, 4711, '$x after new() never modified');

    eval { $x = $class->new('-inf'); };
    like($@, qr/^Tried to /, 'died');
    is($x, 4711, '$x after new() never modified');

    # +$x/0 => +inf
    eval { $x = $class->new('4711'); $x->bdiv(0); };
    like($@, qr/^Tried to /, 'died');
    is($x, 4711, '$x after new() never modified');

    # -$x/0 => -inf
    eval { $x = $class->new('-0815'); $x->bdiv(0); };
    like($@, qr/^Tried to /, 'died');
    is($x, '-815', '$x after new not modified');

    $class->config( trap_nan => 1 );
    # 0/0 => NaN
    eval { $x = $class->new('0'); $x->bdiv(0); };
    like($@, qr/^Tried to /, 'died');
    is($x, '0', '$x after new not modified');
}

##############################################################################
# Math::BigInt

$x = Math::BigInt->new(2);
eval { $x = $mbi->new('0.1'); };
is($x, 2, 'never modified since it dies');

eval { $x = $mbi->new('0a.1'); };
is($x, 2, 'never modified since it dies');

##############################################################################
# Math::BigFloat

$x = Math::BigFloat->new(2);
eval { $x = $mbf->new('0.1a'); };
is($x, 2, 'never modified since it dies');

##############################################################################
# BigRat

Math::BigRat->config(trap_nan => 1,
                     trap_inf => 1);

for my $trap (qw/ 0.1a +inf inf -inf /) {
    my $x = Math::BigRat->new('7/4');

    note("");           # this is just for some space in the output

    # In each of the cases below, $x is not modified, because the code dies.

    eval { $x = $mbr->new("$trap"); };
    is($x, "7/4", qq|\$x = $mbr->new("$trap");|);

    eval { $x = $mbr->new("$trap"); };
    is($x, "7/4", qq|\$x = $mbr->new("$trap");|);

    eval { $x = $mbr->new("$trap/7"); };
    is($x, "7/4", qq|\$x = $mbr->new("$trap/7");|);
}

# all tests done
