#!./perl
#
# Tests that have to do with checking whether characters have (or not have)
# certain Unicode properties; belong (or not belong) to blocks, scripts, etc.
#

use strict;
use warnings;
use 5.010;

my $IS_EBCDIC = ord ('A') == 193;

sub run_tests;

#
# This is the data to test.
#
# This is a hash; keys are the property to test.
# Values are arrays containing characters to test. The characters can
# have the following formats:
#   '\N{CHARACTER NAME}'  -  Use character with that name
#   '\x{1234}'            -  Use character with that hex escape
#   '0x1234'              -  Use chr() to get that character
#   "a"                   -  Character to use
#
# If a character entry starts with ! the character does not belong to the class
#
# If the class is just single letter, we use both \pL and \p{L}
#

use charnames ':full';

my @CLASSES = (
    L                         => ["a", "A"],
    Ll                        => ["b", "!B"],
    Lu                        => ["!c", "C"],
    IsLl                      => ["d", "!D"],
    IsLu                      => ["!e", "E"],
    LC                        => ["f", "!1"],
   'L&'                       => ["g", "!2"],
   'Lowercase Letter'         => ["h", "!H"],

    Common                    => ["!i", "3"],
    Inherited                 => ["!j", '\x{300}'],

    InBasicLatin              => ['\N{LATIN CAPITAL LETTER A}'],
    InLatin1Supplement        => ['\N{LATIN CAPITAL LETTER A WITH GRAVE}'],
    InLatinExtendedA          => ['\N{LATIN CAPITAL LETTER A WITH MACRON}'],
    InLatinExtendedB          => ['\N{LATIN SMALL LETTER B WITH STROKE}'],
    InKatakana                => ['\N{KATAKANA LETTER SMALL A}'],
    IsLatin                   => ["0x100", "0x212b"],
    IsHebrew                  => ["0x5d0", "0xfb4f"],
    IsGreek                   => ["0x37a", "0x386", "!0x387", "0x388",
                                  "0x38a", "!0x38b", "0x38c"],
    HangulSyllables           => ['\x{AC00}'],
   'Script=Latin'             => ['\x{0100}'],
   'Block=LatinExtendedA'     => ['\x{0100}'],
   'Category=UppercaseLetter' => ['\x{0100}'],

    #
    # It's ok to repeat class names.
    #
    InLatin1Supplement        =>
               $IS_EBCDIC ? ['!\x{7f}',  '\x{80}',            '!\x{100}']
                          : ['!\x{7f}',  '\x{80}',  '\x{ff}', '!\x{100}'],
    InLatinExtendedA          =>
                            ['!\x{7f}', '!\x{80}', '!\x{ff}',  '\x{100}'],

    #
    # Properties are case-insensitive, and may have whitespace,
    # dashes and underscores.
    #
   'in-latin1_SUPPLEMENT'     => ['\x{80}', 
                                  '\N{LATIN SMALL LETTER Y WITH DIAERESIS}'],
   '  ^  In Latin 1 Supplement  '
                              => ['!\x{80}', '\N{COFFIN}'],
   'latin-1   supplement'     => ['\x{80}', "0xDF"],

);

my @USER_DEFINED_PROPERTIES = (
   #
   # User defined properties
   #
   InKana1                   => ['\x{3040}', '!\x{303F}'],
   InKana2                   => ['\x{3040}', '!\x{303F}'],
   InKana3                   => ['\x{3041}', '!\x{3040}'],
   InNotKana                 => ['\x{3040}', '!\x{3041}'],
   InConsonant               => ['d',        '!e'],
   IsSyriac1                 => ['\x{0712}', '!\x{072F}'],
   Syriac1                   => ['\x{0712}', '!\x{072F}'],
   '# User-defined character properties my lack \n at the end',
   InGreekSmall              => ['\N{GREEK SMALL LETTER PI}',
                                 '\N{GREEK SMALL LETTER FINAL SIGMA}'],
   InGreekCapital            => ['\N{GREEK CAPITAL LETTER PI}', '!\x{03A2}'],
   Dash                      => ['-'],
   ASCII_Hex_Digit           => ['!-', 'A'],
   AsciiHexAndDash           => ['-', 'A'],
);


#
# From the short properties we populate POSIX-like classes.
#
my %SHORT_PROPERTIES = (
    'Ll'  => ['m', '\N{CYRILLIC SMALL LETTER A}'],
    'Lu'  => ['M', '\N{GREEK CAPITAL LETTER ALPHA}'],
    'Lo'  => ['\N{HIRAGANA LETTER SMALL A}'],
    # is also in other alphabetic
    'Mn'  => ['\N{HEBREW POINT RAFE}'],
    'Nd'  => ["0", '\N{ARABIC-INDIC DIGIT ZERO}'],
    'Pc'  => ["_"],
    'Po'  => ["!"],
    'Zs'  => [" "],
    'Cc'  => ['\x{00}'],
);

#
# Illegal properties
#
my @ILLEGAL_PROPERTIES = qw [q qrst];

my %d;

while (my ($class, $chars) = each %SHORT_PROPERTIES) {
    push @{$d {IsAlpha}} => map {$class =~ /^[LM]/   ? $_ : "!$_"} @$chars;
    push @{$d {IsAlnum}} => map {$class =~ /^[LMN]./ ? $_ : "!$_"} @$chars;
    push @{$d {IsASCII}} => map {length ($_) == 1 || $_ eq '\x{00}'
                                                     ? $_ : "!$_"} @$chars;
    push @{$d {IsCntrl}} => map {$class =~ /^C/      ? $_ : "!$_"} @$chars;
    push @{$d {IsBlank}} => map {$class =~ /^Z[lps]/ ? $_ : "!$_"} @$chars;
    push @{$d {IsDigit}} => map {$class =~ /^Nd$/    ? $_ : "!$_"} @$chars;
    push @{$d {IsGraph}} => map {$class =~ /^([LMNPS]|Co)/
                                                     ? $_ : "!$_"} @$chars;
    push @{$d {IsPrint}} => map {$class =~ /^([LMNPS]|Co|Zs)/
                                                     ? $_ : "!$_"} @$chars;
    push @{$d {IsLower}} => map {$class =~ /^Ll$/    ? $_ : "!$_"} @$chars;
    push @{$d {IsUpper}} => map {$class =~ /^L[ut]/  ? $_ : "!$_"} @$chars;
    push @{$d {IsPunct}} => map {$class =~ /^P/      ? $_ : "!$_"} @$chars;
    push @{$d {IsWord}}  => map {$class =~ /^[LMN]/ || $_ eq "_"
                                                     ? $_ : "!$_"} @$chars;
    push @{$d {IsSpace}} => map {$class =~ /^Z/ ||
                                 length ($_) == 1 && ord ($_) >= 0x09
                                                  && ord ($_) <= 0x0D
                                                     ? $_ : "!$_"} @$chars;
}

delete $d {IsASCII} if $IS_EBCDIC;

push @CLASSES => "# Short properties"        => %SHORT_PROPERTIES,
                 "# POSIX like properties"   => %d,
                 "# User defined properties" => @USER_DEFINED_PROPERTIES;


#
# Calculate the number of tests.
#
my $count = 0;
for (my $i = 0; $i < @CLASSES; $i += 2) {
    $i ++, redo if $CLASSES [$i] =~ /^\h*#\h*(.*)/;
    $count += (length $CLASSES [$i] == 1 ? 4 : 2) * @{$CLASSES [$i + 1]};
}
$count += 2 * @ILLEGAL_PROPERTIES;
$count += 2 * grep {length $_ == 1} @ILLEGAL_PROPERTIES;

my $tests = 0;

say "1..$count";

run_tests unless caller ();

sub match {
    my ($char, $match, $nomatch) = @_;

    my ($str, $name);

    given ($char) {
        when (/^\\/) {
            $str  = eval qq ["$char"];
            $name =      qq ["$char"];
        }
        when (/^0x([0-9A-Fa-f]+)$/) {
            $str  =  chr hex $1;
            $name = "chr ($char)";
        }
        default {
            $str  =      $char;
            $name = qq ["$char"];
        }
    }

    print "not " unless $str =~ /$match/;
    print "ok ", ++ $tests, " - $name =~ /$match/\n";
    print "not " unless $str !~ /$nomatch/;
    print "ok ", ++ $tests, " - $name !~ /$nomatch/\n";
}

sub run_tests {

    while (@CLASSES) {
        my $class = shift @CLASSES;
        if ($class =~ /^\h*#\h*(.*)/) {
            print "# $1\n";
            next;
        }
        last unless @CLASSES;
        my $chars   = shift @CLASSES;
        my @in      =                       grep {!/^!./} @$chars;
        my @out     = map {s/^!(?=.)//; $_} grep { /^!./} @$chars;
        my $in_pat  = eval qq ['\\p{$class}'];
        my $out_pat = eval qq ['\\P{$class}'];

        match $_, $in_pat,  $out_pat for @in;
        match $_, $out_pat, $in_pat  for @out;

        if (1 == length $class) {
            my $in_pat  = eval qq ['\\p$class'];
            my $out_pat = eval qq ['\\P$class'];

            match $_, $in_pat,  $out_pat for @in;
            match $_, $out_pat, $in_pat  for @out;
        }
    }


    my $pat = qr /^Can't find Unicode property definition/;
    print "# Illegal properties\n";
    foreach my $p (@ILLEGAL_PROPERTIES) {
        undef $@;
        my $r = eval "'a' =~ /\\p{$p}/; 1";
        print "not " unless !$r && $@ && $@ =~ $pat;
        print "ok ", ++ $tests, " - Unknown Unicode property \\p{$p}\n";
        undef $@;
        my $s = eval "'a' =~ /\\P{$p}/; 1";
        print "not " unless !$s && $@ && $@ =~ $pat;
        print "ok ", ++ $tests, " - Unknown Unicode property \\P{$p}\n";
        if (length $p == 1) {
            undef $@;
            my $r = eval "'a' =~ /\\p$p/; 1";
            print "not " unless !$r && $@ && $@ =~ $pat;
            print "ok ", ++ $tests, " - Unknown Unicode property \\p$p\n";
            undef $@;
            my $s = eval "'a' =~ /\\P$p/; 1";
            print "not " unless !$s && $@ && $@ =~ $pat;
            print "ok ", ++ $tests, " - Unknown Unicode property \\P$p\n";
        }
    }
}


#
# User defined properties
#

sub InKana1 {<<'--'}
3040    309F
30A0    30FF
--

sub InKana2 {<<'--'}
+utf8::InHiragana
+utf8::InKatakana
--

sub InKana3 {<<'--'}
+utf8::InHiragana
+utf8::InKatakana
-utf8::IsCn
--

sub InNotKana {<<'--'}
!utf8::InHiragana
-utf8::InKatakana
+utf8::IsCn
--

sub InConsonant {<<'--'}   # Not EBCDIC-aware.
0061 007f
-0061
-0065
-0069
-006f
-0075
--

sub IsSyriac1 {<<'--'}
0712    072C
0730    074A
--

sub Syriac1 {<<'--'}
0712    072C
0730    074A
--

sub InGreekSmall   {return "03B1\t03C9"}
sub InGreekCapital {return "0391\t03A9\n-03A2"}

sub AsciiHexAndDash {<<'--'}
+utf8::ASCII_Hex_Digit
+utf8::Dash
--

__END__
