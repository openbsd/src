use warnings;
#no warnings 'once';
use Test::More tests => 1;

use Carp;

# test that enabling overload without loading overload.pm does not trigger infinite recursion

my $p = "OverloadedInXS"; 
*{$p."::(("} = sub{};
*{$p.q!::(""!} = sub { Carp::cluck "<My Stringify>" }; 
sub { Carp::longmess("longmess:") }->(bless {}, $p);
ok(1);


