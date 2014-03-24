#!/usr/bin/perl
#
# Test setting color aliases via the function interface.
#
# Copyright 2012 Russ Allbery <rra@stanford.edu>
#
# This program is free software; you may redistribute it and/or modify it
# under the same terms as Perl itself.

use strict;
use warnings;

use Test::More tests => 23;

# Load the module.
BEGIN {
    delete $ENV{ANSI_COLORS_ALIASES};
    delete $ENV{ANSI_COLORS_DISABLED};
    use_ok('Term::ANSIColor', qw(color colored colorvalid uncolor coloralias));
}

# Confirm our test alias doesn't exist.
my $output = eval { color('alert') };
ok(!$output, 'alert color not recognized');
like(
    $@,
    qr{ \A Invalid [ ] attribute [ ] name [ ] alert [ ] at [ ] }xms,
    '...with the right error'
);

# Basic alias functionality.
is(coloralias('alert', 'red'), 'red', 'coloralias works and returns color');
is(color('alert'), color('red'), 'alert now works as a color');
is(colored('test', 'alert'), "\e[31mtest\e[0m", '..and colored works');
ok(colorvalid('alert'), '...and alert is now a valid color');
is(coloralias('alert'), 'red', 'coloralias with one arg returns value');

# The alias can be changed.
is(coloralias('alert', 'green'), 'green', 'changing the alias works');
is(coloralias('alert'), 'green',        '...and changed the mapping');
is(color('alert'),      color('green'), '...and now returns its new value');

# uncolor ignores aliases.
is_deeply([uncolor("\e[32m")], ['green'], 'uncolor ignores aliases');

# Asking for the value of an unknown alias returns undef.
is(coloralias('warning'), undef, 'coloralias on unknown alias returns undef');

# Invalid alias names.
$output = eval { coloralias('foo;bar', 'green') };
ok(!$output, 'invalid alias name rejected');
like(
    $@,
    qr{ \A Invalid [ ] alias [ ] name [ ] "foo;bar" [ ] at [ ] }xms,
    '...with the right error'
);
$output = eval { coloralias(q{}, 'green') };
ok(!$output, 'empty alias name rejected');
like(
    $@,
    qr{ \A Invalid [ ] alias [ ] name [ ] "" [ ] at [ ] }xms,
    '...with the right error'
);

# Aliasing an existing color.
$output = eval { coloralias('red', 'green') };
ok(!$output, 'aliasing an existing color rejected');
like(
    $@,
    qr{ \A Cannot [ ] alias [ ] standard [ ] color [ ] "red" [ ] at [ ] }xms,
    '...with the right error'
);

# Aliasing to a color that doesn't exist, or to another alias.
$output = eval { coloralias('warning', 'chartreuse') };
ok(!$output, 'aliasing to an unknown color rejected');
like(
    $@,
    qr{ \A Invalid [ ] attribute [ ] name [ ] "chartreuse" [ ] at [ ] }xms,
    '...with the right error'
);
$output = eval { coloralias('warning', 'alert') };
ok(!$output, 'aliasing to an alias rejected');
like(
    $@,
    qr{ \A Invalid [ ] attribute [ ] name [ ] "alert" [ ] at [ ] }xms,
    '...with the right error'
);
