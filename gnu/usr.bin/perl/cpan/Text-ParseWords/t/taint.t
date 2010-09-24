#!./perl -Tw
# [perl #33173] shellwords.pl and tainting

use Text::ParseWords qw(shellwords old_shellwords);
use Scalar::Util qw(tainted);

print "1..2\n";

print "not " if grep { not tainted($_) } shellwords("$0$^X");
print "ok 1\n";

print "not " if grep { not tainted($_) } old_shellwords("$0$^X");
print "ok 2\n";
