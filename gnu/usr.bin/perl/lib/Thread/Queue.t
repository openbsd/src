use warnings;

BEGIN {
    chdir 't' if -d 't';
    push @INC ,'../lib';
    require Config; import Config;
    unless ($Config{'useithreads'}) {
        print "1..0 # Skip: no ithreads\n";
        exit 0;
    }
}

use strict;
use threads;
use Thread::Queue;

my $q = new Thread::Queue;
$|++;
print "1..26\n";

my $test : shared = 1;

sub ok {
    lock($test);
    print "ok $test\n";
    $test++;
}

sub reader {
    my $tid = threads->self->tid;
    my $i = 0;
    while (1) {
	$i++;
#	print "reader (tid $tid): waiting for element $i...\n";
	my $el = $q->dequeue;
	ok();
# 	print "ok $test\n"; $test++;
#	print "reader (tid $tid): dequeued element $i: value $el\n";
	select(undef, undef, undef, rand(1));
	if ($el == -1) {
	    # end marker
#	    print "reader (tid $tid) returning\n";
	    return;
	}
    }
}

my $nthreads = 5;
my @threads;

for (my $i = 0; $i < $nthreads; $i++) {
    push @threads, threads->new(\&reader, $i);
}

for (my $i = 1; $i <= 20; $i++) {
    my $el = int(rand(100));
    select(undef, undef, undef, rand(1));
#    print "writer: enqueuing value $el\n";
    $q->enqueue($el);
}

$q->enqueue((-1) x $nthreads); # one end marker for each thread

for(@threads) {
#	print "waiting for join\n";
	$_->join();
}
ok();
#print "ok $test\n";


