#!./perl -Tw
# [perl #33173] shellwords.pl and tainting

BEGIN {
    if ( $ENV{PERL_CORE} ) {
        require Config;
        no warnings 'once';
        if ($Config::Config{extensions} !~ /\bList\/Util\b/) {
            print "1..0 # Skip: Scalar::Util was not built\n";
            exit 0;
        }
    }
}

use Text::ParseWords qw(shellwords old_shellwords);
use Scalar::Util qw(tainted);

print "1..2\n";

print "not " if grep { not tainted($_) } shellwords("$0$^X");
print "ok 1\n";

print "not " if grep { not tainted($_) } old_shellwords("$0$^X");
print "ok 2\n";
