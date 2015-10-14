#!/usr/bin/perl -w

use strict;
use Socket qw(inet_pton PF_INET PF_INET6 SOCK_STREAM SOMAXCONN sockaddr_in sockaddr_in6);
use Errno;

my @socka = ();
my ($pf, $host, $sin, $sock, $badsock);


if (@ARGV < 3 or @ARGV > 4) {
	die "usage: $0 <pf> <listen> <first> [count]\n"
}

if ($> != 0) {
	die "run this script as root\n"
}

my ($af, $test_listen, $test_first, $test_count) = @ARGV;

$test_count = SOMAXCONN if (not defined $test_count);

my $test_last = $test_first + $test_count;

if ($test_first  <= 0 || 65536 <= $test_first ||
    $test_last   <= 0 || 65536 <= $test_last ||
    $test_listen <= 0 || 65536 <= $test_listen) {
	die "invalid port number\n";
}

if ($test_first > $test_last) {
	die "first must be lower than last\n";
}

if ($test_listen >= $test_first && $test_listen <= $test_last) {
	die "listen must be outside the [first..last] range\n";
}

if ($af == "4") {
	$pf = PF_INET;
	$sin = sockaddr_in($test_listen, inet_pton($pf,"127.0.0.1"));
}
elsif ($af == "6") {
	$pf = PF_INET6;
	$sin = sockaddr_in6($test_listen, inet_pton($pf,"::1"));
}
else {
	die "af must be 4 or 6\n";
}


my $orig_first = qx( sysctl -n net.inet.ip.portfirst );
chomp $orig_first;
my $orig_last  = qx( sysctl -n net.inet.ip.portlast );
chomp $orig_last;


# first < last

socket(SERVSOCK, $pf, SOCK_STREAM, getprotobyname("tcp"));
bind(SERVSOCK, $sin);
listen(SERVSOCK, SOMAXCONN);

my $rc_f = 0;

print "testing with portfirst < portlast\n";

system("sysctl net.inet.ip.portfirst=$test_first > /dev/null");
system("sysctl net.inet.ip.portlast=$test_last > /dev/null");

for ($test_first .. $test_last) {
	socket($sock, $pf, SOCK_STREAM, getprotobyname("tcp"));
	unless (connect($sock, $sin)) {
		print "failed to connect with errno $!\n";
		$rc_f = 1;
	}
	push @socka, $sock;
}

socket($badsock, $pf, SOCK_STREAM, getprotobyname("tcp"));
if (connect($badsock, $sin)) {
	print "connect() succeeded but should have failed\n";
	$rc_f = 1;
}
elsif (not $!{EADDRNOTAVAIL}) {
	print "connect() failed with errno $!, should have been EADDRNOTAVAIL\n";
	$rc_f = 1;
}
close($badsock);

while ($sock = pop @socka) {
	close($sock);
}

close(SERVSOCK);

sleep 1;

if ($rc_f == 0) {
	print "test OK\n"
}
else {
	print "test failed\n"
}

# first > last

socket(SERVSOCK, $pf, SOCK_STREAM, getprotobyname("tcp"));
bind(SERVSOCK, $sin);
listen(SERVSOCK, SOMAXCONN);

my $rc_b = 0;

print "testing with portfirst > portlast\n";

system("sysctl net.inet.ip.portfirst=$test_last > /dev/null");
system("sysctl net.inet.ip.portlast=$test_first > /dev/null");

for ($test_first .. $test_last) {
	socket($sock, $pf, SOCK_STREAM, getprotobyname("tcp"));
	unless (connect($sock, $sin)) {
		print "failed to connect with errno $!\n";
		$rc_b = 1;
	}
	push @socka, $sock;
}

socket($badsock, $pf, SOCK_STREAM, getprotobyname("tcp"));
if (connect($badsock, $sin)) {
	print "connect() succeeded but should have failed\n";
	$rc_b = 1;
}
elsif (not $!{EADDRNOTAVAIL}) {
	print "connect() failed with errno $!, should have been EADDRNOTAVAIL\n";
	$rc_b = 1;
}
close($badsock);

while ($sock = pop @socka) {
	close($sock);
}

close(SERVSOCK);

sleep 1;

if ($rc_b == 0) {
	print "test OK\n"
}
else {
	print "test failed\n"
}

system("sysctl net.inet.ip.portfirst=$orig_first > /dev/null");
system("sysctl net.inet.ip.portlast=$orig_last > /dev/null");

exit ($rc_f || $rc_b);
