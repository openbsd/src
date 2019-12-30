#!/usr/bin/perl
#
# Test Pod::Text::Termcap behavior with various snippets.
#
# Copyright 2002, 2004, 2006, 2009, 2012-2014, 2018
#     Russ Allbery <rra@cpan.org>
#
# This program is free software; you may redistribute it and/or modify it
# under the same terms as Perl itself.

use 5.006;
use strict;
use warnings;

use lib 't/lib';

use Test::More tests => 11;
use Test::Podlators qw(test_snippet);

# Load the module.
BEGIN {
    use_ok('Pod::Text::Termcap');
}

# Hard-code a few values to try to get reproducible results.
$ENV{COLUMNS}  = 80;
$ENV{TERM}     = 'xterm';
$ENV{TERMPATH} = File::Spec->catfile('t', 'data', 'termcap');
$ENV{TERMCAP}  = 'xterm:co=#80:do=^J:md=\E[1m:us=\E[4m:me=\E[m';

# List of snippets run by this test.
my @snippets = qw(escape-wrapping tag-width tag-wrapping width wrapping);

# Run all the tests.
for my $snippet (@snippets) {
    test_snippet('Pod::Text::Termcap', "termcap/$snippet");
}
