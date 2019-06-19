#!perl

use strict;
use warnings;

my $x;

###############################################################################
# inf tests

$x = 1 + inf;
like(ref($x), qr/^Math::BigInt/, '$x = 1 + inf makes a Math::BigInt');
is($x->bstr(), 'inf', qq|$x = 1 + inf; $x->bstr() = 'inf'|);

$x = 1 * inf;
like(ref($x), qr/^Math::BigInt/, '$x = 1 * inf makes a Math::BigInt');
is($x->bstr(), 'inf', qq|$x = 1 * inf; $x->bstr() = 'inf'|);

# these don't work without exporting inf()
$x = inf;
like(ref($x), qr/^Math::BigInt/, '$x = inf makes a Math::BigInt');
is($x->bstr(), 'inf', qq|$x = inf; $x->bstr() = 'inf'|);

$x = inf + inf;
like(ref($x), qr/^Math::BigInt/, '$x = inf + inf makes a Math::BigInt');
is($x->bstr(), 'inf', qq|$x = inf + inf; $x->bstr() = 'inf'|);

$x = inf * inf;
like(ref($x), qr/^Math::BigInt/, '$x = inf * inf makes a Math::BigInt');
is($x->bstr(), 'inf', qq|$x = inf * inf; $x->bstr() = 'inf'|);

###############################################################################
# NaN tests

$x = 1 + NaN;
like(ref($x), qr/^Math::BigInt/, '$x = 1 + NaN makes a Math::BigInt');
is($x->bstr(), 'NaN', qq|$x = 1 + NaN; $x->bstr() = 'NaN'|);

$x = 1 * NaN;
like(ref($x), qr/^Math::BigInt/, '$x = 1 * NaN makes a Math::BigInt');
is($x->bstr(), 'NaN', qq|$x = 1 * NaN; $x->bstr() = 'NaN'|);

# these don't work without exporting NaN()
$x = NaN;
like(ref($x), qr/^Math::BigInt/, '$x = NaN makes a Math::BigInt');
is($x->bstr(), 'NaN', qq|$x = NaN; $x->bstr() = 'NaN'|);

$x = NaN + NaN;
like(ref($x), qr/^Math::BigInt/, '$x = NaN + NaN makes a Math::BigInt');
is($x->bstr(), 'NaN', qq|$x = NaN + NaN; $x->bstr() = 'NaN'|);

$x = NaN * NaN;
like(ref($x), qr/^Math::BigInt/, '$x = NaN * NaN makes a Math::BigInt');
is($x->bstr(), 'NaN', qq|$x = NaN * NaN; $x->bstr() = 'NaN'|);

###############################################################################
# mixed tests

# these don't work without exporting NaN() or inf()

$x = NaN + inf;
like(ref($x), qr/^Math::BigInt/, '$x = NaN + inf makes a Math::BigInt');
is($x->bstr(), 'NaN', qq|$x = NaN + inf; $x->bstr() = 'NaN'|);

$x = NaN * inf;
like(ref($x), qr/^Math::BigInt/, '$x = NaN * inf makes a Math::BigInt');
is($x->bstr(), 'NaN', qq|$x = NaN * inf; $x->bstr() = 'NaN'|);

$x = inf * NaN;
like(ref($x), qr/^Math::BigInt/, '$x = inf * NaN makes a Math::BigInt');
is($x->bstr(), 'NaN', qq|$x = inf * NaN; $x->bstr() = 'NaN'|);

###############################################################################
# inf and NaN as strings.

for my $nan (qw/ nan naN nAn nAN Nan NaN NAn NAN /) {
    my $x = 1 + $nan;
    is($x->bstr(), "NaN", qq|\$x = 1 + "$nan"; \$x->bstr() = "NaN"|);
    isa_ok($x, "Math::BigInt");
}

for my $inf (qw/ inf inF iNf iNF Inf InF INf INF
                 infinity Infinity InFiNiTy iNfInItY
               /)
{
    my $x = 1 + $inf;
    is($x->bstr(), "inf", qq|\$x = 1 + "$inf"; \$x->bstr() = "inf"|);
    isa_ok($x, "Math::BigInt");
}

1;
