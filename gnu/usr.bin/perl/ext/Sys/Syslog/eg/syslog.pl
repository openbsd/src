#!/usr/bin/perl
use strict;
use Sys::Syslog;

die "usage: $0 facility/priority message\n" unless @ARGV;

my ($facility, $priority) = split '/', shift;
my $message = join ' ', @ARGV;

openlog($0, "ndelay,pid", $facility) or die "fatal: can't open syslog: $!\n";
syslog($priority, "%s", $message);
closelog();
