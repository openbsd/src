#!/usr/bin/perl -w

# Based on an ftp client found in the LWP Cookbook and
# revised by Nathan V. Patwardhan <nvp@ora.com>.

# Copyright 1997 O'Reilly and Associates
# This package may be copied under the same terms as Perl itself.
#
# Code appears in the Unix version of the Perl Resource Kit

use LWP::UserAgent;
use URI::URL;

my $ua = new LWP::UserAgent;

# check to see if a JDK port exists for the OS.  i'd say
# that we should use solaris by default, but a 9meg tarfile
# is a hard pill to swallow if it won't work for somebody.  :-)
my $os_type = $^O; my $URL = lookup_jdk_port($os_type);
die("No JDK port found.  Contact your vendor for details.  Exiting.\n")
    if $URL eq '';

print "A JDK port for your OS has been found.\nContacting: ".$URL."\n";

# Now, parse the URL using URI::URL
my($jdk_file) = (url($URL)->crack)[5]; 
$jdk_file =~ /(.+)\/(.+)/; $jdk_file = $2;

print "Attempting to download: $jdk_file\n";

my $expected_length;
my $bytes_received = 0;

open(OUT, ">".$jdk_file) or die("Can't open $jdk_file: $!");
$ua->request(HTTP::Request->new('GET', $URL),
	     sub {
		 my($chunk, $res) = @_;

		 $bytes_received += length($chunk);
		 unless (defined $expected_length) {
		     $expected_length = $res->content_length || 0;
		 }
		 if ($expected_length) {
		     printf STDERR "%d%% - ",
		     100 * $bytes_received / $expected_length;
		 }
		 print STDERR "$bytes_received bytes received\n";

		 print OUT $chunk;
	     }
);
close(OUT);

sub lookup_jdk_port {
    my($port_os) = @_;
    my $jdk_hosts = 'jdk_hosts';
    my %HOSTS = ();

    open(CFG, $jdk_hosts) or die("hosts error: $!");
    while(<CFG>) {
	chop;
	($os, $host) = split(/\s*=>\s*/, $_);
	next unless $os eq $port_os;
	push(@HOSTS, $host);
    }
    close(CFG);

    return "" unless @HOSTS;
    return $HOSTS[rand @HOSTS];		# Pick one at random.
}

