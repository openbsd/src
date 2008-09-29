#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

# WARNING: Do not use anymodules as part of this test code.
# We could get action at a distance that would invalidate the tests.

print "1..2\n";

# This tests whether glob assignment fails to load the tie.
*X = *-;
'X'=~/(?<X>X)/;
print eval '*X{HASH}{X} || 1' ? "" :"not ","ok ",++$test,"\n";

# And since its a similar case we check %! as well
*Y = *!;
print 0<keys(%Y) ? "" :"not ","ok ",++$test,"\n";
