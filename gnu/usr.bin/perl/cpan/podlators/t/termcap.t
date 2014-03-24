#!/usr/bin/perl -w
#
# termcap.t -- Additional specialized tests for Pod::Text::Termcap.
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
BEGIN { use_ok ('Pod::Text::Termcap') }

# Hard-code a few values to try to get reproducible results.
$ENV{COLUMNS} = 80;
$ENV{TERM} = 'xterm';
$ENV{TERMCAP} = 'xterm:co=80:do=^J:md=\E[1m:us=\E[4m:me=\E[m';

my $parser = Pod::Text::Termcap->new;
isa_ok ($parser, 'Pod::Text::Termcap', 'Parser module');
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
[1mWRAPPING[m
    [1m[4mDo[m[m [4m[1mnot[m[m [1m[4minclude[m[m [1m[4mformatting codes when[m[m [1m[4mwrapping[m[m.

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
[1mTAG WIDTH[m
    12345678  A

    [1m12345678[m  B

    1         C

    [1m1[m         D

###
