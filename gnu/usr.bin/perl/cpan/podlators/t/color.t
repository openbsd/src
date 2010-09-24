#!/usr/bin/perl -w
#
# color.t -- Additional specialized tests for Pod::Text::Color.
#
# Copyright 2002, 2004, 2006, 2009 by Russ Allbery <rra@stanford.edu>
#
# This program is free software; you may redistribute it and/or modify it
# under the same terms as Perl itself.

BEGIN {
    chdir 't' if -d 't';
    if ($ENV{PERL_CORE}) {
        @INC = '../lib';
    }
    unshift (@INC, '../blib/lib');
    $| = 1;
}

use strict;

use Test::More;

# Skip this test if Term::ANSIColor isn't available.
eval { require Term::ANSIColor };
if ($@) {
    plan skip_all => 'Term::ANSIColor required for Pod::Text::Color';
} else {
    plan tests => 4;
}
require_ok ('Pod::Text::Color');

# Load tests from the data section below, write the POD to a temporary file,
# convert it, and compare to the expected output.
my $parser = Pod::Text::Color->new;
isa_ok ($parser, 'Pod::Text::Color', 'Parser object');
my $n = 1;
while (<DATA>) {
    next until $_ eq "###\n";
    open (TMP, '> tmp.pod') or die "Cannot create tmp.pod: $!\n";
    while (<DATA>) {
        last if $_ eq "###\n";
        print TMP $_;
    }
    close TMP;
    open (OUT, '> out.tmp') or die "Cannot create out.tmp: $!\n";
    $parser->parse_from_file ('tmp.pod', \*OUT);
    close OUT;
    open (TMP, 'out.tmp') or die "Cannot open out.tmp: $!\n";
    my $output;
    {
        local $/;
        $output = <TMP>;
    }
    close TMP;
    1 while unlink ('tmp.pod', 'out.tmp');
    my $expected = '';
    while (<DATA>) {
        last if $_ eq "###\n";
        $expected .= $_;
    }
    is ($output, $expected, "Output correct for test $n");
    $n++;
}

# Below the marker are bits of POD and corresponding expected output.  This is
# used to test specific features or problems with Pod::Text::Color.  The input
# and output are separated by lines containing only ###.

__DATA__

###
=head1 WRAPPING

B<I<Do>> I<B<not>> B<I<include>> B<I<formatting codes when>> B<I<wrapping>>.
###
[1mWRAPPING[0m
    [1m[33mDo[0m[0m [33m[1mnot[0m[0m [1m[33minclude[0m[0m [1m[33mformatting codes when[0m[0m [1m[33mwrapping[0m[0m.

###

###
=head1 TAG WIDTH

=over 10

=item 12345678

A

=item B<12345678>

B

=item 1

C

=item B<1>

D

=back
###
[1mTAG WIDTH[0m
    12345678  A

    [1m12345678[0m  B

    1         C

    [1m1[0m         D

###
