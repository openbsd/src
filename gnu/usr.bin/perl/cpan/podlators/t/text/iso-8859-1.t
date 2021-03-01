#!/usr/bin/perl
#
# Test Pod::Text ISO-8859-1 handling
#
# Copyright 2016, 2019 Russ Allbery <rra@cpan.org>
#
# This program is free software; you may redistribute it and/or modify it
# under the same terms as Perl itself.
#
# SPDX-License-Identifier: GPL-1.0-or-later OR Artistic-1.0-Perl

use 5.008;
use strict;
use warnings;

use lib 't/lib';

use Test::More tests => 3;
use Test::Podlators qw(test_snippet);

# Load the module.
BEGIN {
    use_ok('Pod::Text');
}

# Test the snippet with the proper encoding.
test_snippet('Pod::Text', 'text/iso-8859-1', { encoding => 'iso-8859-1' });
