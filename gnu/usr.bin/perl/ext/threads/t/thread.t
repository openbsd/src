
BEGIN {
    chdir 't' if -d 't';
    @INC = qw(../lib .);
    require Config; import Config;
    unless ($Config{'useithreads'}) {
        print "1..0 # Skip: no useithreads\n";
        exit 0;
    }
    require "test.pl";
}

use ExtUtils::testlib;
use strict;
BEGIN { $| = 1; print "1..24\n" };
use threads;
use threads::shared;

print "ok 1\n";

sub content {
    print shift;
    return shift;
}
{
    my $t = threads->new(\&content, "ok 2\n", "ok 3\n", 1..1000);
    print $t->join();
}
{
    my $lock : shared;
    my $t;
    {
	lock($lock);
	$t = threads->new(sub { lock($lock); print "ok 5\n"});
	print "ok 4\n";
    }
    $t->join();
}

sub dorecurse {
    my $val = shift;
    my $ret;
    print $val;
    if(@_) {
	$ret = threads->new(\&dorecurse, @_);
	$ret->join;
    }
}
{
    my $t = threads->new(\&dorecurse, map { "ok $_\n" } 6..10);
    $t->join();
}

{
    # test that sleep lets other thread run
    my $t = threads->new(\&dorecurse, "ok 11\n");
    threads->yield; # help out non-preemptive thread implementations
    sleep 1;
    print "ok 12\n";
    $t->join();
}
{
    my $lock : shared;
    sub islocked {
	lock($lock);
	my $val = shift;
	my $ret;
	print $val;
	if (@_) {
	    $ret = threads->new(\&islocked, shift);
	}
	return $ret;
    }
my $t = threads->new(\&islocked, "ok 13\n", "ok 14\n");
$t->join->join;
}



sub testsprintf {
    my $testno = shift;
    my $same = sprintf( "%0.f", $testno);
    return $testno eq $same;
}

sub threaded {
    my ($string, $string_end) = @_;

  # Do the match, saving the output in appropriate variables
    $string =~ /(.*)(is)(.*)/;
  # Yield control, allowing the other thread to fill in the match variables
    threads->yield();
  # Examine the match variable contents; on broken perls this fails
    return $3 eq $string_end;
}


{ 
    curr_test(15);

    my $thr1 = threads->new(\&testsprintf, 15);
    my $thr2 = threads->new(\&testsprintf, 16);
    
    my $short = "This is a long string that goes on and on.";
    my $shorte = " a long string that goes on and on.";
    my $long  = "This is short.";
    my $longe  = " short.";
    my $foo = "This is bar bar bar.";
    my $fooe = " bar bar bar.";
    my $thr3 = new threads \&threaded, $short, $shorte;
    my $thr4 = new threads \&threaded, $long, $longe;
    my $thr5 = new threads \&testsprintf, 19;
    my $thr6 = new threads \&testsprintf, 20;
    my $thr7 = new threads \&threaded, $foo, $fooe;

    ok($thr1->join());
    ok($thr2->join());
    ok($thr3->join());
    ok($thr4->join());
    ok($thr5->join());
    ok($thr6->join());
    ok($thr7->join());
}

# test that 'yield' is importable

package Test1;

use threads 'yield';
yield;
main::ok(1);

package main;


# test async

{
    my $th = async {return 1 };
    ok($th);
    ok($th->join());
}

