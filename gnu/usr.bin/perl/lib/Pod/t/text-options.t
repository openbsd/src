#!/usr/bin/perl -w
# $Id: text-options.t,v 1.1.1.1 2003/12/03 02:44:08 millert Exp $
#
# text-options.t -- Additional tests for Pod::Text options.
#
# Copyright 2002 by Russ Allbery <rra@stanford.edu>
#
# This program is free software; you may redistribute it and/or modify it
# under the same terms as Perl itself.

BEGIN {
    chdir 't' if -d 't';
    if ($ENV{PERL_CORE}) {
        @INC = '../lib';
    } else {
        unshift (@INC, '../blib/lib');
    }
    unshift (@INC, '../blib/lib');
    $| = 1;
    print "1..3\n";
}

END {
    print "not ok 1\n" unless $loaded;
}

use Pod::Text;

$loaded = 1;
print "ok 1\n";

my $n = 2;
while (<DATA>) {
    my %options;
    next until $_ eq "###\n";
    while (<DATA>) {
        last if $_ eq "###\n";
        my ($option, $value) = split;
        $options{$option} = $value;
    }
    open (TMP, '> tmp.pod') or die "Cannot create tmp.pod: $!\n";
    while (<DATA>) {
        last if $_ eq "###\n";
        print TMP $_;
    }
    close TMP;
    my $parser = Pod::Text->new (%options) or die "Cannot create parser\n";
    $parser->parse_from_file ('tmp.pod', 'out.tmp');
    open (TMP, 'out.tmp') or die "Cannot open out.tmp: $!\n";
    my $output;
    {
        local $/;
        $output = <TMP>;
    }
    close TMP;
    unlink ('tmp.pod', 'out.tmp');
    my $expected = '';
    while (<DATA>) {
        last if $_ eq "###\n";
        $expected .= $_;
    }
    if ($output eq $expected) {
        print "ok $n\n";
    } else {
        print "not ok $n\n";
        print "Expected\n========\n$expected\nOutput\n======\n$output\n";
    }
    $n++;
}

# Below the marker are bits of POD and corresponding expected text output.
# This is used to test specific features or problems with Pod::Text.  The
# input and output are separated by lines containing only ###.

__DATA__

###
alt 1
###
=head1 SAMPLE

=over 4

=item F

Paragraph.

=item Bar

=item B

Paragraph.

=item Longer

Paragraph.

=back

###

==== SAMPLE ====

:   F   Paragraph.

:   Bar
:   B   Paragraph.

:   Longer
        Paragraph.

###

###
margin 4
###
=head1 SAMPLE

This is some body text that is long enough to be a paragraph that wraps,
thereby testing margins with wrapped paragraphs.

 This is some verbatim text.

=over 6

=item Test

This is a test of an indented paragraph.

This is another indented paragraph.

=back
###
    SAMPLE
        This is some body text that is long enough to be a paragraph that
        wraps, thereby testing margins with wrapped paragraphs.

         This is some verbatim text.

        Test  This is a test of an indented paragraph.

              This is another indented paragraph.

###
