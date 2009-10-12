#!/usr/bin/perl -w
use strict;

# Print out any PERL_ARGS_ASSERT* macro that was declared but not used.

my %declared;
my %used;

open my $fh, '<', 'proto.h' or die "Can't open proto.h: $!";
while (<$fh>) {
    $declared{$1}++ if /^#define\s+(PERL_ARGS_ASSERT[A-Za-z_]+)\s+/;
}

if (!@ARGV) {
    open my $fh, '<', 'MANIFEST' or die "Can't open MANIFEST: $!";
    while (<$fh>) {
	# *.c or */*.c
	push @ARGV, $1 if m!^((?:[^/]+/)?[^/]+\.c)\t!;
    }
}

while (<>) {
    $used{$1}++ if /^\s+(PERL_ARGS_ASSERT_[A-Za-z_]+);$/;
}

my %unused;

foreach (keys %declared) {
    $unused{$_}++ unless $used{$_};
}

print $_, "\n" foreach sort keys %unused;
