#!./perl

# This tests (?[...]).  XXX These are just basic tests, as full ones would be
# best done with an infrastructure change to allow getting out the inversion
# list of the constructed set and then comparing it character by character
# with the expected result.

use strict;
use warnings;

$| = 1;

BEGIN {
    chdir 't' if -d 't';
    @INC = ('../lib','.');
    require './test.pl';
}

use utf8;
no warnings 'experimental::regex_sets';

like("a", qr/(?[ [a]      # This is a comment
                    ])/, 'Can ignore a comment');
like("a", qr/(?[ [a]      # [[:notaclass:]]
                    ])/, 'A comment isn\'t parsed');
unlike("\x85", qr/(?[ \t ])/, 'NEL is white space');
unlike("\x85", qr/(?[ [\t] ])/, '... including within nested []');
like("\x85", qr/(?[ \t + \ ])/, 'can escape NEL to match');
like("\x85", qr/(?[ [\] ])/, '... including within nested []');
like("\t", qr/(?[ \t + \ ])/, 'can do basic union');
like("\cK", qr/(?[ \s ])/, '\s matches \cK');
unlike("\cK", qr/(?[ \s - \cK ])/, 'can do basic subtraction');
like(" ", qr/(?[ \s - \cK ])/, 'can do basic subtraction');
like(":", qr/(?[ [:] ])/, '[:] is not a posix class');
unlike("\t", qr/(?[ ! \t ])/, 'can do basic complement');
like("\t", qr/(?[ ! [ ^ \t ] ])/, 'can do basic complement');
unlike("\r", qr/(?[ \t ])/, '\r doesn\'t match \t ');
like("\r", qr/(?[ ! \t ])/, 'can do basic complement');
like("0", qr/(?[ [:word:] & [:digit:] ])/, 'can do basic intersection');
unlike("A", qr/(?[ [:word:] & [:digit:] ])/, 'can do basic intersection');
like("0", qr/(?[[:word:]&[:digit:]])/, 'spaces around internal [] aren\'t required');

like("a", qr/(?[ [a] | [b] ])/, '| means union');
like("b", qr/(?[ [a] | [b] ])/, '| means union');
unlike("c", qr/(?[ [a] | [b] ])/, '| means union');

like("a", qr/(?[ [ab] ^ [bc] ])/, 'basic symmetric difference works');
unlike("b", qr/(?[ [ab] ^ [bc] ])/, 'basic symmetric difference works');
like("c", qr/(?[ [ab] ^ [bc] ])/, 'basic symmetric difference works');

like("2", qr/(?[ ( ( \pN & ( [a] + [2] ) ) ) ])/, 'Nesting parens and grouping');
unlike("a", qr/(?[ ( ( \pN & ( [a] + [2] ) ) ) ])/, 'Nesting parens and grouping');

unlike("\x{17f}", qr/(?[ [k] + \p{Blk=ASCII} ])/i, '/i doesn\'t affect \p{}');
like("\N{KELVIN SIGN}", qr/(?[ [k] + \p{Blk=ASCII} ])/i, '/i does affect literals');

my $thai_or_lao = qr/(?[ \p{Thai} + \p{Lao} ])/;
my $thai_or_lao_digit = qr/(?[ \p{Digit} & $thai_or_lao ])/;
like("\N{THAI DIGIT ZERO}", $thai_or_lao_digit, 'embedded qr/(?[ ])/ works');
unlike(chr(ord("\N{THAI DIGIT ZERO}") - 1), $thai_or_lao_digit, 'embedded qr/(?[ ])/ works');
like("\N{THAI DIGIT NINE}", $thai_or_lao_digit, 'embedded qr/(?[ ])/ works');
unlike(chr(ord("\N{THAI DIGIT NINE}") + 1), $thai_or_lao_digit, 'embedded qr/(?[ ])/ works');
like("\N{LAO DIGIT ZERO}", $thai_or_lao_digit, 'embedded qr/(?[ ])/ works');
unlike(chr(ord("\N{LAO DIGIT ZERO}") - 1), $thai_or_lao_digit, 'embedded qr/(?[ ])/ works');
like("\N{LAO DIGIT NINE}", $thai_or_lao_digit, 'embedded qr/(?[ ])/ works');
unlike(chr(ord("\N{LAO DIGIT NINE}") + 1), $thai_or_lao_digit, 'embedded qr/(?[ ])/ works');

my $ascii_word = qr/(?[ \w ])/a;
my $ascii_digits_plus_all_of_arabic = qr/(?[ \p{Digit} & $ascii_word + \p{Arabic} ])/;
like("9", $ascii_digits_plus_all_of_arabic, "/a, then interpolating and intersection works for ASCII in the set");
unlike("A", $ascii_digits_plus_all_of_arabic, "/a, then interpolating and intersection works for ASCII not in the set");
unlike("\N{BENGALI DIGIT ZERO}", $ascii_digits_plus_all_of_arabic, "/a, then interpolating and intersection works for non-ASCII not in either set");
unlike("\N{BENGALI LETTER A}", $ascii_digits_plus_all_of_arabic, "/a, then interpolating and intersection works for non-ASCII in one set");
like("\N{ARABIC LETTER HAMZA}", $ascii_digits_plus_all_of_arabic, "interpolation and intersection is left-associative");
like("\N{EXTENDED ARABIC-INDIC DIGIT ZERO}", $ascii_digits_plus_all_of_arabic, "interpolation and intersection is left-associative");

my $kelvin = qr/(?[ \N{KELVIN SIGN} ])/;
my $fold = qr/(?[ $kelvin ])/i;
like("\N{KELVIN SIGN}", $kelvin, '"\N{KELVIN SIGN}" matches compiled qr/(?[ \N{KELVIN SIGN} ])/');
unlike("K", $fold, "/i on outer (?[ ]) doesn't leak to interpolated one");
unlike("k", $fold, "/i on outer (?[ ]) doesn't leak to interpolated one");

my $kelvin_fold = qr/(?[ \N{KELVIN SIGN} ])/i;
my $still_fold = qr/(?[ $kelvin_fold ])/;
like("K", $still_fold, "/i on interpolated (?[ ]) is retained in outer without /i");
like("k", $still_fold, "/i on interpolated (?[ ]) is retained in outer without /i");

eval 'my $x = qr/(?[ [a] ])/; qr/(?[ $x ])/';
is($@, "", 'qr/(?[ [a] ])/ can be interpolated');

done_testing();

1;
