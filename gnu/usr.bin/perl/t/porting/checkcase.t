#!/usr/bin/perl
# Finds the files that have the same name, case insensitively,
# in the current directory and its subdirectories

use warnings;
use strict;
use File::Find;

my %files;
my $test_count = 0;

find(sub {
	   my $name = $File::Find::name;
	   # Assumes that the path separator is exactly one character.
	   $name =~ s/^\.\..//;

	   # Special exemption for Makefile, makefile
	   return if $name =~ m!\A(?:x2p/)?[Mm]akefile\z!;

	   push @{$files{lc $name}}, $name;
	 }, '..');

foreach (values %files) {
    if (@$_ > 1) {
		print "not ok ".++$test_count. " - ". join(", ", @$_), "\n";
    } else {
		print "ok ".++$test_count. " - ". join(", ", @$_), "\n";
	}
}

print "1..".$test_count."\n";
