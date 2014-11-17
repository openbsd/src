#!perl -w
use strict;
use Test::More;

plan skip_all => 'This is perl core-only test' unless $ENV{PERL_CORE};
plan skip_all => 'Special case v5.21.1 because rjbs' if sprintf("v%vd", $^V) eq 'v5.21.1';

my @modules = qw[
  Module::CoreList
  Module::CoreList::Utils
  Module::CoreList::TieHashDelta
];

plan tests => scalar @modules;

foreach my $mod ( @modules ) {
  eval "require $mod";
  my $vers = eval $mod->VERSION;
  ok( !( $vers < $] || $vers > $] ), "$mod version should match perl version in core" )
    or diag("$mod $vers doesn't match $]");
}
