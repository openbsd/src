
BEGIN {
    chdir 't' if -d 't';
    push @INC, '../lib';
    require Config; import Config;
    unless ($Config{'useithreads'}) {
	print "1..0 # Skip: no useithreads\n";
 	exit 0;	
    }
}

use warnings;
use strict;
use threads;
use threads::shared;

# Note that we can't use  Test::More here, as we would need to
# call is() from within the DESTROY() function at global destruction time,
# and parts of Test::* may have already been freed by then

print "1..4\n";

my $test : shared = 1;

sub is($$$) {
    my ($got, $want, $desc) = @_;
    unless ($got eq $want) {
	print "# EXPECTED: $want\n";
	print "# GOT:      got\n";
	print "not ";
    }
    print "ok $test - $desc\n";
    $test++;
}


#
# This tests for too much destruction
# which was caused by cloning stashes
# on join which led to double the dataspace
#
#########################

$|++;

{ 
    sub Foo::DESTROY { 
	my $self = shift;
	my ($package, $file, $line) = caller;
	is(threads->tid(),$self->{tid},
		"In destroy[$self->{tid}] it should be correct too" )
    }
    my $foo;
    $foo = bless {tid => 0}, 'Foo';			  
    my $bar = threads->create(sub { 
	is(threads->tid(),1, "And tid be 1 here");
	$foo->{tid} = 1;
	return $foo;
    })->join();
    $bar->{tid} = 0;
}
1;
