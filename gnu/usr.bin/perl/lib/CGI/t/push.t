#!./perl -wT

BEGIN {
	chdir 't' if -d 't';
	if ($ENV{PERL_CORE}) {
		@INC = '../lib';
	} else {
		# Due to a bug in older versions of MakeMaker & Test::Harness, we must
		# ensure the blib's are in @INC, else we might use the core CGI.pm
		unshift @INC, qw( ../blib/lib  ../blib/arch lib );
	}
}	

use Test::More tests => 12; 

use_ok( 'CGI::Push' );

ok( my $q = CGI::Push->new(), 'create a new CGI::Push object' );

# test the simple_counter() method
like( join('', $q->simple_counter(10)) , '/updated.+?10.+?times./', 'counter' );

# test do_sleep, except we don't want to bog down the tests
# there's also a potential timing-related failure lurking here
# change this variable at your own risk
my $sleep_in_tests = 0;

SKIP: {
	skip( 'do_sleep() test may take a while', 1 ) unless $sleep_in_tests;

	my $time = time;
	CGI::Push::do_sleep(2);
	is(time - $time, 2, 'slept for a while' );
}

# test push_delay()
ok( ! defined $q->push_delay(), 'no initial delay' );
is( $q->push_delay(.5), .5, 'set a delay' );

my $out = tie *STDOUT, 'TieOut';

# next_page() to be called twice, last_page() once, no delay
my %vars = (
	-next_page	=> sub { return if $_[1] > 2; 'next page' },
	-last_page	=> sub { 'last page' },
	-delay		=> 0,
);

$q->do_push(%vars);

# this seems to appear on every page
like( $$out, '/WARNING: YOUR BROWSER/', 'unsupported browser warning' );

# these should appear correctly
is( ($$out =~ s/next page//g), 2, 'next_page callback called appropriately' );
is( ($$out =~ s/last page//g), 1, 'last_page callback called appropriately' );

# send a fake content type (header capitalization varies in CGI, CGI::Push)
$$out = '';
$q->do_push(%vars, -type => 'fake' );
like( $$out, '/Content-[Tt]ype: fake/', 'set custom Content-type' );

# use our own counter, as $COUNTER in CGI::Push is now off
my $i;
$$out = '';

# no delay, custom headers from callback, only call callback once
$q->do_push(
	-delay		=> 0,
	-type		=> 'dynamic',
	-next_page	=> sub { 
		return if $i++;
		return $_[0]->header('text/plain'), 'arduk';
	 },
);

# header capitalization again, our word should appear only once
like( $$out, '/ype: text\/plain/', 'set custom Content-type in next_page()' );
is( $$out =~ s/arduk//g, 1, 'found text from next_page()' );
	
package TieOut;

sub TIEHANDLE {
	bless( \(my $text), $_[0] );
}

sub PRINT {
	my $self = shift;
	$$self .= join( $/, @_ );
}
