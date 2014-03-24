#!/usr/bin/perl -w
#
# overstrike.t -- Additional specialized tests for Pod::Text::Overstrike.
#
# Copyright 2002, 2004, 2006, 2009, 2012, 2013
#     Russ Allbery <rra@stanford.edu>
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

use Test::More tests => 4;
BEGIN { use_ok ('Pod::Text::Overstrike') }

my $parser = Pod::Text::Overstrike->new;
isa_ok ($parser, 'Pod::Text::Overstrike', 'Parser module');
my $n = 1;
while (<DATA>) {
    next until $_ eq "###\n";
    open (TMP, "> tmp$$.pod") or die "Cannot create tmp$$.pod: $!\n";
    while (<DATA>) {
        last if $_ eq "###\n";
        print TMP $_;
    }
    close TMP;
    open (OUT, "> out$$.tmp") or die "Cannot create out$$.tmp: $!\n";
    $parser->parse_from_file ("tmp$$.pod", \*OUT);
    close OUT;
    open (TMP, "out$$.tmp") or die "Cannot open out$$.tmp: $!\n";
    my $output;
    {
        local $/;
        $output = <TMP>;
    }
    close TMP;
    1 while unlink ("tmp$$.pod", "out$$.tmp");
    my $expected = '';
    while (<DATA>) {
        last if $_ eq "###\n";
        $expected .= $_;
    }
    is ($output, $expected, "Output correct for test $n");
    $n++;
}

# Below the marker are bits of POD and corresponding expected output.  This is
# used to test specific features or problems with Pod::Text::Termcap.  The
# input and output are separated by lines containing only ###.

__DATA__

###
=head1 WRAPPING

B<I<Do>> I<B<not>> B<I<include>> B<I<formatting codes when>> B<I<wrapping>>.
###
WWRRAAPPPPIINNGG
    DDoo _n_o_t iinncclluuddee ffoorrmmaattttiinngg  ccooddeess  wwhheenn wwrraappppiinngg.

###

###
=head1 TAG WIDTH

=over 10

=item 12345678

A

=item B<12345678>

B

=item 1Z<>

C

=item B<1>

D

=back
###
TTAAGG  WWIIDDTTHH
    12345678  A

    1122334455667788  B

    1         C

    11         D

###
