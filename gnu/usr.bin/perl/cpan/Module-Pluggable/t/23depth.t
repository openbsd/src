#!perl -w

use strict;
use FindBin;
use lib (($FindBin::Bin."/lib")=~/^(.*)$/);
use Test::More tests => 2;


my $min = MinTest->new();
my $max = MaxTest->new();
is_deeply([sort qw(MyOtherTest::Plugin::Bar MyOtherTest::Plugin::Foo  MyOtherTest::Plugin::Quux)], [sort $max->plugins], "min depth");
is_deeply([qw(MyOtherTest::Plugin::Quux::Foo)], [sort $min->plugins], "max depth");


package MinTest;
use File::Spec::Functions qw(catdir);
use strict;
use File::Spec::Functions qw(catdir);
use Module::Pluggable search_path => "MyOtherTest::Plugin", min_depth => 4;


sub new {
    my $class = shift;
    return bless {}, $class;
}

package MaxTest;
use File::Spec::Functions qw(catdir);
use strict;
use File::Spec::Functions qw(catdir);
use Module::Pluggable search_path => "MyOtherTest::Plugin", max_depth => 3;


sub new {
    my $class = shift;
    return bless {}, $class;
}
1;