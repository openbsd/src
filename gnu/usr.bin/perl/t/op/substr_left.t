BEGIN {
    chdir 't';
    require './test.pl';
    @INC = '../lib';
}

use warnings;
use strict;

my $str; my $result; my $len;

# In each of the tests below, the OP_SUBSTR should have been replaced
# with a specialised OP_SUBSTR_LEFT OP. The tests in this file are
# intended as sanity tests for pp_substr_left's string position
# calculations and treatment of the input `sv` and output TARG.

# Basic functionality with a simple string
$str = "Hello, World!";
$result = substr($str, 0, 5, "");
is($result, "Hello", 'simple case: correct extraction');
is($str, ", World!", 'simple case: remainder is correct');
# LENGTH is zero
$result = substr($str, 0, 0, "");
is($result, "", 'zero length: returns empty string');
is($str, ", World!", 'zero length: EXPR remains unchanged');
# LENGTH is larger than the string
$result = substr($str, 0, 10, "");
is($result, ", World!", 'LENGTH: returns entire string');
is($str, "", 'LENGTH: EXPR is empty');
# EXPR is an empty string
$result = substr($str, 0, 4, "");
is($result, "", 'empty EXPR: returns empty string');
is($str, "", 'empty EXPR: EXPR remains empty');
# EXPR lexical is undef
{
no warnings 'uninitialized';
$str = undef;
$result = substr($str, 0, 2, "");
is($result, "", 'undef EXPR: returns empty string');
is($str, "", 'undef EXPR: EXPR becomes empty string');
# LENGTH is undef
$str = "Hello";
$result = substr($str, 0, undef, "");
is($result, "", 'undef LENGTH: returns empty string');
is($str, "Hello", 'undef LENGTH: EXPR is unchanged');
}
# LENGTH is negative
$result = substr($str, 0, -2, "");
is($result, "Hel", 'negative LENGTH: returns characters 0..length-2');
is($str, "lo", 'negative LENGTH: 2 chars remaining');
# EXPR is numeric (non-string)
$str = 12345678;
$result = substr($str, 0, 6, "");
is($result, "123456", 'IV EXPR: returns stringified characters');
is($str, "78", 'IV EXPR: stringified EXPR');
# LENGTH IS A NV
$str = "Hello, again";
$len = 2.5;
$result = substr($str, 0, $len, "");
is($result, "He", 'NV LENGTH: returns floor() characters');
is($str, "llo, again", 'NV LENGTH: EXPR retains length-floor() characters');

use Tie::Scalar;
{
    package TiedScalar;
    use base 'Tie::StdScalar';
    sub STORE {
        my ($self, $value) = @_;
        $$self = $value;
    }
    sub FETCH {
        my ($self) = @_;
        return $$self;
    }
}
# EXPR is a tied variable
my $str2;
tie $str2, 'TiedScalar';
$str2 = "Hello World";
$result = substr($str2, 0, 5, "");
is($result, "Hello", 'tied EXPR: returns correct characters');
is($str2, " World", 'tied EXPR: tied EXPR variable updated correctly');
# TARG is a tied variable
my $result2;
tie $result2, 'TiedScalar';
$result2 = substr($str2, 0, 2, "");
is($result2, " W", 'tied TARG: returns correct characters');
is($str2, "orld", 'tied TARG: tied EXPR variable updated correctly');
# EXPR is a scalar containing UTF-8 string
use utf8;
$str = "Привет мир"; # "Hello world" in Russian
$result = substr($str, 0, 7, "");
is($result, "Привет ", 'UTF-8 EXPR: returns correct UTF-8 characters');
is($str, "мир", 'UTF-8 EXPR: UTF-8 string updated correctly');
# LENGTH is outside of IV range
use Config;
$str = "Hello, Bernard";
my $max_iv = $Config{ivsize} == 8 ? 9_223_372_036_854_775_807 : 2_147_483_647;
$result = substr($str, 0, $max_iv + 1, "");
is($result, "Hello, Bernard", 'UV LENGTH: returns entire string');
is($str, "", 'UV LENGTH: EXPR is emptied');
# EXPR contains binary data
$str = "\x00\x01\x02\x03\x04\x05";
$result = substr($str, 0, 3, "");
is($result, "\x00\x01\x02", 'hex EXPR: returns correct characters');
is($str, "\x03\x04\x05", 'hex EXPR: retains correct characters');
# GH #22914. LEN has more than one pointer to REPL.
$str = "perl";
# Hopefully $INC[0] ne '/dev/random' is a reasonable test assumption...
# (We need a condition that no future clever optimiser will strip)
$result = substr($str, 0, $INC[0] eq '/dev/random' ? 2: 3, '');
is($result, 'per', 'GH#22914: non-trivial LEN returns correct characters');
is($str, 'l', 'GH#22914: non-trivial LEN retains correct characters');

done_testing();
