#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

print "1..4\n";

use Text::ParseWords;

@words = shellwords(qq(foo "bar quiz" zoo));
#print join(";", @words), "\n";

print "not " if $words[0] ne 'foo';
print "ok 1\n";

print "not " if $words[1] ne 'bar quiz';
print "ok 2\n";

print "not " if $words[2] ne 'zoo';
print "ok 3\n";

# Test quotewords() with other parameters
@words = quotewords(":+", 1, qq(foo:::"bar:foo":zoo zoo:));
#print join(";", @words), "\n";
print "not " unless join(";", @words) eq qq(foo;"bar:foo";zoo zoo);
print "ok 4\n";
