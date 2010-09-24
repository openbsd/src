#!./perl -w
#
# test a few problems with the Freezer option, not a complete Freezer
# test suite yet

BEGIN {
    require Config; import Config;
    no warnings 'once';
    if ($Config{'extensions'} !~ /\bData\/Dumper\b/) {
	print "1..0 # Skip: Data::Dumper was not built\n";
	exit 0;
    }
}

use strict;
use Test::More qw(no_plan);
use Data::Dumper;
$Data::Dumper::Freezer = 'freeze';

# test for seg-fault bug when freeze() returns a non-ref
my $foo = Test1->new("foo");
my $dumped_foo = Dumper($foo);
ok($dumped_foo, 
   "Use of freezer sub which returns non-ref worked.");
like($dumped_foo, qr/frozed/, 
     "Dumped string has the key added by Freezer.");

# run the same tests with useperl.  this always worked
{
    local $Data::Dumper::Useperl = 1;
    my $foo = Test1->new("foo");
    my $dumped_foo = Dumper($foo);
    ok($dumped_foo, 
       "Use of freezer sub which returns non-ref worked with useperl");
    like($dumped_foo, qr/frozed/, 
         "Dumped string has the key added by Freezer with useperl.");
}

# test for warning when an object doesn't have a freeze()
{
    my $warned = 0;
    local $SIG{__WARN__} = sub { $warned++ };
    my $bar = Test2->new("bar");
    my $dumped_bar = Dumper($bar);
    is($warned, 0, "A missing freeze() shouldn't warn.");
}


# run the same test with useperl, which always worked
{
    local $Data::Dumper::Useperl = 1;
    my $warned = 0;
    local $SIG{__WARN__} = sub { $warned++ };
    my $bar = Test2->new("bar");
    my $dumped_bar = Dumper($bar);
    is($warned, 0, "A missing freeze() shouldn't warn with useperl");
}

# a freeze() which die()s should still trigger the warning
{
    my $warned = 0;
    local $SIG{__WARN__} = sub { $warned++; };
    my $bar = Test3->new("bar");
    my $dumped_bar = Dumper($bar);
    is($warned, 1, "A freeze() which die()s should warn.");
}

# the same should work in useperl
{
    local $Data::Dumper::Useperl = 1;
    my $warned = 0;
    local $SIG{__WARN__} = sub { $warned++; };
    my $bar = Test3->new("bar");
    my $dumped_bar = Dumper($bar);
    is($warned, 1, "A freeze() which die()s should warn with useperl.");
}

# a package with a freeze() which returns a non-ref
package Test1;
sub new { bless({name => $_[1]}, $_[0]) }
sub freeze {
    my $self = shift;
    $self->{frozed} = 1;
}

# a package without a freeze()
package Test2;
sub new { bless({name => $_[1]}, $_[0]) }

# a package with a freeze() which dies
package Test3;
sub new { bless({name => $_[1]}, $_[0]) }
sub freeze { die "freeze() is broked" }
