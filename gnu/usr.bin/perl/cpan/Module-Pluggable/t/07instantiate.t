#!perl -w

use strict;
use FindBin;
use lib (($FindBin::Bin."/lib")=~/^(.*)$/);
use Test::More tests => 6;

my $foo;
ok($foo = MyTest->new());



my @plugins;
ok(@plugins = sort $foo->booga(nork => 'fark'));
is(ref $plugins[0],'MyTest::Extend::Plugin::Bar');
is($plugins[0]->nork,'fark');


@plugins = ();
eval { @plugins = $foo->wooga( nork => 'fark') };
is($@, '');
is(scalar(@plugins),0);


package MyTest;
use File::Spec::Functions qw(catdir);
use strict;
use FindBin;
use lib (($FindBin::Bin."/lib")=~/^(.*)$/);
use Module::Pluggable (search_path => ["MyTest::Extend::Plugin"], sub_name => 'booga', instantiate => 'new');
use Module::Pluggable (search_path => ["MyTest::Extend::Plugin"], sub_name => 'wooga', instantiate => 'nosomuchmethod');


sub new {
    my $class = shift;
    return bless {}, $class;

}
1;

