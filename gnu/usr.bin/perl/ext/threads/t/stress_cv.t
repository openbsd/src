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
BEGIN { print "1..64\n" };
use threads;


print "ok 1\n";




sub ok {	
    my ($id, $ok, $name) = @_;
    
    # You have to do it this way or VMS will get confused.
    print $ok ? "ok $id - $name\n" : "not ok $id - $name\n";

    printf "# Failed test at line %d\n", (caller)[2] unless $ok;
    
    return $ok;
}


ok(2,1,"");


my @threads;
for(3..33) {
  ok($_,1,"Multiple thread test");
  push @threads ,threads->create(sub { my $i = shift; for(1..500000) { $i++}},$_);
}

my $i = 34;
for(@threads) {
  $_->join;
  ok($i++,1,"Thread joined");
}

