
BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require Config; import Config;
    unless ($Config{'useithreads'}) {
        print "1..0 # Skip: no useithreads\n";
        exit 0;
    }
}

use ExtUtils::testlib;
use strict;
BEGIN { $| = 1; print "1..11\n"};

use threads;
use threads::shared;
my $i = 10;
my $y = 20000;
my %localtime;
for(0..$i) {
	$localtime{$_} = localtime($_);
};
my $mutex = 1;
share($mutex);
sub localtime_r {
#  print "Waiting for lock\n";
  lock($mutex);
#  print "foo\n";
  my $retval = localtime(shift());
#  unlock($mutex);
  return $retval;
}
my @threads;
for(0..$i) {
  my $thread = threads->create(sub {
				 my $arg = $_;
		    my $localtime = $localtime{$arg};
		    my $error = 0;
		    for(0..$y) {
		      my $lt = localtime($arg);
		      if($localtime ne $lt) {
			$error++;
		      }	
		    }
				 lock($mutex);
				 if($error) {
				   print "not ok $mutex # not a safe localtime\n";
				 } else {
				   print "ok $mutex\n";
				 }
				 $mutex++;
		  });	
  push @threads, $thread;
}

for(@threads) {
  $_->join();
}

