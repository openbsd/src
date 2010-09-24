#!perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

plan tests => 4;

use strict;

my $str = "\x{99f1}\x{99dd}"; # "camel" in Japanese kanji
$str =~ /(.)/;

ok utf8::is_utf8($1), "is_utf8(unistr)";
scalar "$1"; # invoke SvGETMAGIC
ok utf8::is_utf8($1), "is_utf8(unistr)";

utf8::encode($str); # off the utf8 flag
$str =~ /(.)/;

ok !utf8::is_utf8($1), "is_utf8(bytes)";
scalar "$1"; # invoke SvGETMAGIC
ok !utf8::is_utf8($1), "is_utf8(bytes)";
