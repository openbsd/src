#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

BEGIN {
    our $hasgm;
    eval { my $n = gmtime 0 };
    $hasgm = 1 unless $@ && $@ =~ /unimplemented/;
    unless ($hasgm) { print "1..0 # Skip: no gmtime\n"; exit 0 }
}

BEGIN {
    our @gmtime = gmtime 0; # This is the function gmtime.
    unless (@gmtime) { print "1..0 # Skip: gmtime failed\n"; exit 0 }
}

print "1..10\n";

use Time::gmtime;

print "ok 1\n";

my $gmtime = gmtime 0 ; # This is the OO gmtime.

print "not " unless $gmtime->sec   == $gmtime[0];
print "ok 2\n";

print "not " unless $gmtime->min   == $gmtime[1];
print "ok 3\n";

print "not " unless $gmtime->hour  == $gmtime[2];
print "ok 4\n";

print "not " unless $gmtime->mday  == $gmtime[3];
print "ok 5\n";

print "not " unless $gmtime->mon   == $gmtime[4];
print "ok 6\n";

print "not " unless $gmtime->year  == $gmtime[5];
print "ok 7\n";

print "not " unless $gmtime->wday  == $gmtime[6];
print "ok 8\n";

print "not " unless $gmtime->yday  == $gmtime[7];
print "ok 9\n";

print "not " unless $gmtime->isdst == $gmtime[8];
print "ok 10\n";




