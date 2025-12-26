#!/usr/bin/perl
#
# Test Pod::Text::Color behavior with various snippets.
#
# Copyright 2002, 2004, 2006, 2009, 2012-2013, 2018-2019, 2023-2024
#     Russ Allbery <rra@cpan.org>
#
# This program is free software; you may redistribute it and/or modify it
# under the same terms as Perl itself.

use 5.012;
use warnings;

use lib 't/lib';

use Test::More tests => 13;
use Test::Podlators qw(test_snippet);

# Load the module.
BEGIN {
    use_ok('Pod::Text::Color');
}

# Ensure color is enabled for testing purposes.
delete $ENV{NO_COLOR};

# List of snippets run by this test.
my @snippets = qw(
    escape-wrapping nonbreaking-wrap tag-width tag-wrapping width wrapping
);

# Run all the tests.
for my $snippet (@snippets) {
    test_snippet('Pod::Text::Color', "color/$snippet");
}
