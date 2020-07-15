#!/usr/bin/perl

# $OpenBSD: check.perl,v 1.2 2020/07/15 03:49:59 beck Exp $
#
# Copyright (c) 2020 Bob Beck <beck@openbsd.org>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#

my $num_args = $#ARGV + 1;
if ($num_args != 3) {
    print "\nUsage: test.perl expected known testoutput\n";
    exit 1;
}

my $expected_file=$ARGV[0];
my $known_file=$ARGV[1];
my $output_file=$ARGV[1];

open (OUT, "<$output_file") || die "can't open $output_file";
open (KNOWN, "<$known_file") || die "can't open $known_file";
open (EXPECTED, "<$expected_file") || die "can't open $expected_file";

my @expectedip;
my @expecteddns;
my @knownip;
my @knowndns;
my @outip;
my @outdns;

my $i = 0;
while(<OUT>) {
    chomp;
    my @line = split(',');
    my $id = $line[0];
    die "$id mismatch with $i" if ($id != $i + 1);
    $outdns[$i] = $line[1];
    $outip[$i] = $line[2];
    $i++;
}
$i = 0;
while(<KNOWN>) {
    chomp;
    my @line = split(',');
    my $id = $line[0];
    die "$id mismatch with $i" if ($id != $i + 1);
    $knowndns[$i] = $line[1];
    $knownip[$i] = $line[2];
    $i++;
}
$i = 0;
while(<EXPECTED>) {
    chomp;
    my @line = split(',');
    my $id = $line[0];
    die "$id mismatch with $i" if ($id != $i + 1);
    $expecteddns[$i] = $line[1];
    $expectedip[$i] = $line[2];
    $i++;
}
my $id;
my $regressions = 0;
my $known = 0;
for ($id = 0; $id < $i; $id++) {
    my $ipknown = ($outip[$id] eq $knownip[$id]);
    my $dnsknown = ($outdns[$id] eq $knowndns[$id]);
    if ($expecteddns[$id] ne $outdns[$id] && $expecteddns[$id] !~ /WEAK/) {
	print STDERR "$id DNS  expected $expecteddns[$id] known $knowndns[$id] result $outdns[$id]";
	if ($dnsknown) {
	    print " (known failure)\n";
	    $known++;
	} else {
	    print " (REGRESSED)\n";
	    $regressions++;
	}
    }
    if ($expectedip[$id] ne $outip[$id] && $expectedip[$id] !~ /WEAK/) {
	print "$id IP   expected $expectedip[$id] known $knownip[$id] result $outip[$id]";
	if ($ipknown) {
	    print " (known failure)\n";
	    $known++;
	} else {
	    print " (REGRESSED)\n";
	    $regressions++;
	}	
    }
}
print "\n\nTested $i certificates\n";
if ($regressions == 0) {
    print STDERR "SUCCESS - no new regressions ($known known failures)\n";
    exit 0;
} else {
    print STDERR "FAILED - $regressions new regressions ($known known failures)\n";
    exit 1;
}


