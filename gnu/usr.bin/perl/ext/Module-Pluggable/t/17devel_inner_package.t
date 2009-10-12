#!perl -w
use Test::More tests => 3;

use Devel::InnerPackage qw(list_packages);
use FindBin;
use lib (($FindBin::Bin."/lib")=~/^(.*)$/);

my @packages;

use_ok("TA::C::A::I");
ok(@packages = list_packages("TA::C::A::I"));

is_deeply([sort @packages], [qw(TA::C::A::I::A TA::C::A::I::A::B)]);


