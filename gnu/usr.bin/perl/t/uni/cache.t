BEGIN {
    chdir 't' if -d 't';
    @INC = qw(../lib .);
    require "test.pl";
}

plan tests => 1;

# Looks to see if a "do 'unicore/lib/Sc/Hira.pl'" is called more than once, by
# putting a compile sub first on the libary path;
# XXX Kludge: requires exact path, which might change, and has deep knowledge
# of how utf8_heavy.pl works, which might also change.

BEGIN { # Make sure catches compile time references
    $::count = 0;
    unshift @INC, sub {
       $::count++ if $_[1] eq 'unicore/lib/Sc/Hira.pl';
    };
}

my $s = 'foo';

# The second value is to prevent an optimization that exists at the time this
# is written to re-use a property without trying to look it up if it is the
# only thing in a character class.  They differ in order to make sure that any
# future optimizations that don't re-use identical character classes don't come
# into play
$s =~ m/[\p{Hiragana}\x{101}]/;
$s =~ m/[\p{Hiragana}\x{102}]/;
$s =~ m/[\p{Hiragana}\x{103}]/;
$s =~ m/[\p{Hiragana}\x{104}]/;

is($::count, 1, "Swatch hash caching kept us from reloading swatch hash.");
