#!/usr/bin/perl
#
# t/pod-spelling.t -- Test POD spelling.
#
# Copyright 2008 Russ Allbery <rra@stanford.edu>
#
# This program is free software; you may redistribute it and/or modify it
# under the same terms as Perl itself.

# Called to skip all tests with a reason.
sub skip_all {
    print "1..0 # Skipped: @_\n";
    exit;
}

# Skip all spelling tests unless flagged to run maintainer tests.
skip_all "Spelling tests only run for maintainer"
    unless $ENV{RRA_MAINTAINER_TESTS};

# Make sure we have prerequisites.  hunspell is currently not supported due to
# lack of support for contractions.
eval 'use Test::Pod 1.00';
skip_all "Test::Pod 1.00 required for testing POD" if $@;
eval 'use Pod::Spell';
skip_all "Pod::Spell required to test POD spelling" if $@;
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
skip_all "aspell or ispell required to test POD spelling" unless @spell;

# Run the test, one for each POD file.
$| = 1;
my @pod = all_pod_files ();
my $count = scalar @pod;
print "1..$count\n";
my $n = 1;
for my $pod (@pod) {
    my $child = open (CHILD, '-|');
    if (not defined $child) {
        die "Cannot fork: $!\n";
    } elsif ($child == 0) {
        my $pid = open (SPELL, '|-', @spell)
            or die "Cannot run @spell: $!\n";
        open (POD, '<', $pod) or die "Cannot open $pod: $!\n";
        my $parser = Pod::Spell->new;
        $parser->parse_from_filehandle (\*POD, \*SPELL);
        close POD;
        close SPELL;
        exit ($? >> 8);
    } else {
        my @words = <CHILD>;
        close CHILD;
        if ($? != 0) {
            print "ok $n # skip - @spell failed: $?\n";
        } elsif (@words) {
            for (@words) {
                s/^\s+//;
                s/\s+$//;
            }
            print "not ok $n\n";
            print " - Misspelled words found in $pod\n";
            print "   @words\n";
        } else {
            print "ok $n\n";
        }
        $n++;
    }
}
