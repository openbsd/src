#!perl
use strict;
use warnings;

use Config;
if (!$Config{useithreads}) {
    print "1..0 # Skip: no ithreads\n";
    exit 0;
}
if ($ENV{PERL_CORE_MINITEST}) {
    print "1..0 # Skip: no dynamic loading on miniperl, no threads\n";
    exit 0;
}

require threads;

sub thread_it {
    # Generate things like './op/regexp.t', './t/op/regexp.t', ':op:regexp.t'
    my @paths
	= (join ('/', '.', @_), join ('/', '.', 't', @_), join (':', @_));
		 
    for my $file (@paths) {
	if (-r $file) {
	    print "# found tests in $file\n";
	    $::running_as_thread = "running tests in a new thread";
	    do $file or die $@;
	    print "# running tests in a new thread\n";
	    my $curr = threads->create(sub {
		run_tests();
		return defined &curr_test ? curr_test() : ()
	    })->join();
	    curr_test($curr) if defined $curr;
	    exit;
	}
    }
    die "Cannot find " . join (" or ", @paths) . "\n";
}

1;
