
BEGIN {
    chdir 't' if -d 't';
    push @INC, '../lib','.';
    require Config; import Config;
    unless ($Config{'useithreads'}) {
        print "1..0 # Skip: no useithreads\n";
        exit 0;
    }
    require "test.pl";
}

use ExtUtils::testlib;
use strict;
BEGIN { $| = 1; print "1..31\n" };
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
{
    # there is a little chance this test case will falsly fail
    # since it tests rand	
    my %rand : shared;
    rand(10);
    threads->new( sub { $rand{int(rand(10000000000))}++ } ) foreach 1..25;
    $_->join foreach threads->list;
#    use Data::Dumper qw(Dumper);
#    print Dumper(\%rand);
    #$val = rand();
    ok((keys %rand == 25), "Check that rand works after a new thread");
}

# bugid #24165

run_perl(prog =>
    'use threads; sub a{threads->new(shift)} $t = a sub{}; $t->tid; $t->join; $t->tid');
is($?, 0, 'coredump in global destruction');

# test CLONE_SKIP() functionality

{
    my %c : shared;
    my %d : shared;

    # ---

    package A;
    sub CLONE_SKIP { $c{"A-$_[0]"}++; 1; }
    sub DESTROY    { $d{"A-". ref $_[0]}++ }

    package A1;
    our @ISA = qw(A);
    sub CLONE_SKIP { $c{"A1-$_[0]"}++; 1; }
    sub DESTROY    { $d{"A1-". ref $_[0]}++ }

    package A2;
    our @ISA = qw(A1);

    # ---

    package B;
    sub CLONE_SKIP { $c{"B-$_[0]"}++; 0; }
    sub DESTROY    { $d{"B-" . ref $_[0]}++ }

    package B1;
    our @ISA = qw(B);
    sub CLONE_SKIP { $c{"B1-$_[0]"}++; 1; }
    sub DESTROY    { $d{"B1-" . ref $_[0]}++ }

    package B2;
    our @ISA = qw(B1);

    # ---

    package C;
    sub CLONE_SKIP { $c{"C-$_[0]"}++; 1; }
    sub DESTROY    { $d{"C-" . ref $_[0]}++ }

    package C1;
    our @ISA = qw(C);
    sub CLONE_SKIP { $c{"C1-$_[0]"}++; 0; }
    sub DESTROY    { $d{"C1-" . ref $_[0]}++ }

    package C2;
    our @ISA = qw(C1);

    # ---

    package D;
    sub DESTROY    { $d{"D-" . ref $_[0]}++ }

    package D1;
    our @ISA = qw(D);

    package main;

    {
	my @objs;
	for my $class (qw(A A1 A2 B B1 B2 C C1 C2 D D1)) {
	    push @objs, bless [], $class;
	}

	sub f {
	    my $depth = shift;
	    my $cloned = ""; # XXX due to recursion, doesn't get initialized
	    $cloned .= "$_" =~ /ARRAY/ ? '1' : '0' for @objs;
	    is($cloned, ($depth ? '00010001111' : '11111111111'),
		"objs clone skip at depth $depth");
	    threads->new( \&f, $depth+1)->join if $depth < 2;
	    @objs = ();
	}
	f(0);
    }

    curr_test(curr_test()+2);
    ok(eq_hash(\%c,
	{
	    qw(
		A-A	2
		A1-A1	2
		A1-A2	2
		B-B	2
		B1-B1	2
		B1-B2	2
		C-C	2
		C1-C1	2
		C1-C2	2
	    )
	}),
	"counts of calls to CLONE_SKIP");
    ok(eq_hash(\%d,
	{
	    qw(
		A-A	1
		A1-A1	1
		A1-A2	1
		B-B	3
		B1-B1	1
		B1-B2	1
		C-C	1
		C1-C1	3
		C1-C2	3
		D-D	3
		D-D1	3
	    )
	}),
	"counts of calls to DESTROY");
}

