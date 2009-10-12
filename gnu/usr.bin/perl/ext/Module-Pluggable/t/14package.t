#!perl -w

use strict;
use FindBin;
use lib (($FindBin::Bin."/lib")=~/^(.*)$/);
use Test::More tests => 5;

my $foo;
ok($foo = MyTest->new());

my @plugins;
my @expected = qw(MyTest::Plugin::Bar MyTest::Plugin::Foo MyTest::Plugin::Quux::Foo);
ok(@plugins = sort $foo->plugins);
is_deeply(\@plugins, \@expected);

@plugins = ();

ok(@plugins = sort MyTest->plugins);
is_deeply(\@plugins, \@expected);



package MyTest;
use strict;
sub new { return bless {}, $_[0] }

package MyOtherTest;
use strict;
use Module::Pluggable ( package => "MyTest" );
sub new { return bless {}, $_[0] }


1;

