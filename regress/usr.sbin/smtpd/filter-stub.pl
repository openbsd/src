#!/usr/bin/perl
# filter-stub.pl -- minimal OpenSMTPD filter for regression testing
#
# Implements the OpenSMTPD filter protocol v0.7.  Registers for the phase
# named on the command line and responds with the action that follows:
#
#   filter-stub.pl <phase> <action> [<message>]
#
# Examples:
#   filter-stub.pl mail-from reject "550 5.7.0 Sender rejected"
#   filter-stub.pl rcpt-to   junk
#   filter-stub.pl ehlo      proceed
#
# stdin/stdout are the socket pair with smtpd (set up by fork_filter_process).
# stderr goes to smtpd's error fd and is logged.

use strict;
use warnings;

$| = 1;   # autoflush stdout

my $phase   = shift or die "usage: filter-stub.pl <phase> <action> [msg]\n";
my $action  = shift or die "usage: filter-stub.pl <phase> <action> [msg]\n";
my $message = shift // "";   # optional rejection/disconnect message

print STDERR "filter-stub: phase=$phase action=$action\n";

# -----------------------------------------------------------------------
# Handshake: consume config lines until "config|ready", then register.
# -----------------------------------------------------------------------
while (my $line = <STDIN>) {
	chomp $line;
	print STDERR "filter-stub <<< $line\n";
	last if $line eq 'config|ready';
}

print STDOUT "register|filter|smtp-in|$phase\n";
print STDOUT "register|ready\n";
print STDERR "filter-stub: registered for smtp-in|$phase, action=$action\n";

# -----------------------------------------------------------------------
# Main loop: handle filter events for the registered phase.
# -----------------------------------------------------------------------
while (my $line = <STDIN>) {
	chomp $line;
	print STDERR "filter-stub <<< $line\n";

	# Event format:
	#   filter|version|timestamp|smtp-in|phase|reqid|token|params...
	# Response format:
	#   filter-result|reqid|token|action[|parameter]
	# Both reqid (f[5]) and token (f[6]) must be echoed back.
	my @f = split(/\|/, $line, -1);
	next unless @f >= 7 && $f[0] eq 'filter' && $f[4] eq $phase;

	my $reqid = $f[5];
	my $token = $f[6];

	if ($action eq 'proceed') {
		print STDOUT "filter-result|$reqid|$token|proceed\n";
	} elsif ($action eq 'junk') {
		print STDOUT "filter-result|$reqid|$token|junk\n";
	} elsif ($action eq 'reject') {
		my $msg = $message || "550 5.7.0 Rejected by filter";
		print STDOUT "filter-result|$reqid|$token|reject|$msg\n";
	} elsif ($action eq 'disconnect') {
		my $msg = $message || "421 4.3.0 Service temporarily unavailable";
		print STDOUT "filter-result|$reqid|$token|disconnect|$msg\n";
	} else {
		die "filter-stub: unknown action '$action'\n";
	}
	print STDERR "filter-stub: responded $action to reqid=$reqid token=$token\n";
}

print STDERR "filter-stub: stdin closed, exiting\n";
