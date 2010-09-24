#!./perl -w

BEGIN {
    require Config; import Config;
    if ($^O ne 'VMS' and $Config{'extensions'} !~ /\bPOSIX\b/) {
	print "1..0\n";
	exit 0;
    }
}

use POSIX;
use strict ;

# E.g. \t might or might not be isprint() depending on the locale,
# so let's reset to the default.
setlocale(LC_ALL, 'C') if $Config{d_setlocale};

$| = 1;

# List of characters (and strings) to feed to the is<xxx> functions.
#
# The left-hand side (key) is a character or string.
# The right-hand side (value) is a list of character classes to which
# this string belongs.  This is a *complete* list: any classes not
# listed, are expected to return '0' for the given string.
my %classes =
  (
   'a'    => [ qw(print graph alnum alpha lower xdigit) ],
   'A'    => [ qw(print graph alnum alpha upper xdigit) ],
   'z'    => [ qw(print graph alnum alpha lower) ],
   'Z'    => [ qw(print graph alnum alpha upper) ],
   '0'    => [ qw(print graph alnum digit xdigit) ],
   '9'    => [ qw(print graph alnum digit xdigit) ],
   '.'    => [ qw(print graph punct) ],
   '?'    => [ qw(print graph punct) ],
   ' '    => [ qw(print space) ],
   "\t"   => [ qw(cntrl space) ],
   "\001" => [ qw(cntrl) ],

   # Multi-character strings.  These are logically ANDed, so the
   # presence of different types of chars in one string will
   # reduce the list on the right.
   'abc'       => [ qw(print graph alnum alpha lower xdigit) ],
   'az'        => [ qw(print graph alnum alpha lower) ],
   'aZ'        => [ qw(print graph alnum alpha) ],
   'abc '      => [ qw(print) ],

   '012aF'     => [ qw(print graph alnum xdigit) ],

   " \t"       => [ qw(space) ],

   "abcde\001" => [],

   # An empty string. Always true (al least in old days) [bug #24554]
   ''     => [ qw(print graph alnum alpha lower upper digit xdigit
                  punct cntrl space) ],
  );


# Pass 1: convert the above arrays to hashes.  While doing so, obtain
# a complete list of all the 'is<xxx>' functions.  At least, the ones
# listed above.
my %functions;
foreach my $s (keys %classes) {
    $classes{$s} = { map {
	$functions{"is$_"}++;	# Keep track of all the 'is<xxx>' functions
	"is$_" => 1;		# Our return value: is<xxx>($s) should pass.
    } @{$classes{$s}} };
}

# Expected number of tests is one each for every combination of a
# known is<xxx> function and string listed above.
require '../../t/test.pl';
plan(tests => keys(%classes) * keys(%functions));


#
# Main test loop: Run all POSIX::is<xxx> tests on each string defined above.
# Only the character classes listed for that string should return 1.  We
# always run all functions on every string, and expect to get 0 for the
# character classes not listed in the given string's hash value.
#
foreach my $s (sort keys %classes) {
    foreach my $f (sort keys %functions) {
	my $expected = exists $classes{$s}->{$f};
	my $actual   = eval "POSIX::$f( \$s )";

	ok( $actual == $expected, "$f('$s') == $actual");
    }
}
