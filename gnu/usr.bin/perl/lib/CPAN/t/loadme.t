#!/usr/bin/perl -w

BEGIN {
	chdir 't' if -d 't';
	@INC = '../lib';
}

BEGIN {
    print "1..1\n";
}
use strict;
use CPAN;
use CPAN::FirstTime;

print "ok 1\n";

