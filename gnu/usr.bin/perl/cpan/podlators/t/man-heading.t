#!/usr/bin/perl -w
#
# man-options.t -- Additional tests for Pod::Man options.
#
# Copyright 2002, 2004, 2006, 2008, 2009, 2012 Russ Allbery <rra@stanford.edu>
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

use Test::More tests => 7;
BEGIN { use_ok ('Pod::Man') }

my $n = 1;
while (<DATA>) {
    my %options;
    next until $_ eq "###\n";
    while (<DATA>) {
        last if $_ eq "###\n";
        my ($option, $value) = split (' ', $_, 2);
        chomp $value;
        $options{$option} = $value;
    }
    open (TMP, '> tmp.pod') or die "Cannot create tmp.pod: $!\n";
    print TMP "=head1 NAME\n\ntest - Test man page\n";
    close TMP;
    my $parser = Pod::Man->new (%options);
    isa_ok ($parser, 'Pod::Man', 'Parser object');
    open (OUT, "> out$$.tmp") or die "Cannot create out$$.tmp: $!\n";
    $parser->parse_from_file ('tmp.pod', \*OUT);
    close OUT;
    open (TMP, "out$$.tmp") or die "Cannot open out$$.tmp: $!\n";
    my $heading;
    while (<TMP>) {
        if (/^\.TH/) {
            $heading = $_;
            last;
        }
    }
    close TMP;
    1 while unlink ('tmp.pod', "out$$.tmp");
    my $expected = '';
    while (<DATA>) {
        last if $_ eq "###\n";
        $expected .= $_;
    }
    is ($heading, $expected, "Heading is correct for test $n");
    $n++;
}

# Below the marker are sets of options and the corresponding expected .TH line
# from the man page.  This is used to test specific features or problems with
# Pod::Man.  The options and output are separated by lines containing only
# ###.

__DATA__

###
date 2009-01-17
release 1.0
###
.TH TMP 1 "2009-01-17" "1.0" "User Contributed Perl Documentation"
###

###
date 2009-01-17
name TEST
section 8
release 2.0-beta
###
.TH TEST 8 "2009-01-17" "2.0-beta" "User Contributed Perl Documentation"
###

###
date 2009-01-17
release 1.0
center Testing Documentation
###
.TH TMP 1 "2009-01-17" "1.0" "Testing Documentation"
###
