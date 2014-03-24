#!/usr/bin/perl -w
#
# filehandle.t -- Test the parse_from_filehandle interface.
#
# Copyright 2006, 2009, 2012 by Russ Allbery <rra@stanford.edu>
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

use Test::More tests => 6;

BEGIN {
    use_ok ('Pod::Man');
    use_ok ('Pod::Text');
}

my $man = Pod::Man->new;
isa_ok ($man, 'Pod::Man', 'Pod::Man parser object');
my $text = Pod::Text->new;
isa_ok ($text, 'Pod::Text', 'Pod::Text parser object');
while (<DATA>) {
    next until $_ eq "###\n";
    open (TMP, "> tmp$$.pod") or die "Cannot create tmp.pod: $!\n";
    while (<DATA>) {
        last if $_ eq "###\n";
        print TMP $_;
    }
    close TMP;

    # Test Pod::Man output.
    open (IN, "< tmp$$.pod") or die "Cannot open tmp$$.pod: $!\n";
    open (OUT, "> out$$.tmp") or die "Cannot create out$$.tmp: $!\n";
    $man->parse_from_filehandle (\*IN, \*OUT);
    close IN;
    close OUT;
    open (OUT, "out$$.tmp") or die "Cannot open out$$.tmp: $!\n";
    while (<OUT>) { last if /^\.nh/ }
    my $output;
    {
        local $/;
        $output = <OUT>;
    }
    close OUT;
    my $expected = '';
    while (<DATA>) {
        last if $_ eq "###\n";
        $expected .= $_;
    }
    is ($output, $expected, 'Pod::Man output is correct');

    # Test Pod::Text output.
    open (IN, "< tmp$$.pod") or die "Cannot open tmp$$.pod: $!\n";
    open (OUT, "> out$$.tmp") or die "Cannot create out$$.tmp: $!\n";
    $text->parse_from_filehandle (\*IN, \*OUT);
    close IN;
    close OUT;
    open (OUT, "out$$.tmp") or die "Cannot open out$$.tmp: $!\n";
    {
        local $/;
        $output = <OUT>;
    }
    close OUT;
    1 while unlink ("tmp$$.pod", "out$$.tmp");
    $expected = '';
    while (<DATA>) {
        last if $_ eq "###\n";
        $expected .= $_;
    }
    is ($output, $expected, 'Pod::Text output is correct');
}

# Below the marker are bits of POD, corresponding expected nroff output, and
# corresponding expected text output.  The input and output are separated by
# lines containing only ###.

__DATA__

###
=head1 NAME

gcc - GNU project C and C++ compiler

=head1 C++ NOTES

Other mentions of C++.
###
.SH "NAME"
gcc \- GNU project C and C++ compiler
.SH "\*(C+ NOTES"
.IX Header " NOTES"
Other mentions of \*(C+.
###
NAME
    gcc - GNU project C and C++ compiler

C++ NOTES
    Other mentions of C++.

###
