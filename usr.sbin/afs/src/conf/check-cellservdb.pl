#!/usr/pkg/bin/perl -w
# $arla: check-cellservdb.pl,v 1.4 2000/08/30 21:26:13 lha Exp $
#
#  Copyright (c) 1995 - 2000 Kungliga Tekniska Högskolan
#  (Royal Institute of Technology, Stockholm, Sweden).
#  All rights reserved.
#  
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions
#  are met:
#  
#  1. Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#  
#  2. Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#  
#  3. Neither the name of the Institute nor the names of its contributors
#     may be used to endorse or promote products derived from this software
#     without specific prior written permission.
#  
#  THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
#  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
#  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
#  ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
#  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
#  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
#  OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
#  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
#  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
#  OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
#  SUCH DAMAGE.

#
# You can install Net::DNS with ``perl -MCPAN -e install Net::DNS;''
#

use strict;
use Net::DNS;
use Getopt::Long;

my $cell;
my $found;
my %db;
my %comments;

sub print_cell
{
    my $cell = shift;
    my $comment = shift || "";

    print ">$cell		\#$comment\n";
}

sub query_local_cell
{
    my $cell = shift;

    print "local cellservdb\n";
    print_cell($cell,$comments{$cell});
    my $hostlist = $db{$cell};
    print foreach (@$hostlist);
}

sub query_remote_cell
{
    my $cell = shift;
    my $hostlist = $db{$cell};

    print "query remote host\n";
    my $host;
    foreach (@$hostlist) {
	if (/^([^ \t\n]+)/) {
	    system "bos listhost -server $1 -db -comment \"$comments{$cell}\" -noauth";
	    last if ($? == 0);
	}
    }
}
    
sub query_afsdb 
{
    my $cell = shift;
    my $comment = $comments{$cell};
    my $res;
    my $query;
    my $rr;
    my @hosts = ();
    my $host;

    $res = new Net::DNS::Resolver;
    $query = $res->search($cell, "AFSDB");
    
    if ($query) {
	foreach $rr ($query->answer) {
	    next unless $rr->type eq "AFSDB" and $rr->subtype == 1;
	    push @hosts, $rr->hostname;
	}
    }
    if ($#hosts > 0) {
	printf ("query dns\n");
	print_cell($cell, $comment);
    }
    foreach $host (@hosts) {
	$query = $res->search($host, "A");
	if ($query) {
	    foreach $rr ($query->answer) {
		next unless $rr->type eq "A";
		print $rr->address, "		\#$host\n";
	    }
	}
    }
}    

sub parse_cellservdb
{
    my $cellservdb = shift || "/usr/arla/etc/CellServDB";
    my @hosts;
    my $FILE;
    
    open FILE, "<$cellservdb";
	
    while (<FILE>) {
	$found = 0;
	if (/^>([^ \t]*)[\t ]*#(.*)/) {
	    $cell = $1;
	    $comments{$cell} = $2;
	    $found = 1;
	} elsif (/^>([^ \t]*)/) {
	    $cell = $1;
	    $comments{$cell} = "";
	    $found = 1;
	}
	if (!$found) {
	    push @hosts, $_;
	    my @hostcopy = @hosts;
	    $db{$cell} = \@hostcopy;
	} else {
	    while (defined(pop @hosts)) {} 
	}
    }
    close FILE;
}

sub usage
{
    print "check-cellservdb.pl";
    print "\t--help\n";
    print "\t--cell <cellname>\n";
    print "\t--CellServDB <file>\n";
    print "\t--nodns\n";
    print "\t--nolocal\n";
    print "\t--noremote\n";
    print "\t--all\n";
    exit 1;
}

my $cell = 0;
my $all = 0;
my $nodns = 0;
my $noremote = 0;
my $nolocal = 0;
my $cellservdb;

GetOptions("help" => \&usag,
	   "cell=s" => \$cell,
	   "CellServDB=s" => \$cellservdb,
	   "nodns" => \$nodns,
	   "noremote" => \$noremote,
	   "nolocal" => \$nolocal,
	   "all" => \$all);


parse_cellservdb($cellservdb);

if ($all) {
    my $query_cell;
    foreach $query_cell (sort { reverse($a) cmp reverse($b) } keys %db) {
	
	query_local_cell($query_cell) if(!$nolocal);
	query_remote_cell($query_cell) if(!$noremote);
	query_afsdb($query_cell) if (!$nodns);
    }
} elsif ($cell) {
    query_local_cell($cell) if(!$nolocal);
    query_remote_cell($cell) if(!$noremote);
    query_afsdb($cell) if (!$nodns);
} else {
    usage();
}
