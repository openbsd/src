#!/usr/bin/perl -Tw
#
# t/basic.t -- Test suite for the Term::ANSIColor Perl module.
#
# Copyright 1997, 1998, 2000, 2001, 2002, 2005, 2006, 2009
#     Russ Allbery <rra@stanford.edu>
#
# This program is free software; you may redistribute it and/or modify it
# under the same terms as Perl itself.

use strict;
use Test::More tests => 47;

BEGIN {
    delete $ENV{ANSI_COLORS_DISABLED};
    use_ok ('Term::ANSIColor',
            qw/:pushpop color colored uncolor colorstrip colorvalid/);
}

# Various basic tests.
is (color ('blue on_green', 'bold'), "\e[34;42;1m", 'Simple attributes');
is (colored ('testing', 'blue', 'bold'), "\e[34;1mtesting\e[0m", 'colored');
is ((BLUE BOLD "testing"), "\e[34m\e[1mtesting", 'Constants');
$Term::ANSIColor::AUTORESET = 1;
is ((BLUE BOLD "testing"), "\e[34m\e[1mtesting\e[0m\e[0m", 'AUTORESET');
$Term::ANSIColor::EACHLINE = "\n";
is (colored ("test\n\ntest", 'bold'), "\e[1mtest\e[0m\n\n\e[1mtest\e[0m",
    'EACHLINE');
$Term::ANSIColor::EACHLINE = "\r\n";
is (colored ("test\ntest\r\r\n\r\n", 'bold'),
    "\e[1mtest\ntest\r\e[0m\r\n\r\n",
    'EACHLINE with multiple delimiters');
$Term::ANSIColor::EACHLINE = "\n";
is (colored (['bold', 'on_green'], "test\n", "\n", "test"),
    "\e[1;42mtest\e[0m\n\n\e[1;42mtest\e[0m",
    'colored with reference to array');
is_deeply ([ uncolor ('1;42', "\e[m", '', "\e[0m") ],
           [ qw/bold on_green clear/ ], 'uncolor');

# Several tests for ANSI_COLORS_DISABLED.
$ENV{ANSI_COLORS_DISABLED} = 1;
is (color ('blue'), '', 'color support for ANSI_COLORS_DISABLED');
is (colored ('testing', 'blue', 'on_red'), 'testing',
    'colored support for ANSI_COLORS_DISABLED');
is ((GREEN 'testing'), 'testing', 'Constant support for ANSI_COLORS_DISABLED');
delete $ENV{ANSI_COLORS_DISABLED};

# Make sure DARK is exported.  This was omitted in versions prior to 1.07.
is ((DARK "testing"), "\e[2mtesting\e[0m", 'DARK');

# Check faint as a synonym for dark.
is (colored ('test', 'faint'), "\e[2mtest\e[0m", 'colored supports faint');
is ((FAINT "test"), "\e[2mtest\e[0m", '...and the FAINT constant works');

# Test colored with 0 and EACHLINE.
$Term::ANSIColor::EACHLINE = "\n";
is (colored ('0', 'blue', 'bold'), "\e[34;1m0\e[0m",
    'colored with 0 and EACHLINE');
is (colored ("0\n0\n\n", 'blue', 'bold'), "\e[34;1m0\e[0m\n\e[34;1m0\e[0m\n\n",
    'colored with 0, EACHLINE, and multiple lines');

# Test colored with the empty string and EACHLINE.
is (colored ('', 'blue', 'bold'), '',
    'colored with an empty string and EACHLINE');

# Test push and pop support.
$Term::ANSIColor::AUTORESET = 0;
is ((PUSHCOLOR RED ON_GREEN "text"), "\e[31m\e[42mtext",
    'PUSHCOLOR does not break constants');
is ((PUSHCOLOR BLUE "text"), "\e[34mtext", '...and adding another level');
is ((RESET BLUE "text"), "\e[0m\e[34mtext", '...and using reset');
is ((POPCOLOR "text"), "\e[31m\e[42mtext", '...and POPCOLOR works');
is ((LOCALCOLOR GREEN ON_BLUE "text"), "\e[32m\e[44mtext\e[31m\e[42m",
    'LOCALCOLOR');
$Term::ANSIColor::AUTOLOCAL = 1;
is ((ON_BLUE "text"), "\e[44mtext\e[31m\e[42m", 'AUTOLOCAL');
$Term::ANSIColor::AUTOLOCAL = 0;
is ((POPCOLOR "text"), "\e[0mtext", 'POPCOLOR with empty stack');

# Test push and pop support with the syntax from the original openmethods.com
# submission, which uses a different coding style.
is (PUSHCOLOR (RED ON_GREEN), "\e[31m\e[42m",
    'PUSHCOLOR with explict argument');
is (PUSHCOLOR (BLUE), "\e[34m", '...and another explicit argument');
is (RESET . BLUE . "text", "\e[0m\e[34mtext",
    '...and constants with concatenation');
is (POPCOLOR . "text", "\e[31m\e[42mtext",
    '...and POPCOLOR works without an argument');
is (LOCALCOLOR(GREEN . ON_BLUE . "text"), "\e[32m\e[44mtext\e[31m\e[42m",
    'LOCALCOLOR with two arguments');
is (POPCOLOR . "text", "\e[0mtext", 'POPCOLOR with no arguments');

# Test colorstrip.
is (colorstrip ("\e[1mBold \e[31;42mon green\e[0m\e[m"), 'Bold on green',
    'Basic color stripping');
is (colorstrip ("\e[1m", 'bold', "\e[0m"), 'bold',
    'Color stripping across multiple strings');
is_deeply ([ colorstrip ("\e[1m", 'bold', "\e[0m") ],
           [ '', 'bold', '' ], '...and in an array context');
is (colorstrip ("\e[2cSome other code\e and stray [0m stuff"),
    "\e[2cSome other code\e and stray [0m stuff",
    'colorstrip does not remove non-color stuff');

# Test colorvalid.
is (colorvalid ("blue bold dark", "blink on_green"), 1,
    'colorvalid returns true for valid attributes');
is (colorvalid ("green orange"), undef,
    '...and false for invalid attributes');

# Test error handling.
my $output = eval { color 'chartreuse' };
is ($output, undef, 'color on unknown color name fails');
like ($@, qr/^Invalid attribute name chartreuse at /,
      '...with the right error');
$output = eval { colored "Stuff", 'chartreuse' };
is ($output, undef, 'colored on unknown color name fails');
like ($@, qr/^Invalid attribute name chartreuse at /,
      '...with the right error');
$output = eval { uncolor "\e[28m" };
is ($output, undef, 'uncolor on unknown color code fails');
like ($@, qr/^No name for escape sequence 28 at /, '...with the right error');
$output = eval { uncolor "\e[foom" };
is ($output, undef, 'uncolor on bad escape sequence fails');
like ($@, qr/^Bad escape sequence foo at /, '...with the right error');

# Test error reporting when calling unrecognized Term::ANSIColor subs that go
# through AUTOLOAD.
eval { Term::ANSIColor::RSET () };
like ($@, qr/^undefined subroutine \&Term::ANSIColor::RSET called at /,
      'Correct error from an attribute that is not defined');
eval { Term::ANSIColor::reset () };
like ($@, qr/^undefined subroutine \&Term::ANSIColor::reset called at /,
      'Correct error from a lowercase attribute');
