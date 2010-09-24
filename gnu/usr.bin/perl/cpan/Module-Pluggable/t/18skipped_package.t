#!perl -w

use Test::More tests => 1;
use FindBin;
use lib (($FindBin::Bin."/lib")=~/^(.*)$/);

use Devel::InnerPackage qw(list_packages);
use No::Middle;

my @p = list_packages("No::Middle");
is_deeply([ sort @p ], [ qw(No::Middle::Package::A No::Middle::Package::B) ]);
