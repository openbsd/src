#!/usr/bin/perl
# $OpenBSD: run.pl,v 1.1 1999/08/05 22:42:14 niklas Exp $
# $EOM: run.pl,v 1.2 1999/08/05 22:42:42 niklas Exp $

use strict;
require 5.002;
require 'sys/syscall.ph';
use Socket;
use Sys::Hostname;

my ($rfd, $tickfac, $myaddr, $myport, $hisaddr, $hisport, $proto, $bindaddr,
    $conaddr, $sec, $tick, $action, $template, $data, $next,
    $nfd, $pkt, $verbose);

$| = 1;

$verbose = 1;
$tickfac = 0.001;
$myaddr = gethostbyname ('127.0.0.1');
$myport = 1501;
    $hisaddr = inet_aton ('127.0.0.1');
$hisport = 1500;

$proto = getprotobyname ('udp');
$bindaddr = sockaddr_in ($myport, $myaddr);
socket (SOCKET, PF_INET, SOCK_DGRAM, $proto) || die "socket: $!";
bind (SOCKET, $bindaddr);
vec ($rfd, fileno SOCKET, 1) = 1;

$conaddr = sockaddr_in ($hisport, $hisaddr);

sub getsec
{
    my ($tv) = pack ("ll", 0, 0);
    my ($tz) = pack ("ii", 0, 0);
    syscall (&SYS_gettimeofday, $tv, $tz) && return undef;
    my ($sec, $usec) = unpack ("ll", $tv);
    $sec % 86400 + $usec / 1000000;
}

$sec = &getsec;
while (<>) {
    next if /^\s*#/o || /^\s*$/o;
    chop;
    ($tick, $action, $template, $data) = split ' ', $_, 4;
    while ($data =~ /\\$/o) {
	chop $data;
	$_ = <>;
	next if /^\s*#/o;
	chop;
	$data .= $_;
    }
    $data =~ s/\s//go;
    $data = pack $template, $data;
    $next = $sec + $tick * $tickfac;
    if ($action eq "send") {
	# Wait for the moment to come.
	print STDERR "waiting ", $next - $sec, " secs\n";
	select undef, undef, undef, $next - $sec
	    while ($sec = &getsec) < $next;
#	print $data;
	send SOCKET, $data, 0, $conaddr;
	print STDERR "sent ", unpack ("H*", $data), "\n" if $verbose;
    } elsif ($action eq "recv") {
	$sec = &getsec;
	printf (STDERR "waiting for data or the %.3f sec timeout\n",
		$next - $sec);
	$nfd = select $rfd, undef, undef, $next - $sec;
	if ($nfd) {
	    printf STDERR "got back after %.3f secs\n", &getsec - $sec
		if $verbose;
#	    sysread (STDIN, $pkt, 65536) if $nfd;
	    sysread (SOCKET, $pkt, 65536) if $nfd;
	    print STDERR "read ", unpack ("H*", $pkt), "\n" if $verbose;
	    print STDERR "cmp  ", unpack ("H*", $data), "\n" if $verbose;
	} else {
	    print STDERR "timed out\n" if $verbose;
	}
	die "mismatch\n" if $pkt ne $data;
    }
}
