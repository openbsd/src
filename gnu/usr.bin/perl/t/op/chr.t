#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = qw(. ../lib); # ../lib needed for test.deparse
    require "test.pl";
}

plan tests => 26;

# Note that t/op/ord.t already tests for chr() <-> ord() rountripping.

# Don't assume ASCII.

is(chr(ord("A")), "A");

is(chr(  0), "\x00");
is(chr(127), "\x7F");
is(chr(128), "\x80");
is(chr(255), "\xFF");

# is(chr(-1), undef); # Shouldn't it be?

# Check UTF-8.

sub hexes { join(" ",map{sprintf"%02x",$_}unpack("C*",chr($_[0]))) }

# The following code points are some interesting steps in UTF-8.
is(hexes(   0x100), "c4 80");
is(hexes(   0x7FF), "df bf");
is(hexes(   0x800), "e0 a0 80");
is(hexes(   0xFFF), "e0 bf bf");
is(hexes(  0x1000), "e1 80 80");
is(hexes(  0xCFFF), "ec bf bf");
is(hexes(  0xD000), "ed 80 80");
is(hexes(  0xD7FF), "ed 9f bf");
is(hexes(  0xD800), "ed a0 80"); # not strict utf-8 (surrogate area begin)
is(hexes(  0xDFFF), "ed bf bf"); # not strict utf-8 (surrogate area end)
is(hexes(  0xE000), "ee 80 80");
is(hexes(  0xFFFF), "ef bf bf");
is(hexes( 0x10000), "f0 90 80 80");
is(hexes( 0x3FFFF), "f0 bf bf bf");
is(hexes( 0x40000), "f1 80 80 80");
is(hexes( 0xFFFFF), "f3 bf bf bf");
is(hexes(0x100000), "f4 80 80 80");
is(hexes(0x10FFFF), "f4 8f bf bf"); # Unicode (4.1) last code point
is(hexes(0x110000), "f4 90 80 80");
is(hexes(0x1FFFFF), "f7 bf bf bf"); # last four byte encoding
is(hexes(0x200000), "f8 88 80 80 80");

