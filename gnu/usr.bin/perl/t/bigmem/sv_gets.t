#!perl
BEGIN {
    chdir 't' if -d 't';
    @INC = "../lib";
}

use strict;
require './test.pl';
use Config qw(%Config);

# 2 for the child, 2 for the parent
$ENV{PERL_TEST_MEMORY} >= 4
    or skip_all("Need ~4Gb for this test");
$Config{ptrsize} >= 8
    or skip_all("Need 64-bit pointers for this test");

{
    # https://www.perlmonks.org/?node_id=11161665
    my $x = `$^X -e "print q/x/ x 0x80000000"`;
    is(length $x, 0x80000000, "check entire input read");
    undef $x;
}

{
    # sv_gets_append_to_utf8 append parameter
    my $x = "x";
    $x x= 0x8000_0000;
    utf8::upgrade($x);
    # using bareword handle because the rcatline optimization isn't done
    # for non-barewords
    # for $fh
    # we chdired to "t"
    open FH, "<", "TEST" or die $!;
    $x .= <FH>;
    pass("didn't crash appending readline to large upgraded scalar");
}

{
    # sv_gets_read_record append parameter
    my $x = "x";
    $x x= 0x8000_0000;
    open FH, "<", "TEST" or die $!;
    local $/ = \100;
    $x .= <FH>;
    pass("didn't crash appending readline record to large scalar");
}

done_testing();
