#!/usr/bin/perl

# $OpenBSD: spamd-setup.pl,v 1.3 2003/03/04 05:54:53 beck Exp $
#
# Copyright (c) 2003 Bob Beck <beck@openbsd.org>.  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

use strict;
use Net::Netmask;
use Socket;

my $i;
my @blacklist;
my $globalwhite;
my %Sources;
my $state=0;
my $tag;
my $file;

sub quad2int
{
	my @bytes = split(/\./,$_[0]);

	return undef unless @bytes == 4 && ! grep {!(/\d+$/ && $_<256)} @bytes;

	return unpack("N",pack("C4",@bytes));
}

sub int2quad
{
	return join('.',unpack('C4', pack("N", $_[0])));
}

sub valid_addr() {
	my @bytes = split(/\./,$_[0]);
	return undef unless @bytes == 4 && ! grep {!(/\d+$/ && $_<256)} @bytes;
	return($_[0]);
}

sub prev_addr() {
	my @bytes = split(/\./,$_[0]);
	return undef unless @bytes == 4 && ! grep {!(/\d+$/ && $_<256)} @bytes;
	return int2quad (unpack("N",pack("C4",@bytes)) - 1);
}

sub next_addr() {
	my @bytes = split(/\./,$_[0]);
	return undef unless @bytes == 4 && ! grep {!(/\d+$/ && $_<256)} @bytes;
	return int2quad (unpack("N",pack("C4",@bytes)) + 1);
}

# retrieve lists of netblocks or ip's from a list or urls as first
# arg. Add valid addresses and blocks seen to seen hash (second arg),
# as well as incrementing and decrementing start/end values in list hash
# (third arg) for later use in computing where the list actually
# starts and ends.
sub retrieve_lists() { my ($urls, $seen, $list) = @_;
    my $file = shift(@{$urls});
    for (; $file && open (LIST, "ftp -V -o - $file |");
	$file=shift(@{$urls})) {
	while (<LIST>) {
	    my ($block, $start, $end);
	    # vanna vanna, find me a netblock  we assume one per line
	    chomp;
	    $start = undef;
	    $end = undef;
	    next if (/^\s*\#/);
	    if ($_ =~ m/^(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}\/\d{1,2})/) {
		#We're in CIDR format.......
		$block = new Net::Netmask($1);
		if (!$block->{'ERROR'}) {
		    $start = $block->base();
		    $end = $block->next();
		}
	    } elsif ($_ =~ m/^(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3})[\s-]+(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,2})/){
		#We're in somthing like startaddress - endaddress (Whois)
		$start = &valid_addr($1);
		$end = &next_addr($2);
	    } elsif ($_ =~ m/^(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}):(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3})/) {
		#We're in somthing like address:mask
		$block = new Net::Netmask("$1:$2");
		if (!$block->{'ERROR'}) {
		    $start = $block->base();
		    $end = $block->next();
		}
	    }
	    elsif ($_ =~ m/^(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3})/) {
		#We're just a single solitary IP.
		$start = &valid_addr($1);
		$end = &next_addr($1);
	    } else  {
		# skip anything that doesn't look like an IP or netblock.
		next;
	    }
	    if ($start && $end) {
		${$seen}{"$end"}++;
		${$seen}{"$start"}++;
		${$list}{"$start"}++;
		${$list}{"$end"}--;
	    }
	}
	close(LIST);
    }
}

# make a spamd blacklist - retrieve addressen/netblocks from sources
# in first arg, remove errors or execptions from sources in second
# arg, return a list of strings, consisting of the actual netblocks to
# blacklist in CIDR format.
sub blacklist () { my @args = @_;

    my $i;
    my (@blackurls, @errurls);
    my (%Seen, %Black, %White, @BlackBlocks);
    my ($blackval, $whiteval, $blackstart, $laststate);
    my $j = 0;

    # first, snarf the blacklist and extract the addresses.
    for ($i = 0; $i<=$#args; $i++) {
	if ($args[$i] =~ /^-w$/) {
	    $j = 1;
	} else {
	    if ($j == 0) {
		push (@blackurls, $args[$i]);
	    } else {
		push (@errurls, $args[$i]);
	    }
	}
    }

    &retrieve_lists(\@blackurls, \%Seen, \%Black);
    &retrieve_lists(\@errurls, \%Seen, \%White);

    foreach $a (sort_by_ip_address (keys %Seen)) {
	my $newblack;
	my $newwhite;
	my $state;
	$newblack = $Black{$a}?$Black{$a}:$blackval;
	$newwhite = $White{$a}?$White{$a}:$whiteval;
	if ($state == 0 && $newblack > 0) {
	    $state = 1;
	}
	elsif ($state == 1 && $newblack == 0) {
	    $state = 0;
	}
	if ($newwhite > 0) {
	    $state = 0;
	}
	if ($laststate == 0 && $state == 1) {
	    # start a blacklist
	    $blackstart = $a;
	}
	if ($laststate == 1 && $state == 0) {
	    # end a blacklist
	    push (@BlackBlocks, ( map {$_->desc()}
		(range2cidrlist ($blackstart, &prev_addr($a)))));
	}
	$laststate = $state;
	$blackval = $newblack;
	$whiteval = $newwhite;
    }
    return @BlackBlocks;
}


# Tell spamd about a blacklist.
# returns list of blacklisted CIDR's, suitable for adding to pf rdr.
sub addblack () {
    my ($name, $message, @urls) = @_;
    my @blacknets = &blacklist(@urls);

    if ($#blacknets >= 0) {
	my ($i, $remote, $port, $iaddr, $paddr, $proto, $line);

	# tell spamd about it
	$remote = '127.0.0.1';
	$port = 8026;
	$iaddr = inet_aton($remote);
	$paddr = sockaddr_in($port, $iaddr);
	$proto = getprotobyname('tcp');
	socket(SOCK, PF_INET, SOCK_STREAM, $proto) || die "socket $!";
	connect(SOCK, $paddr) || die "connect: $!";
	$line = "$name;$message;".join(";", @blacknets)."\n";
	print SOCK $line;
	close (SOCK);
    }
    return(@blacknets); # caller must add to pf table.
}



while ($_ = shift(@ARGV)) {
    if ($_ !~ /^-/) {
	unshift(@ARGV, $_);
	last;
    } else {
	if ( /-s/ || /-1/ ) {
	    # spews level 1;
	    $Sources{"spews-1"}= [ "spews-1",
		"\"SPAM. Your address %A is in spews level 1\\n".
		"See http://www.spews.org/ask.cgi?x=%A for more details\\n\"",
		"http://www.spews.org/spews_list_level1.txt", "-w" ];
	} elsif (/-2/) {
	    # spews level 2
	    $Sources{"spews-2"}= [ "spews-2",
		"\"SPAM. Your address %A is in spews level 2\\n".
		"See http://www.spews.org/ask.cgi?x=%A for more details\\n\"",
		"http://www.spews.org/spews_list_level2.txt", "-w" ];
	} elsif (/-k/) {
	    # korea
	    $Sources{"korea-okean"}= [ "korea-okean",
		"\"SPAM. Your address %A appears to be from korea\\n".
		"See http://www.okean.com/asianspamblocks.html for more".
		" details\\n\"",
		"http://www.okean.com/koreacidr.txt", "-w" ];
	} elsif (/-c/) {
	    # china
	    $Sources{"china-okean"}= [ "china-okean",
		"\"SPAM. Your address %A appears to be from china\\n".
		"See http://www.okean.com/asianspamblocks.html for more".
		" details\\n\"",
		"http://www.okean.com/chinacidr.txt", "-w" ];
	} elsif (/-w/) {
	    # global whitelist file
	    $globalwhite = shift(@ARGV);
	} elsif (/-f/) {
	    # local blacklist file
	    my $blackfile = shift(@ARGV);
	    if ($blackfile) {
		$Sources{"localblock"}= [ "localblock",
		   "\"SPAM. Your address %A has been locally blocked\\n" .
		   "as a SPAM source\\n\"",
		   "file:$blackfile", "-w" ];
	    }
	}
    }
}

## read remaining input as files, setting up other lists.
##
## format must be
#SPAMD_SOURCE:tag:"message to send to luser"
#blacklist url
#blacklist url
#SPAMD_SOURCE_REMOVE
#removelist url
#removelist url
while ($file = shift(@ARGV)) {
    open(SETUP, "<$file") || die ("can't open $file");
    while (<SETUP>) {
	chomp;
	next if (/^\s*\#/);
	if (/^SPAMD_SOURCE;([^;]*);(\".*\")$/) {
	    if ($state == 1) {
		# finish off last one.
		push(@{$Sources{$tag}}, "-w");
	    }
	    $state = 1;
	    $tag = $1;
	    $Sources{$tag}=[ $tag, $2 ];
	} elsif ($state > 0) {
	    if (/^SPAMD_SOURCE_REMOVE$/) {
		push(@{$Sources{$tag}}, "-w");
		$state = 2;
	    }
	    else  {
		push(@{$Sources{$tag}}, $_);
	    }
	}
    }
}


foreach  $i (keys %Sources) {
    my @args = @{$Sources{$i}};
    if ($globalwhite) {
	push(@args, "file:$globalwhite");
    }
    push (@blacklist, &addblack(@args));
}

# replace spamd table with new blacklist.
open (PFCTL, "|pfctl -q -t spamd -T replace -f -") ||
    die ("can't exec pfctl");
for ($i=0; $i<=$#blacklist; $i++) {
    print PFCTL $blacklist[$i], "\n";
}
close(PFCTL);
