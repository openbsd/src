#!/usr/bin/perl
#
# Test Pod::Man ISO-8859-1 handling
#
# Copyright 2016 Russ Allbery <rra@cpan.org>
#
# This program is free software; you may redistribute it and/or modify it
# under the same terms as Perl itself.

use 5.006;
use strict;
use warnings;

use lib 't/lib';

use Test::More tests => 3;
use Test::Podlators qw(test_snippet);

# Load the module.
BEGIN {
    use_ok('Pod::Man');
}

# Test the snippet with the proper encoding.
test_snippet('Pod::Man', 'man/iso-8859-1', { encoding => 'iso-8859-1' });
