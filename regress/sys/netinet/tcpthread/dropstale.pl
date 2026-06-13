#!/usr/bin/perl -T

use strict;
use warnings;
use Errno;
use Socket;
use Socket6;

$ENV{PATH} = "/bin:/sbin:/usr/bin:/usr/sbin";
my $addrfile = $ARGV[0]
    or die "usage: $0 addrfile\n";

open(my $lp, '<', $addrfile)
    or die "Open '$addrfile' for reading failed: $!";
my %address;
foreach (<$lp>) {
    chomp;
    @address{$_} = 1;
}
close($lp)
    or die "Close '$addrfile' after reading failed: $!";

my @netstat = qw(/usr/bin/netstat -nv -p tcp);
open(my $ns, '-|', @netstat)
    or die "Open pipe from '@netstat' failed: $!";

my $dropped = 0;
foreach (<$ns>) {
    my ($proto, undef, undef, $local, $foreign, $state) = split;
    $proto =~ /^tcp/ or next;
    $state =~ /^TIME_WAIT/ or next;
    $address{$local} || $address{$foreign} or next;
    $dropped++ if tcpdrop($local, $foreign);
}
print "TIME_WAIT connections dropped: $dropped\n";

close($ns) or die $! ?
    "Close pipe from netstat failed: $!" :
    "'@netstat' failed: $?";

exit;

# fork and exec tcpdrop(8) is too expensive, implement in pure Perl
sub tcpdrop {
    my ($local, $foreign) = @_;

    my ($laddr, $faddr);
    if ($local =~ /:/) {
	    my ($addr, $port) = $local =~ /^([0-9a-f:]+)\.([0-9]+)$/
		or die "invalid local address format: $local";
	    $laddr = pack_sockaddr_in6($port, inet_pton(AF_INET6, $addr));
    } else {
	    my ($addr, $port) = $local =~ /^([0-9.]+)\.([0-9]+)$/
		or die "invalid local address format: $local";
	    $laddr = pack_sockaddr_in($port, inet_pton(AF_INET, $addr));
    }
    if ($foreign =~ /:/) {
	    my ($addr, $port) = $foreign =~ /^([0-9a-f:]+)\.([0-9]+)$/
		or die "invalid foreign address format: $foreign";
	    $faddr = pack_sockaddr_in6($port, inet_pton(AF_INET6, $addr));
    } else {
	    my ($addr, $port) = $foreign =~ /^([0-9.]+)\.([0-9]+)$/
		or die "invalid foreign address format: $foreign";
	    $faddr = pack_sockaddr_in($port, inet_pton(AF_INET, $addr));
    }

    #	struct tcp_ident_mapping {
    #		struct sockaddr_storage faddr, laddr;
    #		int euid, ruid;
    #		u_int rdomain;
    #	};
    my $tcp_ident_mapping = pack("a256a256iiIx4", $faddr, $laddr, 0, 0, 0);

    #	#define CTL_NET		4
    #	#define PF_INET		AF_INET
    #	#define AF_INET		2
    #	#define IPPROTO_TCP	6
    #	#define TCPCTL_DROP	19
    my $mib = pack("i4", 4, 2, 6, 19);
    #	#define SYS_sysctl	202
    if (syscall(202, $mib, 4, 0, 0, $tcp_ident_mapping,
	length($tcp_ident_mapping)) != 0) {
	    return 0 if $!{ESRCH};
	    die "syscall sysctl TCPCTL_DROP $local $foreign: $!";
    }
    return 1;
}
