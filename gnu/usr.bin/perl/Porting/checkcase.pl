#!/usr/bin/perl
# Finds the files that have the same name, case insensitively,
# in the current directory and its subdirectories

use warnings;
use strict;
use File::Find;

my %files;
find(sub {
	   my $name = $File::Find::name;
	   # Assumes that the path separator is exactly one character.
	   $name =~ s/^\.\..//;
	   push @{$files{lc $name}}, $name;
	 }, '.');

my $failed;

foreach (values %files) {
    if (@$_ > 1) {
	print join(", ", @$_), "\n";
	$failed++;
    }
}

print "no similarly named files found\n" unless $failed;
