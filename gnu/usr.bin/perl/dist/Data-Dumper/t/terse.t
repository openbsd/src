#!perl
use strict;
use warnings;

use Test::More tests => 2;

use Data::Dumper;

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
