#!/usr/bin/perl -w
#
# pod-parser.t -- Tests for backward compatibility with Pod::Parser.
#
# Copyright 2006, 2008, 2009 by Russ Allbery <rra@stanford.edu>
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
BEGIN {
    use_ok ('Pod::Man');
    use_ok ('Pod::Text');
}

my $parser = Pod::Man->new;
isa_ok ($parser, 'Pod::Man', 'Pod::Man parser object');
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
is ($output, "Some random \\fBtext\\fR.\n", 'Pod::Man -cutting output');

$parser = Pod::Text->new;
isa_ok ($parser, 'Pod::Text', 'Pod::Text parser object');
open (OUT, '> out.tmp') or die "Cannot create out.tmp: $!\n";
$parser->parse_from_file ({ -cutting => 0 }, 'tmp.pod', \*OUT);
close OUT;
open (OUT, 'out.tmp') or die "Cannot open out.tmp: $!\n";
{
    local $/;
    $output = <OUT>;
}
close OUT;
is ($output, "    Some random text.\n\n", 'Pod::Text -cutting output');

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
is ($output, "    Some random text.\n\n", 'Pod::Text pod2text function');

1 while unlink ('tmp.pod', 'out.tmp');
exit 0;
