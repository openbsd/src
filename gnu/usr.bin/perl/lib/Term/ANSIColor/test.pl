#!/usr/bin/perl
# $Id: test.pl,v 1.5 2005/08/21 18:31:58 eagle Exp $
#
# test.pl -- Test suite for the Term::ANSIColor Perl module.
#
# Before "make install" is performed this script should be runnable with "make
# test".  After "make install" it should work as "perl test.pl".

##############################################################################
# Ensure module can be loaded
##############################################################################

BEGIN { $| = 1; print "1..16\n" }
END   { print "not ok 1\n" unless $loaded }
delete $ENV{ANSI_COLORS_DISABLED};
use Term::ANSIColor qw(:constants color colored uncolor);
$loaded = 1;
print "ok 1\n";

##############################################################################
# Test suite
##############################################################################

# Test simple color attributes.
if (color ('blue on_green', 'bold') eq "\e[34;42;1m") {
    print "ok 2\n";
} else {
    print "not ok 2\n";
}

# Test colored.
if (colored ("testing", 'blue', 'bold') eq "\e[34;1mtesting\e[0m") {
    print "ok 3\n";
} else {
    print "not ok 3\n";
}

# Test the constants.
if (BLUE BOLD "testing" eq "\e[34m\e[1mtesting") {
    print "ok 4\n";
} else {
    print "not ok 4\n";
}

# Test AUTORESET.
$Term::ANSIColor::AUTORESET = 1;
if (BLUE BOLD "testing" eq "\e[34m\e[1mtesting\e[0m\e[0m") {
    print "ok 5\n";
} else {
    print "not ok 5\n";
}

# Test EACHLINE.
$Term::ANSIColor::EACHLINE = "\n";
if (colored ("test\n\ntest", 'bold')
    eq "\e[1mtest\e[0m\n\n\e[1mtest\e[0m") {
    print "ok 6\n";
} else {
    print colored ("test\n\ntest", 'bold'), "\n";
    print "not ok 6\n";
}

# Test EACHLINE with multiple trailing delimiters.
$Term::ANSIColor::EACHLINE = "\r\n";
if (colored ("test\ntest\r\r\n\r\n", 'bold')
    eq "\e[1mtest\ntest\r\e[0m\r\n\r\n") {
    print "ok 7\n";
} else {
    print "not ok 7\n";
}

# Test the array ref form.
$Term::ANSIColor::EACHLINE = "\n";
if (colored (['bold', 'on_green'], "test\n", "\n", "test")
    eq "\e[1;42mtest\e[0m\n\n\e[1;42mtest\e[0m") {
    print "ok 8\n";
} else {
    print colored (['bold', 'on_green'], "test\n", "\n", "test");
    print "not ok 8\n";
}

# Test uncolor.
my @names = uncolor ('1;42', "\e[m", '', "\e[0m");
if (join ('|', @names) eq 'bold|on_green|clear') {
    print "ok 9\n";
} else {
    print join ('|', @names), "\n";
    print "not ok 9\n";
}

# Test ANSI_COLORS_DISABLED.
$ENV{ANSI_COLORS_DISABLED} = 1;
if (color ('blue') eq '') {
    print "ok 10\n";
} else {
    print "not ok 10\n";
}
if (colored ('testing', 'blue', 'on_red') eq 'testing') {
    print "ok 11\n";
} else {
    print "not ok 11\n";
}
if (GREEN 'testing' eq 'testing') {
    print "ok 12\n";
} else {
    print "not ok 12\n";
}
delete $ENV{ANSI_COLORS_DISABLED};

# Make sure DARK is exported.  This was omitted in versions prior to 1.07.
if (DARK "testing" eq "\e[2mtesting\e[0m") {
    print "ok 13\n";
} else {
    print "not ok 13\n";
}

# Test colored with 0 and EACHLINE.
$Term::ANSIColor::EACHLINE = "\n";
if (colored ('0', 'blue', 'bold') eq "\e[34;1m0\e[0m") {
    print "ok 14\n";
} else {
    print "not ok 14\n";
}
if (colored ("0\n0\n\n", 'blue', 'bold')
    eq "\e[34;1m0\e[0m\n\e[34;1m0\e[0m\n\n") {
    print "ok 15\n";
} else {
    print "not ok 15\n";
}

# Test colored with the empty string and EACHLINE.
if (colored ('', 'blue', 'bold') eq '') {
    print "ok 16\n";
} else {
    print "not ok 16\n";
}
