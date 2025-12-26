#!/usr/bin/perl
use strict;
use warnings;
use POSIX qw(strftime);

# Generate .patch for later usage by smokers that are not synching via git

# Format date
sub iso_time_with_dot {
    return strftime "%Y-%m-%d.%H:%M:%S", gmtime(shift || time)
}

# Generate the content of .patch for HEAD
chomp(my ($git_dir, $sha1) = `git rev-parse --git-dir HEAD`);
die "Not in a git repository!" if !$git_dir;

my $branch = `git rev-parse --abbrev-ref HEAD`;
chomp $branch;

my $tstamp = iso_time_with_dot(`git log -1 --pretty="format:%ct" HEAD`);
my $describe= `git describe HEAD`;
chomp $describe;
my $dot_patch = join(" ", $branch, $tstamp, $sha1, $describe);

print $dot_patch, -t STDOUT ? "\n" : "";
