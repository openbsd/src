#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = qw(. ../lib); # ../lib needed for test.deparse
    require "test.pl";
}

plan tests => 7;

# compile time evaluation

# 'A' 65	ASCII
# 'A' 193	EBCDIC

ok(ord('A') == 65 || ord('A') == 193, "ord('A') is ".ord('A'));

is(ord(chr(500)), 500, "compile time chr 500");

# run time evaluation

$x = 'ABC';

ok(ord($x) == 65 || ord($x) == 193, "ord('$x') is ".ord($x));

ok(chr 65 eq 'A' || chr 193 eq 'A', "chr can produce 'A'");

$x = 500;
is(ord(chr($x)), $x, "runtime chr $x");

is(ord("\x{1234}"), 0x1234, 'compile time ord \x{....}');

$x = "\x{1234}";
is(ord($x), 0x1234, 'runtime ord \x{....}');

