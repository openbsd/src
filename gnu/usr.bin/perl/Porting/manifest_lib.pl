#!/usr/bin/perl

use strict;

=head1 NAME

Porting/manifest_lib.pl - functions for managing manifests

=head1 SYNOPSIS

    require './Porting/manifest_lib.pl';

=head1 DESCRIPTION

This file makes available one function, C<sort_manifest()>.

=head2 C<sort_manifest>

Treats its arguments as (chomped) lines from a MANIFEST file, and returns that
listed sorted appropriately.

=cut

# Try to get a sane sort. case insensitive, more or less
# sorted such that path components are compared independently,
# and so that lib/Foo/Bar sorts before lib/Foo-Alpha/Baz
# and so that lib/Foo/Bar.pm sorts before lib/Foo/Bar/Alpha.pm
# and so that configure and Configure sort together.
sub sort_manifest {
    return
    # case insensitive sorting of directory components independently.
    map { $_->[0] } # extract the full line
    sort {
        $a->[1] cmp $b->[1] || # sort in order of munged filename
        $a->[0] cmp $b->[0]    # then by the exact text in full line
    }
    map {
        # split out the filename and the description
        my ($f) = split /\s+/, $_, 2;
        # lc the filename so Configure and configure sort together in the list
        my $m= lc $f; # $m for munged
        # replace slashes by nulls, this makes short directory names sort before
        # longer ones, such as "foo/" sorting before "foo-bar/"
        $m =~ s!/!\0!g;
        # replace the extension (only one) by null null extension.
        # this puts any foo/blah.ext before any files in foo/blah/
        $m =~ s{(?<!\A)(\.[^.]+\z)}{\0\0$1};

        # return the original string, and the munged filename
        [ $_, $m ];
    } @_;
}

1;

# ex: set ts=8 sts=4 sw=4 et:
