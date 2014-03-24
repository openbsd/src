#!perl
use strict;
use warnings;

use Data::Dumper;
use Test::More tests => 6;
use lib qw( ./t/lib );
use Testing qw( _dumptostr );


my $hash = { foo => 42 };

for my $useperl (0..1) {
    my $dumper = Data::Dumper->new([$hash]);
    $dumper->Terse(1);
    $dumper->Indent(2);
    $dumper->Useperl($useperl);

    is $dumper->Dump, <<'WANT', "Terse(1), Indent(2), Useperl($useperl)";
{
  'foo' => 42
}
WANT
}

my (%dumpstr);
my $dumper;

$dumper = Data::Dumper->new([$hash]);
$dumpstr{noterse} = _dumptostr($dumper);
# $VAR1 = {
#           'foo' => 42
#         };

$dumper = Data::Dumper->new([$hash]);
$dumper->Terse();
$dumpstr{terse_no_arg} = _dumptostr($dumper);

$dumper = Data::Dumper->new([$hash]);
$dumper->Terse(0);
$dumpstr{terse_0} = _dumptostr($dumper);

$dumper = Data::Dumper->new([$hash]);
$dumper->Terse(1);
$dumpstr{terse_1} = _dumptostr($dumper);
# {
#   'foo' => 42
# }

$dumper = Data::Dumper->new([$hash]);
$dumper->Terse(undef);
$dumpstr{terse_undef} = _dumptostr($dumper);

is($dumpstr{noterse}, $dumpstr{terse_no_arg},
    "absence of Terse is same as Terse()");
is($dumpstr{noterse}, $dumpstr{terse_0},
    "absence of Terse is same as Terse(0)");
isnt($dumpstr{noterse}, $dumpstr{terse_1},
    "absence of Terse is different from Terse(1)");
is($dumpstr{noterse}, $dumpstr{terse_undef},
    "absence of Terse is same as Terse(undef)");
