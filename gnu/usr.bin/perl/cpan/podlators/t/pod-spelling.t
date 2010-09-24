#!/usr/bin/perl -w
#
# Check for spelling errors in POD documentation
#
# Checks all POD files in the tree for spelling problems using Pod::Spell and
# either aspell or ispell.  aspell is preferred.  This test is disabled unless
# RRA_MAINTAINER_TESTS is set, since spelling dictionaries vary too much
# between environments.
#
# Copyright 2008, 2009 Russ Allbery <rra@stanford.edu>
#
# This program is free software; you may redistribute it and/or modify it
# under the same terms as Perl itself.

use strict;
use Test::More;

# Skip all spelling tests unless the maintainer environment variable is set.
plan skip_all => 'Spelling tests only run for maintainer'
    unless $ENV{RRA_MAINTAINER_TESTS};

# Load required Perl modules.
eval 'use Test::Pod 1.00';
plan skip_all => 'Test::Pod 1.00 required for testing POD' if $@;
eval 'use Pod::Spell';
plan skip_all => 'Pod::Spell required to test POD spelling' if $@;

# Locate a spell-checker.  hunspell is not currently supported due to its lack
# of support for contractions (at least in the version in Debian).
my @spell;
my %options = (aspell => [ qw(-d en_US --home-dir=./ list) ],
               ispell => [ qw(-d american -l -p /dev/null) ]);
SEARCH: for my $program (qw/aspell ispell/) {
    for my $dir (split ':', $ENV{PATH}) {
        if (-x "$dir/$program") {
            @spell = ("$dir/$program", @{ $options{$program} });
        }
        last SEARCH if @spell;
    }
}
plan skip_all => 'aspell or ispell required to test POD spelling'
    unless @spell;

# Prerequisites are satisfied, so we're going to do some testing.  Figure out
# what POD files we have and from that develop our plan.
$| = 1;
my @pod = all_pod_files ();
plan tests => scalar @pod;

# Finally, do the checks.
for my $pod (@pod) {
    my $child = open (CHILD, '-|');
    if (not defined $child) {
        die "Cannot fork: $!\n";
    } elsif ($child == 0) {
        my $pid = open (SPELL, '|-', @spell) or die "Cannot run @spell: $!\n";
        open (POD, '<', $pod) or die "Cannot open $pod: $!\n";
        my $parser = Pod::Spell->new;
        $parser->parse_from_filehandle (\*POD, \*SPELL);
        close POD;
        close SPELL;
        exit ($? >> 8);
    } else {
        my @words = <CHILD>;
        close CHILD;
      SKIP: {
            skip "@spell failed for $pod", 1 unless $? == 0;
            for (@words) {
                s/^\s+//;
                s/\s+$//;
            }
            is ("@words", '', $pod);
        }
    }
}
