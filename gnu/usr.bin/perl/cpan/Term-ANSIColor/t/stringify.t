#!/usr/bin/perl -Tw
#
# t/stringify.t -- Test suite for stringify interaction.
#
# Copyright 2011 Revilo Reegiles
# Copyright 2011 Russ Allbery <rra@stanford.edu>
#
# This program is free software; you may redistribute it and/or modify it
# under the same terms as Perl itself.

# Create a dummy class that implements stringification.
package Test::Stringify;
use overload '""' => 'stringify';
sub new { return bless {} }
sub stringify { return "Foo Bar\n" }
package main;

use strict;
use Test::More tests => 6;

BEGIN {
    delete $ENV{ANSI_COLORS_DISABLED};
    use_ok ('Term::ANSIColor',
            qw/:pushpop color colored uncolor colorstrip colorvalid/);
}

is (colored ([ 'blue', 'bold' ], 'testing'), "\e[34;1mtesting\e[0m",
    'colored with an array reference');
is (colored ("ok\n", 'bold blue'), "\e[1;34mok\n\e[0m",
    'colored with a following string');
my $test = Test::Stringify->new;
is (colored ($test . "", 'bold blue'), "\e[1;34mFoo Bar\n\e[0m",
    'colored with forced stringification');
is (colored ($test, 'bold blue'), "\e[1;34mFoo Bar\n\e[0m",
    'colored with a non-array reference');
my %foo = (foo => 'bar');
like (colored (\%foo, 'bold blue'), qr/\e\[1;34mHASH\(.*\)\e\[0m/,
      'colored with a hash reference');
