use warnings;

BEGIN {
    chdir 't' if -d 't';
    push @INC ,'../lib';
    require Config; import Config;
    unless ($Config{'useithreads'}) {
        print "1..0 # Skip: no threads\n";
        exit 0;
    }
}
$|++;
print "1..31\n";
use strict;


use threads;

use threads::shared;

# We can't use the normal ok() type stuff here, as part of the test is
# to check that the numbers get printed in the right order. Instead, we
# set a 'base' number for each part of the test and specify the ok()
# number as an offset from that base.

my $Base = 0;

sub ok {
    my ($offset, $bool, $text) = @_;
    print "not " unless $bool;
    print "ok ", $Base + $offset, " - $text\n";
}

# test locking

{
    my $lock : shared;
    my $tr;

    # test that a subthread can't lock until parent thread has unlocked

    {
	lock($lock);
	ok(1,1,"set first lock");
	$tr = async {
	    lock($lock);
	    ok(3,1,"set lock in subthread");
	};
	threads->yield;
	ok(2,1,"still got lock");
    }
    $tr->join;

    $Base += 3;

    # ditto with ref to thread

    {
	my $lockref = \$lock;
	lock($lockref);
	ok(1,1,"set first lockref");
	$tr = async {
	    lock($lockref);
	    ok(3,1,"set lockref in subthread");
	};
	threads->yield;
	ok(2,1,"still got lockref");
    }
    $tr->join;

    $Base += 3;

    # make sure recursive locks unlock at the right place
    {
	lock($lock);
	ok(1,1,"set first recursive lock");
	lock($lock);
	threads->yield;
	{
	    lock($lock);
	    threads->yield;
	}
	$tr = async {
	    lock($lock);
	    ok(3,1,"set recursive lock in subthread");
	};
	{
	    lock($lock);
	    threads->yield;
	    {
		lock($lock);
		threads->yield;
		lock($lock);
		threads->yield;
	    }
	}
	ok(2,1,"still got recursive lock");
    }
    $tr->join;

    $Base += 3;

    # Make sure a lock factory gives out fresh locks each time 
    # for both attribute and run-time shares

    sub lock_factory1 { my $lock : shared; return \$lock; }
    sub lock_factory2 { my $lock; share($lock); return \$lock; }

    my (@locks1, @locks2);
    push @locks1, lock_factory1() for 1..2;
    push @locks1, lock_factory2() for 1..2;
    push @locks2, lock_factory1() for 1..2;
    push @locks2, lock_factory2() for 1..2;

    ok(1,1,"lock factory: locking all locks");
    lock $locks1[0];
    lock $locks1[1];
    lock $locks1[2];
    lock $locks1[3];
    ok(2,1,"lock factory: locked all locks");
    $tr = async {
	ok(3,1,"lock factory: child: locking all locks");
	lock $locks2[0];
	lock $locks2[1];
	lock $locks2[2];
	lock $locks2[3];
	ok(4,1,"lock factory: child: locked all locks");
    };
    $tr->join;
	
    $Base += 4;
}

# test cond_signal()

{
    my $lock : shared;

    sub foo {
	lock($lock);
	ok(1,1,"cond_signal: created first lock");
	my $tr2 = threads->create(\&bar);
	cond_wait($lock);
	$tr2->join();
	ok(5,1,"cond_signal: joined");
    }

    sub bar {
	ok(2,1,"cond_signal: child before lock");
	lock($lock);
	ok(3,1,"cond_signal: child locked");
	cond_signal($lock);
	ok(4,1,"cond_signal: signalled");
    }

    my $tr  = threads->create(\&foo);
    $tr->join();

    $Base += 5;

    # ditto, but with lockrefs

    my $lockref = \$lock;
    sub foo2 {
	lock($lockref);
	ok(1,1,"cond_signal: ref: created first lock");
	my $tr2 = threads->create(\&bar2);
	cond_wait($lockref);
	$tr2->join();
	ok(5,1,"cond_signal: ref: joined");
    }

    sub bar2 {
	ok(2,1,"cond_signal: ref: child before lock");
	lock($lockref);
	ok(3,1,"cond_signal: ref: child locked");
	cond_signal($lockref);
	ok(4,1,"cond_signal: ref: signalled");
    }

    $tr  = threads->create(\&foo2);
    $tr->join();

    $Base += 5;

}


# test cond_broadcast()

{
    my $counter : shared = 0;

    # broad(N) forks off broad(N-1) and goes into a wait, in such a way
    # that it's guaranteed to reach the wait before its child enters the
    # locked region. When N reaches 0, the child instead does a
    # cond_broadcast to wake all its ancestors.

    sub broad {
	my $n = shift;
	my $th;
	{
	    lock($counter);
	    if ($n > 0) {
		$counter++;
		$th = threads->new(\&broad, $n-1);
		cond_wait($counter);
		$counter += 10;
	    }
	    else {
		ok(1, $counter == 3, "cond_broadcast: all three waiting");
		cond_broadcast($counter);
	    }
	}
	$th->join if $th;
    }

    threads->new(\&broad, 3)->join;
    ok(2, $counter == 33, "cond_broadcast: all three threads woken");
    print "# counter=$counter\n";

    $Base += 2;


    # ditto, but with refs and shared()

    my $counter2 = 0;
    share($counter2);
    my $r = \$counter2;

    sub broad2 {
	my $n = shift;
	my $th;
	{
	    lock($r);
	    if ($n > 0) {
		$$r++;
		$th = threads->new(\&broad2, $n-1);
		cond_wait($r);
		$$r += 10;
	    }
	    else {
		ok(1, $$r == 3, "cond_broadcast: ref: all three waiting");
		cond_broadcast($r);
	    }
	}
	$th->join if $th;
    }

    threads->new(\&broad2, 3)->join;;
    ok(2, $$r == 33, "cond_broadcast: ref: all three threads woken");
    print "# counter=$$r\n";

    $Base += 2;

}

# test warnings;

{
    my $warncount = 0;
    local $SIG{__WARN__} = sub { $warncount++ };

    my $lock : shared;

    cond_signal($lock);
    ok(1, $warncount == 1, 'get warning on cond_signal');
    cond_broadcast($lock);
    ok(2, $warncount == 2, 'get warning on cond_broadcast');
    no warnings 'threads';
    cond_signal($lock);
    ok(3, $warncount == 2, 'get no warning on cond_signal');
    cond_broadcast($lock);
    ok(4, $warncount == 2, 'get no warning on cond_broadcast');

    $Base += 4;
}



