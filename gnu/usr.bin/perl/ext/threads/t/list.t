
BEGIN {
    chdir 't' if -d 't';
    push @INC, '../lib';
    require Config; import Config;
    unless ($Config{'useithreads'}) {
        print "1..0 # Skip: no useithreads\n";
        exit 0;
    }
}

use ExtUtils::testlib;

use strict;


BEGIN { $| = 1; print "1..8\n" };
use threads;



print "ok 1\n";


#########################
sub ok {	
    my ($id, $ok, $name) = @_;

    # You have to do it this way or VMS will get confused.
    print $ok ? "ok $id - $name\n" : "not ok $id - $name\n";

    printf "# Failed test at line %d\n", (caller)[2] unless $ok;

    return $ok;
}

ok(2, scalar @{[threads->list]} == 0,'');



threads->create(sub {})->join();
ok(3, scalar @{[threads->list]} == 0,'');

my $thread = threads->create(sub {});
ok(4, scalar @{[threads->list]} == 1,'');
$thread->join();
ok(5, scalar @{[threads->list]} == 0,'');

$thread = threads->create(sub { ok(6, threads->self == (threads->list)[0],'')});
threads->yield; # help out non-preemptive thread implementations
sleep 1;
ok(7, $thread == (threads->list)[0],'');
$thread->join();
ok(8, scalar @{[threads->list]} == 0,'');
