#!perl -w

use strict;
use FindBin;
use lib (($FindBin::Bin."/lib")=~/^(.*)$/);
use Test::More tests => 2;

my $t = MyTest->new();


ok($t->plugins());

ok(keys %{MyTest::Plugin::Foo::});


package MyTest;
use File::Spec::Functions qw(catdir);
use strict;
use Module::Pluggable (require => 1);
use base qw(Module::Pluggable);


sub new {
    my $class = shift;
    return bless {}, $class;

}
1;

