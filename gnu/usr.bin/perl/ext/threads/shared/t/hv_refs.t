use warnings;

BEGIN {
#    chdir 't' if -d 't';
#    push @INC ,'../lib';
    require Config; import Config;
    unless ($Config{'useithreads'}) {
        print "1..0 # Skip: no useithreads\n";
        exit 0;
    }
}


sub ok {
    my ($id, $ok, $name) = @_;

    $name = '' unless defined $name;
    # You have to do it this way or VMS will get confused.
    print $ok ? "ok $id - $name\n" : "not ok $id - $name\n";

    printf "# Failed test at line %d\n", (caller)[2] unless $ok;

    return $ok;
}

sub skip {
    my ($id, $ok, $name) = @_;
    print "ok $id # skip _thrcnt - $name \n";
}

use ExtUtils::testlib;
use strict;
BEGIN { print "1..17\n" };
use threads;
use threads::shared;
ok(1,1,"loaded");
my $foo;
share($foo);
my %foo;
share(%foo);
$foo{"foo"} = \$foo;
ok(2, !defined ${$foo{foo}}, "Check deref");
$foo = "test";
ok(3, ${$foo{foo}} eq "test", "Check deref after assign");
threads->create(sub{${$foo{foo}} = "test2";})->join();
ok(4, $foo eq "test2", "Check after assign in another thread");
my $bar = delete($foo{foo});
ok(5, $$bar eq "test2", "check delete");
threads->create( sub {
   my $test;
   share($test);
   $test = "thread3";
   $foo{test} = \$test;
   })->join();
ok(6, ${$foo{test}} eq "thread3", "Check reference created in another thread");
my $gg = $foo{test};
$$gg = "test";
ok(7, ${$foo{test}} eq "test", "Check reference");
my $gg2 = delete($foo{test});
ok(8, threads::shared::_id($$gg) == threads::shared::_id($$gg2),
       sprintf("Check we get the same thing (%x vs %x)",
       threads::shared::_id($$gg),threads::shared::_id($$gg2)));
ok(9, $$gg eq $$gg2, "And check the values are the same");
ok(10, keys %foo == 0, "And make sure we realy have deleted the values");
{
    my (%hash1, %hash2);
    share(%hash1);
    share(%hash2);
    $hash1{hash} = \%hash2;
    $hash2{"bar"} = "foo";
    ok(11, $hash1{hash}->{bar} eq "foo", "Check hash references work");
    threads->create(sub { $hash2{"bar2"} = "foo2"})->join();
    ok(12, $hash1{hash}->{bar2} eq "foo2", "Check hash references work");
    threads->create(sub { my (%hash3); share(%hash3); $hash2{hash} = \%hash3; $hash3{"thread"} = "yes"})->join();
    ok(13, $hash1{hash}->{hash}->{thread} eq "yes", "Check hash created in another thread");
}

{
  my $h = {a=>14};
  my $r = \$h->{a};
  share($r);
  lock($r);
  lock($h->{a});
  ok(14, 1, "lock on helems now work, this was bug 10045");

}
{
    my $object : shared = &share({});
    threads->new(sub {
		     bless $object, 'test1';
		 })->join;
    ok(15, ref($object) eq 'test1', "blessing does work");
    my %test = (object => $object);
    ok(16, ref($test{object}) eq 'test1', "and some more work");
    bless $object, 'test2';
    ok(17, ref($test{object}) eq 'test2', "reblessing works!");
}

