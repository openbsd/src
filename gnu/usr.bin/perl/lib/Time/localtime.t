#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

BEGIN {
    our $haslocal;
    eval { my $n = localtime 0 };
    $haslocal = 1 unless $@ && $@ =~ /unimplemented/;
    unless ($haslocal) { print "1..0 # Skip: no localtime\n"; exit 0 }
}

BEGIN {
    our @localtime = localtime 0; # This is the function localtime.
    unless (@localtime) { print "1..0 # Skip: localtime failed\n"; exit 0 }
}

print "1..10\n";

use Time::localtime;

print "ok 1\n";

my $localtime = localtime 0 ; # This is the OO localtime.

print "not " unless $localtime->sec   == $localtime[0];
print "ok 2\n";

print "not " unless $localtime->min   == $localtime[1];
print "ok 3\n";

print "not " unless $localtime->hour  == $localtime[2];
print "ok 4\n";

print "not " unless $localtime->mday  == $localtime[3];
print "ok 5\n";

print "not " unless $localtime->mon   == $localtime[4];
print "ok 6\n";

print "not " unless $localtime->year  == $localtime[5];
print "ok 7\n";

print "not " unless $localtime->wday  == $localtime[6];
print "ok 8\n";

print "not " unless $localtime->yday  == $localtime[7];
print "ok 9\n";

print "not " unless $localtime->isdst == $localtime[8];
print "ok 10\n";




