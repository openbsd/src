#! /usr/bin/perl

# This script converts a "bb.out" file into a format
# suitable for processing by gprof

# Write a new-style gmon header

print pack("A4Ix12", "gmon", 1);


# The input file format contains header lines and data lines.
# Header lines contain a count of how many data lines follow before
# the next header line.  $blockcount is set to the count that
# appears in each header line, then decremented at each data line.
# $blockcount should always be zero at the start of a header line,
# and should never be zero at the start of a data line.

$blockcount=0;

while (<>) {
    if (/^File .*, ([0-9]+) basic blocks/) {
	print STDERR "Miscount: line $.\n" if ($blockcount != 0);
	$blockcount = $1;

	print pack("cI", 2, $blockcount);
    }
    if (/Block.*executed([ 0-9]+) time.* address= 0x([0-9a-fA-F]*)/) {
	print STDERR "Miscount: line $.\n" if ($blockcount == 0);
	$blockcount-- if ($blockcount > 0);

	$count = $1;
	$addr = hex $2;

	print pack("II",$addr,$count);
    }
}
