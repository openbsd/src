#!./perl -w

BEGIN {
	unshift @INC, 't';
	require Config;
	if (($Config::Config{'extensions'} !~ /\bB\b/) ){
		print "1..0 # Skip -- Perl configured without B module\n";
		exit 0;
	}
	require 'test.pl';
}

use strict;
use Config;
use File::Spec;
use File::Path;

my $path = File::Spec->catdir( 'lib', 'B' );
unless (-d $path) {
	mkpath( $path ) or skip_all( 'Cannot create fake module path' );
}

my $file = File::Spec->catfile( $path, 'success.pm' );
local *OUT;
open(OUT, '>', $file) or skip_all( 'Cannot write fake backend module');
print OUT while <DATA>;
close *OUT;

plan( 9 ); # And someone's responsible.

# use() makes it difficult to avoid O::import()
require_ok( 'O' );

my @args = ('-Ilib', '-MO=success,foo,bar', '-e', '1' );
my @lines = get_lines( @args );

is( $lines[0], 'Compiling!', 'Output should not be saved without -q switch' );
is( $lines[1], '(foo) <bar>', 'O.pm should call backend compile() method' );
is( $lines[2], '[]', 'Nothing should be in $O::BEGIN_output without -q' );
is( $lines[3], '-e syntax OK', 'O.pm should not munge perl output without -qq');

$args[1] = '-MO=-q,success,foo,bar';
@lines = get_lines( @args );
isnt( $lines[1], 'Compiling!', 'Output should not be printed with -q switch' );

SKIP: {
	skip( '-q redirection does not work without PerlIO', 2)
		unless $Config{useperlio};
	is( $lines[1], "[Compiling!", '... but should be in $O::BEGIN_output' );

	$args[1] = '-MO=-qq,success,foo,bar';
	@lines = get_lines( @args );
	is( scalar @lines, 3, '-qq should suppress even the syntax OK message' );
}

$args[1] = '-MO=success,fail';
@lines = get_lines( @args );
like( $lines[1], qr/fail at .eval/,
	'O.pm should die if backend compile() does not return a subref' );

sub get_lines {
	split(/[\r\n]+/, runperl( args => [ @_ ], stderr => 1 ));
}

END {
	1 while unlink($file);
	rmdir($path); # not "1 while" since there might be more in there
}

__END__
package B::success;

$| = 1;
print "Compiling!\n";

sub compile {
	return 'fail' if ($_[0] eq 'fail');
	print "($_[0]) <$_[1]>\n";
	return sub { print "[$O::BEGIN_output]\n" };
}

1;
