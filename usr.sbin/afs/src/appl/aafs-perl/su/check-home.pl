#!/usr/pkg/bin/perl -w
# $arla: check-home.pl,v 1.1 2002/06/03 02:03:43 lha Exp $

use strict;
use AAFS;

my $cell = AAFS::aafs_cell_create("su.se");

my $vldblist = AAFS::aafs_vldb_query($cell, {});

my @foo = grep {
    &AAFS::aafs_volume_examine_nvldb($$_)->{'name'} =~ /user\..*/;
} @$vldblist;

print "Found ", $#foo, " entries.\n";

foreach my $v (@foo) {
    my $q = AAFS::aafs_volume_examine_nvldb($$v);

    my $info = AAFS::aafs_volume_examine_info($$v, 1);

    my $p; my $warn = "";
    if ($info->[0]->{'MaxQuota'} eq 0) {
	$p = sprintf("Unlimited (using %d MB)", $info->[0]->{'Usage'} / 1000);
    } else {
	$p = sprintf("%2.1f %%",
		     $info->[0]->{'Usage'} / $info->[0]->{'MaxQuota'} * 100);
	if ($info->[0]->{'Usage'} / $info->[0]->{'MaxQuota'} > 0.95 &&
	    abs($info->[0]->{'MaxQuota'} - $info->[0]->{'Usage'}) < 20000) {
	    $warn = " <<<WARN";
	}
    }

    printf("%-30s\t%7s%s\n",
	   $q->{name}, 
	   $p, $warn);
}
