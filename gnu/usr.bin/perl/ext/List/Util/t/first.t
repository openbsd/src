#!./perl

BEGIN {
    unless (-d 'blib') {
	chdir 't' if -d 't';
	@INC = '../lib';
	require Config; import Config;
	keys %Config; # Silence warning
	if ($Config{extensions} !~ /\bList\/Util\b/) {
	    print "1..0 # Skip: List::Util was not built\n";
	    exit 0;
	}
    }
}

use List::Util qw(first);

print "1..8\n";

print "not " unless defined &first;
print "ok 1\n";

print "not " unless 9 == first { 8 == ($_ - 1) } 9,4,5,6;
print "ok 2\n";

print "not " if defined(first { 0 } 1,2,3,4);
print "ok 3\n";

print "not " if defined(first { 0 });
print "ok 4\n";

my $foo = first { $_->[1] le "e" and "e" le $_->[2] }
		[qw(a b c)], [qw(d e f)], [qw(g h i)];
print "not " unless $foo->[0] eq 'd';
print "ok 5\n";

# Check that eval{} inside the block works correctly
my $i = 0;
print "not " unless 5 == first { eval { die }; ($i == 5, $i = $_)[0] } 0,1,2,3,4,5,5;
print "ok 6\n";

print "not " if defined eval { first { die if $_ } 0,0,1 };
print "ok 7\n";

($x) = foobar();
$x = '' unless defined $x;
print "${x}ok 8\n";

sub foobar {  first { !defined(wantarray) || wantarray } "not ","not ","not " }

