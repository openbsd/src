
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

print "1..10\n";

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

#
# This tests whether we can call Config::myconfig after threads have been
# started (interpreter cloned).  5.8.1 and 5.8.2 contained a bug that would
# disallow that too be done, because an attempt was made to change a variable
# with the : unique attribute.
#
#########################

threads->new( sub {1} )->join;
my $not = eval { Config::myconfig() } ? '' : 'not ';
print "${not}ok $test - Are we able to call Config::myconfig after clone\n";
$test++;

# bugid 24383 - :unique hashes weren't being made readonly on interpreter
# clone; check that they are.

our $unique_scalar : unique;
our @unique_array : unique;
our %unique_hash : unique;
threads->new(
    sub {
	eval { $unique_scalar = 1 };
	print $@ =~ /read-only/  ? '' : 'not ', "ok $test - unique_scalar\n";
	$test++;
	eval { $unique_array[0] = 1 };
	print $@ =~ /read-only/  ? '' : 'not ', "ok $test - unique_array\n";
	$test++;
	eval { $unique_hash{abc} = 1 };
	print $@ =~ /disallowed/  ? '' : 'not ', "ok $test - unique_hash\n";
	$test++;
    }
)->join;

# bugid #24940 :unique should fail on my and sub declarations

for my $decl ('my $x : unique', 'sub foo : unique') {
    eval $decl;
    print $@ =~
	/^The 'unique' attribute may only be applied to 'our' variables/
	    ? '' : 'not ', "ok $test - $decl\n";
    $test++;
}


# Returing a closure from a thread caused problems. If the last index in
# the anon sub's pad wasn't for a lexical, then a core dump could occur.
# Otherwise, there might be leaked scalars.

# XXX DAPM 9-Jan-04 - backed this out for now - returning a closure from a
# thread seems to crash win32

# sub f {
#     my $x = "foo";
#     sub { $x."bar" };
# }
# 
# my $string = threads->new(\&f)->join->();
# print $string eq 'foobar' ?  '' : 'not ', "ok $test - returning closure\n";
# $test++;

1;
