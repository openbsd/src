#!/usr/bin/perl -w
#
# pod-parser.t -- Tests for backward compatibility with Pod::Parser.
#
# Copyright 2006, 2008 by Russ Allbery <rra@stanford.edu>
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
    print "1..4\n";
}

my $loaded;

END {
    print "not ok 1\n" unless $loaded;
}

use Pod::Man;
use Pod::Text;
use strict;

$loaded = 1;
print "ok 1\n";

my $parser = Pod::Man->new or die "Cannot create parser\n";
open (TMP, '> tmp.pod') or die "Cannot create tmp.pod: $!\n";
print TMP "Some random B<text>.\n";
close TMP;
open (OUT, '> out.tmp') or die "Cannot create out.tmp: $!\n";
$parser->parse_from_file ({ -cutting => 0 }, 'tmp.pod', \*OUT);
close OUT;
open (OUT, 'out.tmp') or die "Cannot open out.tmp: $!\n";
while (<OUT>) { last if /^\.nh/ }
my $output;
{
    local $/;
    $output = <OUT>;
}
close OUT;
if ($output eq "Some random \\fBtext\\fR.\n") {
    print "ok 2\n";
} else {
    print "not ok 2\n";
    print "Expected\n========\nSome random \\fBtext\\fR.\n\n";
    print "Output\n======\n$output\n";
}

$parser = Pod::Text->new or die "Cannot create parser\n";
open (OUT, '> out.tmp') or die "Cannot create out.tmp: $!\n";
$parser->parse_from_file ({ -cutting => 0 }, 'tmp.pod', \*OUT);
close OUT;
open (OUT, 'out.tmp') or die "Cannot open out.tmp: $!\n";
{
    local $/;
    $output = <OUT>;
}
close OUT;
if ($output eq "    Some random text.\n\n") {
    print "ok 3\n";
} else {
    print "not ok 3\n";
    print "Expected\n========\n    Some random text.\n\n\n";
    print "Output\n======\n$output\n";
}

# Test the pod2text function, particularly with only one argument.
open (TMP, '> tmp.pod') or die "Cannot create tmp.pod: $!\n";
print TMP "=pod\n\nSome random B<text>.\n";
close TMP;
open (OUT, '> out.tmp') or die "Cannot create out.tmp: $!\n";
open (SAVE, '>&STDOUT') or die "Cannot dup stdout: $!\n";
open (STDOUT, '>&OUT') or die "Cannot replace stdout: $!\n";
pod2text ('tmp.pod');
close OUT;
open (STDOUT, '>&SAVE') or die "Cannot fix stdout: $!\n";
close SAVE;
open (OUT, 'out.tmp') or die "Cannot open out.tmp: $!\n";
{
    local $/;
    $output = <OUT>;
}
close OUT;
if ($output eq "    Some random text.\n\n") {
    print "ok 4\n";
} else {
    print "not ok 4\n";
    print "Expected\n========\n    Some random text.\n\n\n";
    print "Output\n======\n$output\n";
}

unlink ('tmp.pod', 'out.tmp');
exit 0;
