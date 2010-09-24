#!./perl
#
# This is a home for regular expression tests that don't fit into
# the format supported by re/regexp.t.  If you want to add a test
# that does fit that format, add it to re/re_tests, not here.

use strict;
use warnings;
use 5.010;


sub run_tests;

$| = 1;


BEGIN {
    chdir 't' if -d 't';
    @INC = ('../lib','.');
    do "re/ReTest.pl" or die $@;
}


plan tests => 1159;  # Update this when adding/deleting tests.

run_tests() unless caller;

#
# Tests start here.
#
sub run_tests {

  SKIP:
    {
        local $Message = '\C matches octet';
        $_ = "a\x{100}b";
        ok /(.)(\C)(\C)(.)/ or skip q [\C doesn't match], 4;
        iseq $1, "a";
        if ($IS_ASCII) {     # ASCII (or equivalent), should be UTF-8
            iseq $2, "\xC4";
            iseq $3, "\x80";
        }
        elsif ($IS_EBCDIC) { # EBCDIC (or equivalent), should be UTF-EBCDIC
            iseq $2, "\x8C";
            iseq $3, "\x41";
        }
        else {
            SKIP: {
                ok 0, "Unexpected platform", "ord ('A') = $ordA";
                skip "Unexpected platform";
            }
        }
        iseq $4, "b";
    }


  SKIP:
    {
        local $Message = '\C matches octet';
        $_ = "\x{100}";
        ok /(\C)/g or skip q [\C doesn't match], 2;
        if ($IS_ASCII) {
            iseq $1, "\xC4";
        }
        elsif ($IS_EBCDIC) {
            iseq $1, "\x8C";
        }
        else {
            ok 0, "Unexpected platform", "ord ('A') = $ordA";
        }
        ok /(\C)/g or skip q [\C doesn't match];
        if ($IS_ASCII) {
            iseq $1, "\x80";
        }
        elsif ($IS_EBCDIC) {
            iseq $1, "\x41";
        }
        else {
            ok 0, "Unexpected platform", "ord ('A') = $ordA";
        }
    }


    {
        # Japhy -- added 03/03/2001
        () = (my $str = "abc") =~ /(...)/;
        $str = "def";
        iseq $1, "abc", 'Changing subject does not modify $1';
    }


  SKIP:
    {
        # The trick is that in EBCDIC the explicit numeric range should
        # match (as also in non-EBCDIC) but the explicit alphabetic range
        # should not match.
        ok "\x8e" =~ /[\x89-\x91]/, '"\x8e" =~ /[\x89-\x91]/';
        ok "\xce" =~ /[\xc9-\xd1]/, '"\xce" =~ /[\xc9-\xd1]/';

        skip "Not an EBCDIC platform", 2 unless ord ('i') == 0x89 &&
                                                ord ('J') == 0xd1;

        # In most places these tests would succeed since \x8e does not
        # in most character sets match 'i' or 'j' nor would \xce match
        # 'I' or 'J', but strictly speaking these tests are here for
        # the good of EBCDIC, so let's test these only there.
        nok "\x8e" !~ /[i-j]/, '"\x8e" !~ /[i-j]/';
        nok "\xce" !~ /[I-J]/, '"\xce" !~ /[I-J]/';
    }


    {
        ok "\x{ab}"   =~ /\x{ab}/,   '"\x{ab}"   =~ /\x{ab}/  ';
        ok "\x{abcd}" =~ /\x{abcd}/, '"\x{abcd}" =~ /\x{abcd}/';
    }


    {
        local $Message = 'bug id 20001008.001';

        my @x = ("stra\337e 138", "stra\337e 138");
        for (@x) {
            ok s/(\d+)\s*([\w\-]+)/$1 . uc $2/e;
            ok my ($latin) = /^(.+)(?:\s+\d)/;
            iseq $latin, "stra\337e";
        ok $latin =~ s/stra\337e/straße/;
            #
            # Previous code follows, but outcommented - there were no tests.
            #
            # $latin =~ s/stra\337e/straße/; # \303\237 after the 2nd a
            # use utf8; # needed for the raw UTF-8
            # $latin =~ s!(s)tr(?:aß|s+e)!$1tr.!; # \303\237 after the a
        }
    }


    {
        local $Message = 'Test \x escapes';
        ok "ba\xd4c" =~ /([a\xd4]+)/ && $1 eq "a\xd4";
        ok "ba\xd4c" =~ /([a\xd4]+)/ && $1 eq "a\x{d4}";
        ok "ba\x{d4}c" =~ /([a\xd4]+)/ && $1 eq "a\x{d4}";
        ok "ba\x{d4}c" =~ /([a\xd4]+)/ && $1 eq "a\xd4";
        ok "ba\xd4c" =~ /([a\x{d4}]+)/ && $1 eq "a\xd4";
        ok "ba\xd4c" =~ /([a\x{d4}]+)/ && $1 eq "a\x{d4}";
        ok "ba\x{d4}c" =~ /([a\x{d4}]+)/ && $1 eq "a\x{d4}";
        ok "ba\x{d4}c" =~ /([a\x{d4}]+)/ && $1 eq "a\xd4";
    }


  SKIP:
    {
        local $Message = 'Match code points > 255';
        $_ = "abc\x{100}\x{200}\x{300}\x{380}\x{400}defg";
        ok /(.\x{300})./ or skip "No match", 4;
        ok $` eq "abc\x{100}"            && length ($`) == 4;
        ok $& eq "\x{200}\x{300}\x{380}" && length ($&) == 3;
        ok $' eq "\x{400}defg"           && length ($') == 5;
        ok $1 eq "\x{200}\x{300}"        && length ($1) == 2;
    }



    {
        my $x = "\x{10FFFD}";
        $x =~ s/(.)/$1/g;
        ok ord($x) == 0x10FFFD && length($x) == 1, "From Robin Houston";
    }


    {
        my %d = (
            "7f" => [0, 0, 0],
            "80" => [1, 1, 0],
            "ff" => [1, 1, 0],
           "100" => [0, 1, 1],
        );
      SKIP:
        while (my ($code, $match) = each %d) {
            local $Message = "Properties of \\x$code";
            my $char = eval qq ["\\x{$code}"];
            my $i = 0;
            ok (($char =~ /[\x80-\xff]/)            xor !$$match [$i ++]);
            ok (($char =~ /[\x80-\x{100}]/)         xor !$$match [$i ++]);
            ok (($char =~ /[\x{100}]/)              xor !$$match [$i ++]);
        }
    }


    {
        # From Japhy
        local $Message;
        must_warn 'qr/(?c)/',    '^Useless \(\?c\)';
        must_warn 'qr/(?-c)/',   '^Useless \(\?-c\)';
        must_warn 'qr/(?g)/',    '^Useless \(\?g\)';
        must_warn 'qr/(?-g)/',   '^Useless \(\?-g\)';
        must_warn 'qr/(?o)/',    '^Useless \(\?o\)';
        must_warn 'qr/(?-o)/',   '^Useless \(\?-o\)';

        # Now test multi-error regexes
        must_warn 'qr/(?g-o)/',  '^Useless \(\?g\).*\nUseless \(\?-o\)';
        must_warn 'qr/(?g-c)/',  '^Useless \(\?g\).*\nUseless \(\?-c\)';
        # (?c) means (?g) error won't be thrown
        must_warn 'qr/(?o-cg)/', '^Useless \(\?o\).*\nUseless \(\?-c\)';
        must_warn 'qr/(?ogc)/',  '^Useless \(\?o\).*\nUseless \(\?g\).*\n' .
                                  'Useless \(\?c\)';
    }


    {
        local $Message = "/x tests";
        $_ = "foo";
        eval_ok <<"        --";
          /f
           o\r
           o
           \$
          /x
        --
        eval_ok <<"        --";
          /f
           o
           o
           \$\r
          /x
        --
    }


    {
        local $Message = "/o feature";
        sub test_o {$_ [0] =~ /$_[1]/o; return $1}
        iseq test_o ('abc', '(.)..'), 'a';
        iseq test_o ('abc', '..(.)'), 'a';
    }

    {
        # Test basic $^N usage outside of a regex
        local $Message = '$^N usage outside of a regex';
        my $x = "abcdef";
        ok ($x =~ /cde/                  and !defined $^N);
        ok ($x =~ /(cde)/                and $^N eq "cde");
        ok ($x =~ /(c)(d)(e)/            and $^N eq   "e");
        ok ($x =~ /(c(d)e)/              and $^N eq "cde");
        ok ($x =~ /(foo)|(c(d)e)/        and $^N eq "cde");
        ok ($x =~ /(c(d)e)|(foo)/        and $^N eq "cde");
        ok ($x =~ /(c(d)e)|(abc)/        and $^N eq "abc");
        ok ($x =~ /(c(d)e)|(abc)x/       and $^N eq "cde");
        ok ($x =~ /(c(d)e)(abc)?/        and $^N eq "cde");
        ok ($x =~ /(?:c(d)e)/            and $^N eq   "d");
        ok ($x =~ /(?:c(d)e)(?:f)/       and $^N eq   "d");
        ok ($x =~ /(?:([abc])|([def]))*/ and $^N eq   "f");
        ok ($x =~ /(?:([ace])|([bdf]))*/ and $^N eq   "f");
        ok ($x =~ /(([ace])|([bd]))*/    and $^N eq   "e");
       {ok ($x =~ /(([ace])|([bdf]))*/   and $^N eq   "f");}
        ## Test to see if $^N is automatically localized -- it should now
        ## have the value set in the previous test.
        iseq $^N, "e", '$^N is automatically localized';

        # Now test inside (?{ ... })
        local $Message = '$^N usage inside (?{ ... })';
        our ($y, $z);
        ok ($x =~ /a([abc])(?{$y=$^N})c/                    and $y eq  "b");
        ok ($x =~ /a([abc]+)(?{$y=$^N})d/                   and $y eq  "bc");
        ok ($x =~ /a([abcdefg]+)(?{$y=$^N})d/               and $y eq  "bc");
        ok ($x =~ /(a([abcdefg]+)(?{$y=$^N})d)(?{$z=$^N})e/ and $y eq  "bc"
                                                            and $z eq "abcd");
        ok ($x =~ /(a([abcdefg]+)(?{$y=$^N})de)(?{$z=$^N})/ and $y eq  "bc"
                                                            and $z eq "abcde");

    }


  SKIP:
    {
        ## Should probably put in tests for all the POSIX stuff,
        ## but not sure how to guarantee a specific locale......

        skip "Not an ASCII platform", 2 unless $IS_ASCII;
        local $Message = 'Test [[:cntrl:]]';
        my $AllBytes = join "" => map {chr} 0 .. 255;
        (my $x = $AllBytes) =~ s/[[:cntrl:]]//g;
        iseq $x, join "", map {chr} 0x20 .. 0x7E, 0x80 .. 0xFF;

        ($x = $AllBytes) =~ s/[^[:cntrl:]]//g;
        iseq $x, join "", map {chr} 0x00 .. 0x1F, 0x7F;
    }


    {
        # With /s modifier UTF8 chars were interpreted as bytes
        local $Message = "UTF-8 chars aren't bytes";
        my $a = "Hello \x{263A} World";
        my @a = ($a =~ /./gs);
        iseq $#a, 12;
    }


    {
        local $Message = '. matches \n with /s';
        my $str1 = "foo\nbar";
        my $str2 = "foo\n\x{100}bar";
        my ($a, $b) = map {chr} $IS_ASCII ? (0xc4, 0x80) : (0x8c, 0x41);
        my @a;
        @a = $str1 =~ /./g;   iseq @a, 6; iseq "@a", "f o o b a r";
        @a = $str1 =~ /./gs;  iseq @a, 7; iseq "@a", "f o o \n b a r";
        @a = $str1 =~ /\C/g;  iseq @a, 7; iseq "@a", "f o o \n b a r";
        @a = $str1 =~ /\C/gs; iseq @a, 7; iseq "@a", "f o o \n b a r";
        @a = $str2 =~ /./g;   iseq @a, 7; iseq "@a", "f o o \x{100} b a r";
        @a = $str2 =~ /./gs;  iseq @a, 8; iseq "@a", "f o o \n \x{100} b a r";
        @a = $str2 =~ /\C/g;  iseq @a, 9; iseq "@a", "f o o \n $a $b b a r";
        @a = $str2 =~ /\C/gs; iseq @a, 9; iseq "@a", "f o o \n $a $b b a r";
    }


    {
        no warnings 'digit';
        # Check that \x## works. 5.6.1 and 5.005_03 fail some of these.
        my $x;
        $x = "\x4e" . "E";
        ok ($x =~ /^\x4EE$/, "Check only 2 bytes of hex are matched.");

        $x = "\x4e" . "i";
        ok ($x =~ /^\x4Ei$/, "Check that invalid hex digit stops it (2)");

        $x = "\x4" . "j";
        ok ($x =~ /^\x4j$/,  "Check that invalid hex digit stops it (1)");

        $x = "\x0" . "k";
        ok ($x =~ /^\xk$/,   "Check that invalid hex digit stops it (0)");

        $x = "\x0" . "x";
        ok ($x =~ /^\xx$/, "\\xx isn't to be treated as \\0");

        $x = "\x0" . "xa";
        ok ($x =~ /^\xxa$/, "\\xxa isn't to be treated as \\xa");

        $x = "\x9" . "_b";
        ok ($x =~ /^\x9_b$/, "\\x9_b isn't to be treated as \\x9b");

        # and now again in [] ranges

        $x = "\x4e" . "E";
        ok ($x =~ /^[\x4EE]{2}$/, "Check only 2 bytes of hex are matched.");

        $x = "\x4e" . "i";
        ok ($x =~ /^[\x4Ei]{2}$/, "Check that invalid hex digit stops it (2)");

        $x = "\x4" . "j";
        ok ($x =~ /^[\x4j]{2}$/,  "Check that invalid hex digit stops it (1)");

        $x = "\x0" . "k";
        ok ($x =~ /^[\xk]{2}$/,   "Check that invalid hex digit stops it (0)");

        $x = "\x0" . "x";
        ok ($x =~ /^[\xx]{2}$/, "\\xx isn't to be treated as \\0");

        $x = "\x0" . "xa";
        ok ($x =~ /^[\xxa]{3}$/, "\\xxa isn't to be treated as \\xa");

        $x = "\x9" . "_b";
        ok ($x =~ /^[\x9_b]{3}$/, "\\x9_b isn't to be treated as \\x9b");

        # Check that \x{##} works. 5.6.1 fails quite a few of these.

        $x = "\x9b";
        ok ($x =~ /^\x{9_b}$/, "\\x{9_b} is to be treated as \\x9b");

        $x = "\x9b" . "y";
        ok ($x =~ /^\x{9_b}y$/, "\\x{9_b} is to be treated as \\x9b (again)");

        $x = "\x9b" . "y";
        ok ($x =~ /^\x{9b_}y$/, "\\x{9b_} is to be treated as \\x9b");

        $x = "\x9b" . "y";
        ok ($x =~ /^\x{9_bq}y$/, "\\x{9_bc} is to be treated as \\x9b");

        $x = "\x0" . "y";
        ok ($x =~ /^\x{x9b}y$/, "\\x{x9b} is to be treated as \\x0");

        $x = "\x0" . "y";
        ok ($x =~ /^\x{0x9b}y$/, "\\x{0x9b} is to be treated as \\x0");

        $x = "\x9b" . "y";
        ok ($x =~ /^\x{09b}y$/, "\\x{09b} is to be treated as \\x9b");

        $x = "\x9b";
        ok ($x =~ /^[\x{9_b}]$/, "\\x{9_b} is to be treated as \\x9b");

        $x = "\x9b" . "y";
        ok ($x =~ /^[\x{9_b}y]{2}$/,
                                 "\\x{9_b} is to be treated as \\x9b (again)");

        $x = "\x9b" . "y";
        ok ($x =~ /^[\x{9b_}y]{2}$/, "\\x{9b_} is to be treated as \\x9b");

        $x = "\x9b" . "y";
        ok ($x =~ /^[\x{9_bq}y]{2}$/, "\\x{9_bc} is to be treated as \\x9b");

        $x = "\x0" . "y";
        ok ($x =~ /^[\x{x9b}y]{2}$/, "\\x{x9b} is to be treated as \\x0");

        $x = "\x0" . "y";
        ok ($x =~ /^[\x{0x9b}y]{2}$/, "\\x{0x9b} is to be treated as \\x0");

        $x = "\x9b" . "y";
        ok ($x =~ /^[\x{09b}y]{2}$/, "\\x{09b} is to be treated as \\x9b");

    }


    {
        # High bit bug -- japhy
        my $x = "ab\200d";
        ok $x =~ /.*?\200/, "High bit fine";
    }


    {
        # The basic character classes and Unicode
        ok "\x{0100}" =~ /\w/, 'LATIN CAPITAL LETTER A WITH MACRON in /\w/';
        ok "\x{0660}" =~ /\d/, 'ARABIC-INDIC DIGIT ZERO in /\d/';
        ok "\x{1680}" =~ /\s/, 'OGHAM SPACE MARK in /\s/';
    }


    {
        local $Message = "Folding matches and Unicode";
        ok "a\x{100}" =~ /A/i;
        ok "A\x{100}" =~ /a/i;
        ok "a\x{100}" =~ /a/i;
        ok "A\x{100}" =~ /A/i;
        ok "\x{101}a" =~ /\x{100}/i;
        ok "\x{100}a" =~ /\x{100}/i;
        ok "\x{101}a" =~ /\x{101}/i;
        ok "\x{100}a" =~ /\x{101}/i;
        ok "a\x{100}" =~ /A\x{100}/i;
        ok "A\x{100}" =~ /a\x{100}/i;
        ok "a\x{100}" =~ /a\x{100}/i;
        ok "A\x{100}" =~ /A\x{100}/i;
        ok "a\x{100}" =~ /[A]/i;
        ok "A\x{100}" =~ /[a]/i;
        ok "a\x{100}" =~ /[a]/i;
        ok "A\x{100}" =~ /[A]/i;
        ok "\x{101}a" =~ /[\x{100}]/i;
        ok "\x{100}a" =~ /[\x{100}]/i;
        ok "\x{101}a" =~ /[\x{101}]/i;
        ok "\x{100}a" =~ /[\x{101}]/i;
    }


    {
        use charnames ':full';
        local $Message = "Folding 'LATIN LETTER A WITH GRAVE'";

        my $lower = "\N{LATIN SMALL LETTER A WITH GRAVE}";
        my $UPPER = "\N{LATIN CAPITAL LETTER A WITH GRAVE}";

        ok $lower =~ m/$UPPER/i;
        ok $UPPER =~ m/$lower/i;
        ok $lower =~ m/[$UPPER]/i;
        ok $UPPER =~ m/[$lower]/i;

        local $Message = "Folding 'GREEK LETTER ALPHA WITH VRACHY'";

        $lower = "\N{GREEK CAPITAL LETTER ALPHA WITH VRACHY}";
        $UPPER = "\N{GREEK SMALL LETTER ALPHA WITH VRACHY}";

        ok $lower =~ m/$UPPER/i;
        ok $UPPER =~ m/$lower/i;
        ok $lower =~ m/[$UPPER]/i;
        ok $UPPER =~ m/[$lower]/i;

        local $Message = "Folding 'LATIN LETTER Y WITH DIAERESIS'";

        $lower = "\N{LATIN SMALL LETTER Y WITH DIAERESIS}";
        $UPPER = "\N{LATIN CAPITAL LETTER Y WITH DIAERESIS}";

        ok $lower =~ m/$UPPER/i;
        ok $UPPER =~ m/$lower/i;
        ok $lower =~ m/[$UPPER]/i;
        ok $UPPER =~ m/[$lower]/i;
    }


    {
        use charnames ':full';
        local $PatchId = "13843";
        local $Message = "GREEK CAPITAL LETTER SIGMA vs " .
                         "COMBINING GREEK PERISPOMENI";

        my $SIGMA = "\N{GREEK CAPITAL LETTER SIGMA}";
        my $char  = "\N{COMBINING GREEK PERISPOMENI}";

        may_not_warn sub {ok "_:$char:_" !~ m/_:$SIGMA:_/i};
    }


    {
        local $Message = '\X';
        use charnames ':full';

        ok "a!"                          =~ /^(\X)!/ && $1 eq "a";
        ok "\xDF!"                       =~ /^(\X)!/ && $1 eq "\xDF";
        ok "\x{100}!"                    =~ /^(\X)!/ && $1 eq "\x{100}";
        ok "\x{100}\x{300}!"             =~ /^(\X)!/ && $1 eq "\x{100}\x{300}";
        ok "\N{LATIN CAPITAL LETTER E}!" =~ /^(\X)!/ &&
               $1 eq "\N{LATIN CAPITAL LETTER E}";
        ok "\N{LATIN CAPITAL LETTER E}\N{COMBINING GRAVE ACCENT}!"
                                         =~ /^(\X)!/ &&
               $1 eq "\N{LATIN CAPITAL LETTER E}\N{COMBINING GRAVE ACCENT}";

        local $Message = '\C and \X';
        ok "!abc!" =~ /a\Cc/;
        ok "!abc!" =~ /a\Xc/;
    }


    {
        local $Message = "Final Sigma";

        my $SIGMA = "\x{03A3}"; # CAPITAL
        my $Sigma = "\x{03C2}"; # SMALL FINAL
        my $sigma = "\x{03C3}"; # SMALL

        ok $SIGMA =~ /$SIGMA/i;
        ok $SIGMA =~ /$Sigma/i;
        ok $SIGMA =~ /$sigma/i;

        ok $Sigma =~ /$SIGMA/i;
        ok $Sigma =~ /$Sigma/i;
        ok $Sigma =~ /$sigma/i;

        ok $sigma =~ /$SIGMA/i;
        ok $sigma =~ /$Sigma/i;
        ok $sigma =~ /$sigma/i;

        ok $SIGMA =~ /[$SIGMA]/i;
        ok $SIGMA =~ /[$Sigma]/i;
        ok $SIGMA =~ /[$sigma]/i;

        ok $Sigma =~ /[$SIGMA]/i;
        ok $Sigma =~ /[$Sigma]/i;
        ok $Sigma =~ /[$sigma]/i;

        ok $sigma =~ /[$SIGMA]/i;
        ok $sigma =~ /[$Sigma]/i;
        ok $sigma =~ /[$sigma]/i;

        local $Message = "More final Sigma";

        my $S3 = "$SIGMA$Sigma$sigma";

        ok ":$S3:" =~ /:(($SIGMA)+):/i   && $1 eq $S3 && $2 eq $sigma;
        ok ":$S3:" =~ /:(($Sigma)+):/i   && $1 eq $S3 && $2 eq $sigma;
        ok ":$S3:" =~ /:(($sigma)+):/i   && $1 eq $S3 && $2 eq $sigma;

        ok ":$S3:" =~ /:(([$SIGMA])+):/i && $1 eq $S3 && $2 eq $sigma;
        ok ":$S3:" =~ /:(([$Sigma])+):/i && $1 eq $S3 && $2 eq $sigma;
        ok ":$S3:" =~ /:(([$sigma])+):/i && $1 eq $S3 && $2 eq $sigma;
    }


    {
        use charnames ':full';
        local $Message = "Parlez-Vous " .
                         "Fran\N{LATIN SMALL LETTER C WITH CEDILLA}ais?";

        ok "Fran\N{LATIN SMALL LETTER C}ais" =~ /Fran.ais/ &&
            $& eq "Francais";
        ok "Fran\N{LATIN SMALL LETTER C WITH CEDILLA}ais" =~ /Fran.ais/ &&
            $& eq "Fran\N{LATIN SMALL LETTER C WITH CEDILLA}ais";
        ok "Fran\N{LATIN SMALL LETTER C}ais" =~ /Fran\Cais/ &&
            $& eq "Francais";
        # COMBINING CEDILLA is two bytes when encoded
        ok "Franc\N{COMBINING CEDILLA}ais" =~ /Franc\C\Cais/;
        ok "Fran\N{LATIN SMALL LETTER C}ais" =~ /Fran\Xais/ &&
            $& eq "Francais";
        ok "Fran\N{LATIN SMALL LETTER C WITH CEDILLA}ais" =~ /Fran\Xais/  &&
            $& eq "Fran\N{LATIN SMALL LETTER C WITH CEDILLA}ais";
        ok "Franc\N{COMBINING CEDILLA}ais" =~ /Fran\Xais/ &&
            $& eq "Franc\N{COMBINING CEDILLA}ais";
        ok "Fran\N{LATIN SMALL LETTER C WITH CEDILLA}ais" =~
           /Fran\N{LATIN SMALL LETTER C WITH CEDILLA}ais/  &&
            $& eq "Fran\N{LATIN SMALL LETTER C WITH CEDILLA}ais";
        ok "Franc\N{COMBINING CEDILLA}ais" =~ /Franc\N{COMBINING CEDILLA}ais/ &&
            $& eq "Franc\N{COMBINING CEDILLA}ais";

        my @f = (
            ["Fran\N{LATIN SMALL LETTER C}ais",                    "Francais"],
            ["Fran\N{LATIN SMALL LETTER C WITH CEDILLA}ais",
                               "Fran\N{LATIN SMALL LETTER C WITH CEDILLA}ais"],
            ["Franc\N{COMBINING CEDILLA}ais", "Franc\N{COMBINING CEDILLA}ais"],
        );
        foreach my $entry (@f) {
            my ($subject, $match) = @$entry;
            ok $subject =~ /Fran(?:c\N{COMBINING CEDILLA}?|
                    \N{LATIN SMALL LETTER C WITH CEDILLA})ais/x &&
               $& eq $match;
        }
    }


    {
        local $Message = "Lingering (and useless) UTF8 flag doesn't mess up /i";
        my $pat = "ABcde";
        my $str = "abcDE\x{100}";
        chop $str;
        ok $str =~ /$pat/i;

        $pat = "ABcde\x{100}";
        $str = "abcDE";
        chop $pat;
        ok $str =~ /$pat/i;

        $pat = "ABcde\x{100}";
        $str = "abcDE\x{100}";
        chop $pat;
        chop $str;
        ok $str =~ /$pat/i;
    }


    {
        use charnames ':full';
        local $Message = "LATIN SMALL LETTER SHARP S " .
                         "(\N{LATIN SMALL LETTER SHARP S})";

        ok "\N{LATIN SMALL LETTER SHARP S}" =~
                                            /\N{LATIN SMALL LETTER SHARP S}/;
        ok "\N{LATIN SMALL LETTER SHARP S}" =~
                                            /\N{LATIN SMALL LETTER SHARP S}/i;
        ok "\N{LATIN SMALL LETTER SHARP S}" =~
                                           /[\N{LATIN SMALL LETTER SHARP S}]/;
        ok "\N{LATIN SMALL LETTER SHARP S}" =~
                                           /[\N{LATIN SMALL LETTER SHARP S}]/i;

        ok "ss" =~  /\N{LATIN SMALL LETTER SHARP S}/i;
        ok "SS" =~  /\N{LATIN SMALL LETTER SHARP S}/i;
        ok "ss" =~ /[\N{LATIN SMALL LETTER SHARP S}]/i;
        ok "SS" =~ /[\N{LATIN SMALL LETTER SHARP S}]/i;

        ok "\N{LATIN SMALL LETTER SHARP S}" =~ /ss/i;
        ok "\N{LATIN SMALL LETTER SHARP S}" =~ /SS/i;

        local $Message = "Unoptimized named sequence in class";
        ok "ss" =~ /[\N{LATIN SMALL LETTER SHARP S}x]/i;
        ok "SS" =~ /[\N{LATIN SMALL LETTER SHARP S}x]/i;
        ok "\N{LATIN SMALL LETTER SHARP S}" =~
          /[\N{LATIN SMALL LETTER SHARP S}x]/;
        ok "\N{LATIN SMALL LETTER SHARP S}" =~
          /[\N{LATIN SMALL LETTER SHARP S}x]/i;
    }


    {
        # More whitespace: U+0085, U+2028, U+2029\n";

        # U+0085, U+00A0 need to be forced to be Unicode, the \x{100} does that.
      SKIP: {
          skip "EBCDIC platform", 4 if $IS_EBCDIC;
          # Do \x{0015} and \x{0041} match \s in EBCDIC?
          ok "<\x{100}\x{0085}>" =~ /<\x{100}\s>/, '\x{0085} in \s';
          ok        "<\x{0085}>" =~        /<\v>/, '\x{0085} in \v';
          ok "<\x{100}\x{00A0}>" =~ /<\x{100}\s>/, '\x{00A0} in \s';
          ok        "<\x{00A0}>" =~        /<\h>/, '\x{00A0} in \h';
        }
        my @h = map {sprintf "%05x" => $_} 0x01680, 0x0180E, 0x02000 .. 0x0200A,
                                           0x0202F, 0x0205F, 0x03000;
        my @v = map {sprintf "%05x" => $_} 0x02028, 0x02029;

        my @H = map {sprintf "%05x" => $_} 0x01361,   0x0200B, 0x02408, 0x02420,
                                           0x0303F,   0xE0020;
        my @V = map {sprintf "%05x" => $_} 0x0008A .. 0x0008D, 0x00348, 0x10100,
                                           0xE005F,   0xE007C;

        for my $hex (@h) {
            my $str = eval qq ["<\\x{$hex}>"];
            ok $str =~ /<\s>/, "\\x{$hex} in \\s";
            ok $str =~ /<\h>/, "\\x{$hex} in \\h";
            ok $str !~ /<\v>/, "\\x{$hex} not in \\v";
        }

        for my $hex (@v) {
            my $str = eval qq ["<\\x{$hex}>"];
            ok $str =~ /<\s>/, "\\x{$hex} in \\s";
            ok $str =~ /<\v>/, "\\x{$hex} in \\v";
            ok $str !~ /<\h>/, "\\x{$hex} not in \\h";
        }

        for my $hex (@H) {
            my $str = eval qq ["<\\x{$hex}>"];
            ok $str =~ /<\S>/, "\\x{$hex} in \\S";
            ok $str =~ /<\H>/, "\\x{$hex} in \\H";
        }

        for my $hex (@V) {
            my $str = eval qq ["<\\x{$hex}>"];
            ok $str =~ /<\S>/, "\\x{$hex} in \\S";
            ok $str =~ /<\V>/, "\\x{$hex} in \\V";
        }
    }


    {
        # . with /s should work on characters, as opposed to bytes
        local $Message = ". with /s works on characters, not bytes";

        my $s = "\x{e4}\x{100}";
        # This is not expected to match: the point is that
        # neither should we get "Malformed UTF-8" warnings.
        may_not_warn sub {$s =~ /\G(.+?)\n/gcs}, "No 'Malformed UTF-8' warning";

        my @c;
        push @c => $1 while $s =~ /\G(.)/gs;

        local $" = "";
        iseq "@c", $s;

        # Test only chars < 256
        my $t1 = "Q003\n\n\x{e4}\x{f6}\n\nQ004\n\n\x{e7}";
        my $r1 = "";
        while ($t1 =~ / \G ( .+? ) \n\s+ ( .+? ) ( $ | \n\s+ ) /xgcs) {
        $r1 .= $1 . $2;
        }

        my $t2 = $t1 . "\x{100}"; # Repeat with a larger char
        my $r2 = "";
        while ($t2 =~ / \G ( .+? ) \n\s+ ( .+? ) ( $ | \n\s+ ) /xgcs) {
        $r2 .= $1 . $2;
        }
        $r2 =~ s/\x{100}//;

        iseq $r1, $r2;
    }


    {
        local $Message = "Unicode lookbehind";
        ok "A\x{100}B"        =~ /(?<=A.)B/;
        ok "A\x{200}\x{300}B" =~ /(?<=A..)B/;
        ok "\x{400}AB"        =~ /(?<=\x{400}.)B/;
        ok "\x{500}\x{600}B"  =~ /(?<=\x{500}.)B/;

        # Original code also contained:
        # ok "\x{500\x{600}}B"  =~ /(?<=\x{500}.)B/;
        # but that looks like a typo.
    }


    {
        local $Message = 'UTF-8 hash keys and /$/';
        # http://www.xray.mpe.mpg.de/mailing-lists/perl5-porters
        #                                         /2002-01/msg01327.html

        my $u = "a\x{100}";
        my $v = substr ($u, 0, 1);
        my $w = substr ($u, 1, 1);
        my %u = ($u => $u, $v => $v, $w => $w);
        for (keys %u) {
            my $m1 =            /^\w*$/ ? 1 : 0;
            my $m2 = $u {$_} =~ /^\w*$/ ? 1 : 0;
            iseq $m1, $m2;
        }
    }


    {
        local $Message = "No SEGV in s/// and UTF-8";
        my $s = "s#\x{100}" x 4;
        ok $s =~ s/[^\w]/ /g;
        if ( 1 or $ENV{PERL_TEST_LEGACY_POSIX_CC} ) {
            iseq $s, "s \x{100}" x 4;
        }
        else {
            iseq $s, "s  " x 4;
        }
    }


    {
        local $Message = "UTF-8 bug (maybe already known?)";
        my $u = "foo";
        $u =~ s/./\x{100}/g;
        iseq $u, "\x{100}\x{100}\x{100}";

        $u = "foobar";
        $u =~ s/[ao]/\x{100}/g;
        iseq $u, "f\x{100}\x{100}b\x{100}r";

        $u =~ s/\x{100}/e/g;
        iseq $u, "feeber";
    }


    {
        local $Message = "UTF-8 bug with s///";
        # check utf8/non-utf8 mixtures
        # try to force all float/anchored check combinations

        my $c = "\x{100}";
        my $subst;
        for my $re ("xx.*$c", "x.*$c$c", "$c.*xx", "$c$c.*x",
                    "xx.*(?=$c)", "(?=$c).*xx",) {
            ok "xxx" !~ /$re/;
            ok +($subst = "xxx") !~ s/$re//;
        }
        for my $re ("xx.*$c*", "$c*.*xx") {
            ok "xxx" =~ /$re/;
            ok +($subst = "xxx") =~ s/$re//;
            iseq $subst, "";
        }
        for my $re ("xxy*", "y*xx") {
            ok "xx$c" =~ /$re/;
            ok +($subst = "xx$c") =~ s/$re//;
            iseq $subst, $c;
            ok "xy$c" !~ /$re/;
            ok +($subst = "xy$c") !~ s/$re//;
        }
        for my $re ("xy$c*z", "x$c*yz") {
            ok "xyz" =~ /$re/;
            ok +($subst = "xyz") =~ s/$re//;
            iseq $subst, "";
        }
    }


    {
        local $Message = "qr /.../x";
        my $R = qr / A B C # D E/x;
        ok "ABCDE" =~    $R   && $& eq "ABC";
        ok "ABCDE" =~   /$R/  && $& eq "ABC";
        ok "ABCDE" =~  m/$R/  && $& eq "ABC";
        ok "ABCDE" =~  /($R)/ && $1 eq "ABC";
        ok "ABCDE" =~ m/($R)/ && $1 eq "ABC";
    }




    {
        local $\;
        $_ = 'aaaaaaaaaa';
        utf8::upgrade($_); chop $_; $\="\n";
        ok /[^\s]+/, 'm/[^\s]/ utf8';
        ok /[^\d]+/, 'm/[^\d]/ utf8';
        ok +($a = $_, $_ =~ s/[^\s]+/./g), 's/[^\s]/ utf8';
        ok +($a = $_, $a =~ s/[^\d]+/./g), 's/[^\s]/ utf8';
    }




    {
        # Subject: Odd regexp behavior
        # From: Markus Kuhn <Markus.Kuhn@cl.cam.ac.uk>
        # Date: Wed, 26 Feb 2003 16:53:12 +0000
        # Message-Id: <E18o4nw-0008Ly-00@wisbech.cl.cam.ac.uk>
        # To: perl-unicode@perl.org

        local $Message = 'Markus Kuhn 2003-02-26';

        my $x = "\x{2019}\nk";
        ok $x =~ s/(\S)\n(\S)/$1 $2/sg;
        ok $x eq "\x{2019} k";

        $x = "b\nk";
        ok $x =~ s/(\S)\n(\S)/$1 $2/sg;
        ok $x eq "b k";

        ok "\x{2019}" =~ /\S/;
    }


    {
        # XXX DAPM 13-Apr-06. Recursive split is still broken. It's only luck it
        # hasn't been crashing. Disable this test until it is fixed properly.
        # XXX also check what it returns rather than just doing ok(1,...)
        # split /(?{ split "" })/, "abc";
        local $TODO = "Recursive split is still broken";
        ok 0, 'cache_re & "(?{": it dumps core in 5.6.1 & 5.8.0';
    }


    {
        ok "\x{100}\n" =~ /\x{100}\n$/, "UTF-8 length cache and fbm_compile";
    }


    {
        package Str;
        use overload q /""/ => sub {${$_ [0]};};
        sub new {my ($c, $v) = @_; bless \$v, $c;}

        package main;
        $_ = Str -> new ("a\x{100}/\x{100}b");
        ok join (":", /\b(.)\x{100}/g) eq "a:/", "re_intuit_start and PL_bostr";
    }

    {
        my $re = qq /^([^X]*)X/;
        utf8::upgrade ($re);
        ok "\x{100}X" =~ /$re/, "S_cl_and ANYOF_UNICODE & ANYOF_INVERTED";
    }

    {
        ok "123\x{100}" =~ /^.*1.*23\x{100}$/,
           'UTF-8 + multiple floating substr';
    }

    {
        local $Message = '<20030808193656.5109.1@llama.ni-s.u-net.com>';

        # LATIN SMALL/CAPITAL LETTER A WITH MACRON
        ok "  \x{101}" =~ qr/\x{100}/i;

        # LATIN SMALL/CAPITAL LETTER A WITH RING BELOW
        ok "  \x{1E01}" =~ qr/\x{1E00}/i;

        # DESERET SMALL/CAPITAL LETTER LONG I
        ok "  \x{10428}" =~ qr/\x{10400}/i;

        # LATIN SMALL/CAPITAL LETTER A WITH RING BELOW + 'X'
        ok "  \x{1E01}x" =~ qr/\x{1E00}X/i;
    }

    {
        for (120 .. 130) {
            my $head = 'x' x $_;
            local $Message = q [Don't misparse \x{...} in regexp ] .
                             q [near 127 char EXACT limit];
            for my $tail ('\x{0061}', '\x{1234}', '\x61') {
                eval_ok qq ["$head$tail" =~ /$head$tail/];
            }
            local $Message = q [Don't misparse \N{...} in regexp ] .
                             q [near 127 char EXACT limit];
            for my $tail ('\N{SNOWFLAKE}') {
                eval_ok qq [use charnames ':full';
                           "$head$tail" =~ /$head$tail/];
            }
        }
    }

    {   # TRIE related
        our @got = ();
        "words" =~ /(word|word|word)(?{push @got, $1})s$/;
        iseq @got, 1, "TRIE optimation";

        @got = ();
        "words" =~ /(word|word|word)(?{push @got,$1})s$/i;
        iseq @got, 1,"TRIEF optimisation";

        my @nums = map {int rand 1000} 1 .. 100;
        my $re = "(" . (join "|", @nums) . ")";
        $re = qr/\b$re\b/;

        foreach (@nums) {
            ok $_ =~ /$re/, "Trie nums";
        }

        $_ = join " ", @nums;
        @got = ();
        push @got, $1 while /$re/g;

        my %count;
        $count {$_} ++ for @got;
        my $ok = 1;
        for (@nums) {
            $ok = 0 if --$count {$_} < 0;
        }
        ok $ok, "Trie min count matches";
    }


    {
        # TRIE related
        # LATIN SMALL/CAPITAL LETTER A WITH MACRON
        ok "foba  \x{101}foo" =~ qr/(foo|\x{100}foo|bar)/i &&
           $1 eq "\x{101}foo",
           "TRIEF + LATIN SMALL/CAPITAL LETTER A WITH MACRON";

        # LATIN SMALL/CAPITAL LETTER A WITH RING BELOW
        ok "foba  \x{1E01}foo" =~ qr/(foo|\x{1E00}foo|bar)/i &&
           $1 eq "\x{1E01}foo",
           "TRIEF + LATIN SMALL/CAPITAL LETTER A WITH RING BELOW";

        # DESERET SMALL/CAPITAL LETTER LONG I
        ok "foba  \x{10428}foo" =~ qr/(foo|\x{10400}foo|bar)/i &&
           $1 eq "\x{10428}foo",
           "TRIEF + DESERET SMALL/CAPITAL LETTER LONG I";

        # LATIN SMALL/CAPITAL LETTER A WITH RING BELOW + 'X'
        ok "foba  \x{1E01}xfoo" =~ qr/(foo|\x{1E00}Xfoo|bar)/i &&
           $1 eq "\x{1E01}xfoo",
           "TRIEF + LATIN SMALL/CAPITAL LETTER A WITH RING BELOW + 'X'";

        use charnames ':full';

        my $s = "\N{LATIN SMALL LETTER SHARP S}";
        ok "foba  ba$s" =~ qr/(foo|Ba$s|bar)/i &&  $1 eq "ba$s",
           "TRIEF + LATIN SMALL LETTER SHARP S =~ ss";
        ok "foba  ba$s" =~ qr/(Ba$s|foo|bar)/i &&  $1 eq "ba$s",
           "TRIEF + LATIN SMALL LETTER SHARP S =~ ss";
        ok "foba  ba$s" =~ qr/(foo|bar|Ba$s)/i &&  $1 eq "ba$s",
           "TRIEF + LATIN SMALL LETTER SHARP S =~ ss";

        ok "foba  ba$s" =~ qr/(foo|Bass|bar)/i &&  $1 eq "ba$s",
           "TRIEF + LATIN SMALL LETTER SHARP S =~ ss";

        ok "foba  ba$s" =~ qr/(foo|BaSS|bar)/i &&  $1 eq "ba$s",
           "TRIEF + LATIN SMALL LETTER SHARP S =~ SS";

        ok "foba  ba${s}pxySS$s$s" =~ qr/(b(?:a${s}t|a${s}f|a${s}p)[xy]+$s*)/i
            &&  $1 eq "ba${s}pxySS$s$s",
           "COMMON PREFIX TRIEF + LATIN SMALL LETTER SHARP S";
    }




    {
    BEGIN {
        unshift @INC, 'lib';
    }
        use Cname;

        ok 'fooB'  =~ /\N{foo}[\N{B}\N{b}]/, "Passthrough charname";
        #
        # Why doesn't must_warn work here?
        #
        my $w;
        local $SIG {__WARN__} = sub {$w .= "@_"};
        eval 'q(xxWxx) =~ /[\N{WARN}]/';
        ok $w && $w =~ /Using just the first character returned by \\N{} in character class/,
                 "single character in [\\N{}] warning";

        undef $w;
        eval q [ok "\0" !~ /[\N{EMPTY-STR}XY]/,
                   "Zerolength charname in charclass doesn't match \\0"];
        ok $w && $w =~ /Ignoring zero length/,
                 'Ignoring zero length \N{} in character class warning';

        ok 'AB'  =~ /(\N{EVIL})/ && $1 eq 'A', 'Charname caching $1';
        ok 'ABC' =~ /(\N{EVIL})/,              'Charname caching $1';
        ok 'xy'  =~ /x\N{EMPTY-STR}y/,
                    'Empty string charname produces NOTHING node';
        ok ''    =~ /\N{EMPTY-STR}/,
                    'Empty string charname produces NOTHING node';
        ok "\N{LONG-STR}" =~ /^\N{LONG-STR}$/, 'Verify that long string works';
        ok "\N{LONG-STR}" =~ /^\N{LONG-STR}$/i, 'Verify under folding that long string works';

        # If remove the limitation in regcomp code these should work
        # differently
        undef $w;
        eval q [ok "\N{LONG-STR}" =~ /^\N{TOO-LONG-STR}$/, 'Verify that too long a string fails gracefully'];
        ok $w && $w =~ /Using just the first characters returned/, 'Verify that got too-long string warning in \N{} that exceeds the limit';
        undef $w;
        eval q [ok "\N{LONG-STR}" =~ /^\N{TOO-LONG-STR}$/i, 'Verify under folding that too long a string fails gracefully'];
        ok $w && $w =~ /Using just the first characters returned/, 'Verify under folding that got too-long string warning in \N{} that exceeds the limit';
        undef $w;
        eval q [ok "\N{TOO-LONG-STR}" !~ /^\N{TOO-LONG-STR}$/, 'Verify that too long a string doesnt work'];
        ok $w && $w =~ /Using just the first characters returned/, 'Verify that got too-long string warning in \N{} that exceeds the limit';
        undef $w;
        eval q [ok "\N{TOO-LONG-STR}" !~ /^\N{TOO-LONG-STR}$/i, 'Verify under folding that too long a string doesnt work'];
        ok $w && $w =~ /Using just the first characters returned/i, 'Verify under folding that got too-long string warning in \N{} that exceeds the limit';
        undef $w;
        eval 'q(syntax error) =~ /\N{MALFORMED}/';
        ok $@ && $@ =~ /Malformed/, 'Verify that malformed utf8 gives an error';
        undef $w;
        eval 'q() =~ /\N{4F}/';
        ok $w && $w =~ /Deprecated/, 'Verify that leading digit in name gives warning';
        undef $w;
        eval 'q() =~ /\N{COM,MA}/';
        ok $w && $w =~ /Deprecated/, 'Verify that comma in name gives warning';
        undef $w;
        my $name = "A\x{D7}O";
        eval "q(W) =~ /\\N{$name}/";
        ok $w && $w =~ /Deprecated/, 'Verify that latin1 symbol in name gives warning';
        undef $w;
        $name = "A\x{D1}O";
        eval "q(W) =~ /\\N{$name}/";
        ok ! $w, 'Verify that latin1 letter in name doesnt give warning';

    }


    {
        use charnames ':full';

        ok 'aabc' !~ /a\N{PLUS SIGN}b/, '/a\N{PLUS SIGN}b/ against aabc';
        ok 'a+bc' =~ /a\N{PLUS SIGN}b/, '/a\N{PLUS SIGN}b/ against a+bc';

        ok ' A B' =~ /\N{SPACE}\N{U+0041}\N{SPACE}\N{U+0042}/,
            'Intermixed named and unicode escapes';
        ok "\N{SPACE}\N{U+0041}\N{SPACE}\N{U+0042}" =~
           /\N{SPACE}\N{U+0041}\N{SPACE}\N{U+0042}/,
            'Intermixed named and unicode escapes';
        ok "\N{SPACE}\N{U+0041}\N{SPACE}\N{U+0042}" =~
           /[\N{SPACE}\N{U+0041}][\N{SPACE}\N{U+0042}]/,
            'Intermixed named and unicode escapes';
        ok "\0" =~ /^\N{NULL}$/, 'Verify that \N{NULL} works; is not confused with an error';
    }


    {
        our $brackets;
        $brackets = qr{
            {  (?> [^{}]+ | (??{ $brackets }) )* }
        }x;

        ok "{b{c}d" !~ m/^((??{ $brackets }))/, "Bracket mismatch";

        SKIP: {
            our @stack = ();
            my @expect = qw(
                stuff1
                stuff2
                <stuff1>and<stuff2>
                right
                <right>
                <<right>>
                <<<right>>>
                <<stuff1>and<stuff2>><<<<right>>>>
            );

            local $_ = '<<<stuff1>and<stuff2>><<<<right>>>>>';
            ok /^(<((?:(?>[^<>]+)|(?1))*)>(?{push @stack, $2 }))$/,
                "Recursion matches";
            iseq @stack, @expect, "Right amount of matches"
                 or skip "Won't test individual results as count isn't equal",
                          0 + @expect;
            my $idx = 0;
            foreach my $expect (@expect) {
                iseq $stack [$idx], $expect,
                    "Expecting '$expect' at stack pos #$idx";
                $idx ++;
            }
        }
    }


    {
        my $s = '123453456';
        $s =~ s/(?<digits>\d+)\k<digits>/$+{digits}/;
        ok $s eq '123456', 'Named capture (angle brackets) s///';
        $s = '123453456';
        $s =~ s/(?'digits'\d+)\k'digits'/$+{digits}/;
        ok $s eq '123456', 'Named capture (single quotes) s///';
    }


    {
        my @ary = (
            pack('U', 0x00F1),            # n-tilde
            '_'.pack('U', 0x00F1),        # _ + n-tilde
            'c'.pack('U', 0x0327),        # c + cedilla
            pack('U*', 0x00F1, 0x0327),   # n-tilde + cedilla
            pack('U', 0x0391),            # ALPHA
            pack('U', 0x0391).'2',        # ALPHA + 2
            pack('U', 0x0391).'_',        # ALPHA + _
        );

        for my $uni (@ary) {
            my ($r1, $c1, $r2, $c2) = eval qq {
                use utf8;
                scalar ("..foo foo.." =~ /(?'${uni}'foo) \\k'${uni}'/),
                        \$+{${uni}},
                scalar ("..bar bar.." =~ /(?<${uni}>bar) \\k<${uni}>/),
                        \$+{${uni}};
            };
            ok $r1,                         "Named capture UTF (?'')";
            ok defined $c1 && $c1 eq 'foo', "Named capture UTF \%+";
            ok $r2,                         "Named capture UTF (?<>)";
            ok defined $c2 && $c2 eq 'bar', "Named capture UTF \%+";
        }
    }

    {
        my $s = 'foo bar baz';
        my @res;
        if ('1234' =~ /(?<A>1)(?<B>2)(?<A>3)(?<B>4)/) {
            foreach my $name (sort keys(%-)) {
                my $ary = $- {$name};
                foreach my $idx (0 .. $#$ary) {
                    push @res, "$name:$idx:$ary->[$idx]";
                }
            }
        }
        my @expect = qw (A:0:1 A:1:3 B:0:2 B:1:4);
        iseq "@res", "@expect", "Check %-";
        eval'
            no warnings "uninitialized";
            print for $- {this_key_doesnt_exist};
        ';
        ok !$@,'lvalue $- {...} should not throw an exception';
    }

    {
        # \, breaks {3,4}
        ok "xaaay"    !~ /xa{3\,4}y/, '\, in a pattern';
        ok "xa{3,4}y" =~ /xa{3\,4}y/, '\, in a pattern';

        # \c\ followed by _
        ok "x\c_y"    !~ /x\c\_y/,    '\_ in a pattern';
        ok "x\c\_y"   =~ /x\c\_y/,    '\_ in a pattern';

        # \c\ followed by other characters
        for my $c ("z", "\0", "!", chr(254), chr(256)) {
            my $targ = "a\034$c";
            my $reg  = "a\\c\\$c";
            ok eval ("qq/$targ/ =~ /$reg/"), "\\c\\ in pattern";
        }
    }

    {   # Test the (*PRUNE) pattern
        our $count = 0;
        'aaab' =~ /a+b?(?{$count++})(*FAIL)/;
        iseq $count, 9, "Expect 9 for no (*PRUNE)";
        $count = 0;
        'aaab' =~ /a+b?(*PRUNE)(?{$count++})(*FAIL)/;
        iseq $count, 3, "Expect 3 with (*PRUNE)";
        local $_ = 'aaab';
        $count = 0;
        1 while /.(*PRUNE)(?{$count++})(*FAIL)/g;
        iseq $count, 4, "/.(*PRUNE)/";
        $count = 0;
        'aaab' =~ /a+b?(??{'(*PRUNE)'})(?{$count++})(*FAIL)/;
        iseq $count, 3, "Expect 3 with (*PRUNE)";
        local $_ = 'aaab';
        $count = 0;
        1 while /.(??{'(*PRUNE)'})(?{$count++})(*FAIL)/g;
        iseq $count, 4, "/.(*PRUNE)/";
    }


    {   # Test the (*SKIP) pattern
        our $count = 0;
        'aaab' =~ /a+b?(*SKIP)(?{$count++})(*FAIL)/;
        iseq $count, 1, "Expect 1 with (*SKIP)";
        local $_ = 'aaab';
        $count = 0;
        1 while /.(*SKIP)(?{$count++})(*FAIL)/g;
        iseq $count, 4, "/.(*SKIP)/";
        $_ = 'aaabaaab';
        $count = 0;
        our @res = ();
        1 while /(a+b?)(*SKIP)(?{$count++; push @res,$1})(*FAIL)/g;
        iseq $count, 2, "Expect 2 with (*SKIP)";
        iseq "@res", "aaab aaab", "Adjacent (*SKIP) works as expected";
    }


    {   # Test the (*SKIP) pattern
        our $count = 0;
        'aaab' =~ /a+b?(*MARK:foo)(*SKIP)(?{$count++})(*FAIL)/;
        iseq $count, 1, "Expect 1 with (*SKIP)";
        local $_ = 'aaab';
        $count = 0;
        1 while /.(*MARK:foo)(*SKIP)(?{$count++})(*FAIL)/g;
        iseq $count, 4, "/.(*SKIP)/";
        $_ = 'aaabaaab';
        $count = 0;
        our @res = ();
        1 while /(a+b?)(*MARK:foo)(*SKIP)(?{$count++; push @res,$1})(*FAIL)/g;
        iseq $count, 2, "Expect 2 with (*SKIP)";
        iseq "@res", "aaab aaab", "Adjacent (*SKIP) works as expected";
    }


    {   # Test the (*SKIP) pattern
        our $count = 0;
        'aaab' =~ /a*(*MARK:a)b?(*MARK:b)(*SKIP:a)(?{$count++})(*FAIL)/;
        iseq $count, 3, "Expect 3 with *MARK:a)b?(*MARK:b)(*SKIP:a)";
        local $_ = 'aaabaaab';
        $count = 0;
        our @res = ();
        1 while
        /(a*(*MARK:a)b?)(*MARK:x)(*SKIP:a)(?{$count++; push @res,$1})(*FAIL)/g;
        iseq $count, 5, "Expect 5 with (*MARK:a)b?)(*MARK:x)(*SKIP:a)";
        iseq "@res", "aaab b aaab b ",
             "Adjacent (*MARK:a)b?)(*MARK:x)(*SKIP:a) works as expected";
    }


    {   # Test the (*COMMIT) pattern
        our $count = 0;
        'aaabaaab' =~ /a+b?(*COMMIT)(?{$count++})(*FAIL)/;
        iseq $count, 1, "Expect 1 with (*COMMIT)";
        local $_ = 'aaab';
        $count = 0;
        1 while /.(*COMMIT)(?{$count++})(*FAIL)/g;
        iseq $count, 1, "/.(*COMMIT)/";
        $_ = 'aaabaaab';
        $count = 0;
        our @res = ();
        1 while /(a+b?)(*COMMIT)(?{$count++; push @res,$1})(*FAIL)/g;
        iseq $count, 1, "Expect 1 with (*COMMIT)";
        iseq "@res", "aaab", "Adjacent (*COMMIT) works as expected";
    }


    {
        # Test named commits and the $REGERROR var
        our $REGERROR;
        for my $name ('', ':foo') {
            for my $pat ("(*PRUNE$name)",
                         ($name ? "(*MARK$name)" : "") . "(*SKIP$name)",
                         "(*COMMIT$name)") {
                for my $suffix ('(*FAIL)', '') {
                    'aaaab' =~ /a+b$pat$suffix/;
                    iseq $REGERROR,
                         ($suffix ? ($name ? 'foo' : "1") : ""),
                        "Test $pat and \$REGERROR $suffix";
                }
            }
        }
    }


    {
        # Test named commits and the $REGERROR var
        package Fnorble;
        our $REGERROR;
        for my $name ('', ':foo') {
            for my $pat ("(*PRUNE$name)",
                         ($name ? "(*MARK$name)" : "") . "(*SKIP$name)",
                         "(*COMMIT$name)") {
                for my $suffix ('(*FAIL)','') {
                    'aaaab' =~ /a+b$pat$suffix/;
                  ::iseq $REGERROR,
                         ($suffix ? ($name ? 'foo' : "1") : ""),
                        "Test $pat and \$REGERROR $suffix";
                }
            }
        }
    }


    {
        # Test named commits and the $REGERROR var
        local $Message = '$REGERROR';
        our $REGERROR;
        for my $word (qw (bar baz bop)) {
            $REGERROR = "";
            "aaaaa$word" =~
              /a+(?:bar(*COMMIT:bar)|baz(*COMMIT:baz)|bop(*COMMIT:bop))(*FAIL)/;
            iseq $REGERROR, $word;
        }
    }

    {
        #Mindnumbingly simple test of (*THEN)
        for ("ABC","BAX") {
            ok /A (*THEN) X | B (*THEN) C/x, "Simple (*THEN) test";
        }
    }


    {
        local $Message = "Relative Recursion";
        my $parens = qr/(\((?:[^()]++|(?-1))*+\))/;
        local $_ = 'foo((2*3)+4-3) + bar(2*(3+4)-1*(2-3))';
        my ($all, $one, $two) = ('', '', '');
        ok m/foo $parens \s* \+ \s* bar $parens/x;
        iseq $1, '((2*3)+4-3)';
        iseq $2, '(2*(3+4)-1*(2-3))';
        iseq $&, 'foo((2*3)+4-3) + bar(2*(3+4)-1*(2-3))';
        iseq $&, $_;
    }

    {
        my $spaces="      ";
        local $_ = join 'bar', $spaces, $spaces;
        our $count = 0;
        s/(?>\s+bar)(?{$count++})//g;
        iseq $_, $spaces, "SUSPEND final string";
        iseq $count, 1, "Optimiser should have prevented more than one match";
    }


    {
        # From Message-ID: <877ixs6oa6.fsf@k75.linux.bogus>
        my $dow_name = "nada";
        my $parser = "(\$dow_name) = \$time_string =~ /(D\x{e9}\\ " .
                     "C\x{e9}adaoin|D\x{e9}\\ Sathairn|\\w+|\x{100})/";
        my $time_string = "D\x{e9} C\x{e9}adaoin";
        eval $parser;
        ok !$@, "Test Eval worked";
        iseq $dow_name, $time_string, "UTF-8 trie common prefix extraction";
    }


    {
        my $v;
        ($v = 'bar') =~ /(\w+)/g;
        $v = 'foo';
        iseq "$1", 'bar', '$1 is safe after /g - may fail due ' .
                          'to specialized config in pp_hot.c'
    }


    {
        local $Message = "http://nntp.perl.org/group/perl.perl5.porters/118663";
        my $qr_barR1 = qr/(bar)\g-1/;
        ok "foobarbarxyz" =~ $qr_barR1;
        ok "foobarbarxyz" =~ qr/foo${qr_barR1}xyz/;
        ok "foobarbarxyz" =~ qr/(foo)${qr_barR1}xyz/;
        ok "foobarbarxyz" =~ qr/(foo)(bar)\g{-1}xyz/;
        ok "foobarbarxyz" =~ qr/(foo${qr_barR1})xyz/;
        ok "foobarbarxyz" =~ qr/(foo(bar)\g{-1})xyz/;
    }

    {
        local $Message = '$REGMARK';
        our @r = ();
        our ($REGMARK, $REGERROR);
        ok 'foofoo' =~ /foo (*MARK:foo) (?{push @r,$REGMARK}) /x;
        iseq "@r","foo";
        iseq $REGMARK, "foo";
        ok 'foofoo' !~ /foo (*MARK:foo) (*FAIL) /x;
        ok !$REGMARK;
        iseq $REGERROR, 'foo';
    }


    {
        local $Message = '\K test';
        my $x;
        $x = "abc.def.ghi.jkl";
        $x =~ s/.*\K\..*//;
        iseq $x, "abc.def.ghi";

        $x = "one two three four";
        $x =~ s/o+ \Kthree//g;
        iseq $x, "one two  four";

        $x = "abcde";
        $x =~ s/(.)\K/$1/g;
        iseq $x, "aabbccddee";
    }


    {
        sub kt {
            return '4' if $_[0] eq '09028623';
        }
        # Nested EVAL using PL_curpm (via $1 or friends)
        my $re;
        our $grabit = qr/ ([0-6][0-9]{7}) (??{ kt $1 }) [890] /x;
        $re = qr/^ ( (??{ $grabit }) ) $ /x;
        my @res = '0902862349' =~ $re;
        iseq join ("-", @res), "0902862349",
            'PL_curpm is set properly on nested eval';

        our $qr = qr/ (o) (??{ $1 }) /x;
        ok 'boob'=~/( b (??{ $qr }) b )/x && 1, "PL_curpm, nested eval";
    }


    {
        use charnames ":full";
        ok "\N{ROMAN NUMERAL ONE}" =~ /\p{Alphabetic}/, "I =~ Alphabetic";
        ok "\N{ROMAN NUMERAL ONE}" =~ /\p{Uppercase}/,  "I =~ Uppercase";
        ok "\N{ROMAN NUMERAL ONE}" !~ /\p{Lowercase}/,  "I !~ Lowercase";
        ok "\N{ROMAN NUMERAL ONE}" =~ /\p{IDStart}/,    "I =~ ID_Start";
        ok "\N{ROMAN NUMERAL ONE}" =~ /\p{IDContinue}/, "I =~ ID_Continue";
        ok "\N{SMALL ROMAN NUMERAL ONE}" =~ /\p{Alphabetic}/, "i =~ Alphabetic";
        ok "\N{SMALL ROMAN NUMERAL ONE}" !~ /\p{Uppercase}/,  "i !~ Uppercase";
        ok "\N{SMALL ROMAN NUMERAL ONE}" =~ /\p{Lowercase}/,  "i =~ Lowercase";
        ok "\N{SMALL ROMAN NUMERAL ONE}" =~ /\p{IDStart}/,    "i =~ ID_Start";
        ok "\N{SMALL ROMAN NUMERAL ONE}" =~ /\p{IDContinue}/, "i =~ ID_Continue"
    }


    {
        # requirement of Unicode Technical Standard #18, 1.7 Code Points
        # cf. http://www.unicode.org/reports/tr18/#Supplementary_Characters
        for my $u (0x7FF, 0x800, 0xFFFF, 0x10000) {
            no warnings 'utf8'; # oops
            my $c = chr $u;
            my $x = sprintf '%04X', $u;
            ok "A${c}B" =~ /A[\0-\x{10000}]B/, "Unicode range - $x";
        }
    }


    {
        my $res="";

        if ('1' =~ /(?|(?<digit>1)|(?<digit>2))/) {
            $res = "@{$- {digit}}";
        }
        iseq $res, "1",
            "Check that (?|...) doesnt cause dupe entries in the names array";

        $res = "";
        if ('11' =~ /(?|(?<digit>1)|(?<digit>2))(?&digit)/) {
            $res = "@{$- {digit}}";
        }
        iseq $res, "1", "Check that (?&..) to a buffer inside " .
                        "a (?|...) goes to the leftmost";
    }


    {
        use warnings;
        local $Message = "ASCII pattern that really is UTF-8";
        my @w;
        local $SIG {__WARN__} = sub {push @w, "@_"};
        my $c = qq (\x{DF});
        ok $c =~ /${c}|\x{100}/;
        ok @w == 0;
    }


    {
        local $Message = "Corruption of match results of qr// across scopes";
        my $qr = qr/(fo+)(ba+r)/;
        'foobar' =~ /$qr/;
        iseq "$1$2", "foobar";
        {
            'foooooobaaaaar' =~ /$qr/;
            iseq "$1$2", 'foooooobaaaaar';
        }
        iseq "$1$2", "foobar";
    }


    {
        local $Message = "HORIZWS";
        local $_ = "\t \r\n \n \t".chr(11)."\n";
        s/\H/H/g;
        s/\h/h/g;
        iseq $_, "hhHHhHhhHH";
        $_ = "\t \r\n \n \t" . chr (11) . "\n";
        utf8::upgrade ($_);
        s/\H/H/g;
        s/\h/h/g;
        iseq $_, "hhHHhHhhHH";
    }


    {
        local $Message = "Various whitespace special patterns";
        my @h = map {chr $_}   0x09,   0x20,   0xa0, 0x1680, 0x180e, 0x2000,
                             0x2001, 0x2002, 0x2003, 0x2004, 0x2005, 0x2006,
                             0x2007, 0x2008, 0x2009, 0x200a, 0x202f, 0x205f,
                             0x3000;
        my @v = map {chr $_}   0x0a,   0x0b,   0x0c,   0x0d,   0x85, 0x2028,
                             0x2029;
        my @lb = ("\x0D\x0A", map {chr $_} 0x0A .. 0x0D, 0x85, 0x2028, 0x2029);
        foreach my $t ([\@h,  qr/\h/, qr/\h+/],
                       [\@v,  qr/\v/, qr/\v+/],
                       [\@lb, qr/\R/, qr/\R+/],) {
            my $ary = shift @$t;
            foreach my $pat (@$t) {
                foreach my $str (@$ary) {
                    ok $str =~ /($pat)/, $pat;
                    iseq $1, $str, $pat;
                    utf8::upgrade ($str);
                    ok $str =~ /($pat)/, "Upgraded string - $pat";
                    iseq $1, $str, "Upgraded string - $pat";
                }
            }
        }
    }


    {
        local $Message = "Check that \\xDF match properly in its various forms";
        # Test that \xDF matches properly. this is pretty hacky stuff,
        # but its actually needed. The malarky with '-' is to prevent
        # compilation caching from playing any role in the test.
        my @df = (chr (0xDF), '-', chr (0xDF));
        utf8::upgrade ($df [2]);
        my @strs = ('ss', 'sS', 'Ss', 'SS', chr (0xDF));
        my @ss = map {("$_", "$_")} @strs;
        utf8::upgrade ($ss [$_ * 2 + 1]) for 0 .. $#strs;

        for my $ssi (0 .. $#ss) {
            for my $dfi (0 .. $#df) {
                my $pat = $df [$dfi];
                my $str = $ss [$ssi];
                my $utf_df = ($dfi > 1) ? 'utf8' : '';
                my $utf_ss = ($ssi % 2) ? 'utf8' : '';
                (my $sstr = $str) =~ s/\xDF/\\xDF/;

                if ($utf_df || $utf_ss || length ($ss [$ssi]) == 1) {
                    my $ret = $str =~ /$pat/i;
                    next if $pat eq '-';
                    ok $ret, "\"$sstr\" =~ /\\xDF/i " .
                             "(str is @{[$utf_ss||'latin']}, pat is " .
                             "@{[$utf_df||'latin']})";
                }
                else {
                    my $ret = $str !~ /$pat/i;
                    next if $pat eq '-';
                    ok $ret, "\"$sstr\" !~ /\\xDF/i " .
                             "(str is @{[$utf_ss||'latin']}, pat is " .
                             "@{[$utf_df||'latin']})";
                }
            }
        }
    }


    {
        local $Message = "BBC(Bleadperl Breaks CPAN) Today: String::Multibyte";
        my $re  = qr/(?:[\x00-\xFF]{4})/;
        my $hyp = "\0\0\0-";
        my $esc = "\0\0\0\\";

        my $str = "$esc$hyp$hyp$esc$esc";
        my @a = ($str =~ /\G(?:\Q$esc$esc\E|\Q$esc$hyp\E|$re)/g);

        iseq @a,3;
        local $" = "=";
        iseq "@a","$esc$hyp=$hyp=$esc$esc";
    }


    {
        # Test for keys in %+ and %-
        local $Message = 'Test keys in %+ and %-';
        no warnings 'uninitialized';
        my $_ = "abcdef";
        /(?<foo>a)|(?<foo>b)/;
        iseq ((join ",", sort keys %+), "foo");
        iseq ((join ",", sort keys %-), "foo");
        iseq ((join ",", sort values %+), "a");
        iseq ((join ",", sort map "@$_", values %-), "a ");
        /(?<bar>a)(?<bar>b)(?<quux>.)/;
        iseq ((join ",", sort keys %+), "bar,quux");
        iseq ((join ",", sort keys %-), "bar,quux");
        iseq ((join ",", sort values %+), "a,c"); # leftmost
        iseq ((join ",", sort map "@$_", values %-), "a b,c");
        /(?<un>a)(?<deux>c)?/; # second buffer won't capture
        iseq ((join ",", sort keys %+), "un");
        iseq ((join ",", sort keys %-), "deux,un");
        iseq ((join ",", sort values %+), "a");
        iseq ((join ",", sort map "@$_", values %-), ",a");
    }


    {
        # length() on captures, the numbered ones end up in Perl_magic_len
        my $_ = "aoeu \xe6var ook";
        /^ \w+ \s (?<eek>\S+)/x;

        iseq length ($`),      0, q[length $`];
        iseq length ($'),      4, q[length $'];
        iseq length ($&),      9, q[length $&];
        iseq length ($1),      4, q[length $1];
        iseq length ($+{eek}), 4, q[length $+{eek} == length $1];
    }


    {
        my $ok = -1;

        $ok = exists ($-{x}) ? 1 : 0 if 'bar' =~ /(?<x>foo)|bar/;
        iseq $ok, 1, '$-{x} exists after "bar"=~/(?<x>foo)|bar/';
        iseq scalar (%+), 0, 'scalar %+ == 0 after "bar"=~/(?<x>foo)|bar/';
        iseq scalar (%-), 1, 'scalar %- == 1 after "bar"=~/(?<x>foo)|bar/';

        $ok = -1;
        $ok = exists ($+{x}) ? 1 : 0 if 'bar' =~ /(?<x>foo)|bar/;
        iseq $ok, 0, '$+{x} not exists after "bar"=~/(?<x>foo)|bar/';
        iseq scalar (%+), 0, 'scalar %+ == 0 after "bar"=~/(?<x>foo)|bar/';
        iseq scalar (%-), 1, 'scalar %- == 1 after "bar"=~/(?<x>foo)|bar/';

        $ok = -1;
        $ok = exists ($-{x}) ? 1 : 0 if 'foo' =~ /(?<x>foo)|bar/;
        iseq $ok, 1, '$-{x} exists after "foo"=~/(?<x>foo)|bar/';
        iseq scalar (%+), 1, 'scalar %+ == 1 after "foo"=~/(?<x>foo)|bar/';
        iseq scalar (%-), 1, 'scalar %- == 1 after "foo"=~/(?<x>foo)|bar/';

        $ok = -1;
        $ok = exists ($+{x}) ? 1 : 0 if 'foo'=~/(?<x>foo)|bar/;
        iseq $ok, 1, '$+{x} exists after "foo"=~/(?<x>foo)|bar/';
    }


    {
        local $_;
        ($_ = 'abc') =~ /(abc)/g;
        $_ = '123';
        iseq "$1", 'abc', "/g leads to unsafe match vars: $1";
    }


    {
        local $Message = 'Message-ID: <20070818091501.7eff4831@r2d2>';
        my $str = "";
        for (0 .. 5) {
            my @x;
            $str .= "@x"; # this should ALWAYS be the empty string
            'a' =~ /(a|)/;
            push @x, 1;
        }
        iseq length ($str), 0, "Trie scope error, string should be empty";
        $str = "";
        my @foo = ('a') x 5;
        for (@foo) {
            my @bar;
            $str .= "@bar";
            s/a|/push @bar, 1/e;
        }
        iseq length ($str), 0, "Trie scope error, string should be empty";
    }


    {
# more TRIE/AHOCORASICK problems with mixed utf8 / latin-1 and case folding
    for my $chr (160 .. 255) {
        my $chr_byte = chr($chr);
        my $chr_utf8 = chr($chr); utf8::upgrade($chr_utf8);
        my $rx = qr{$chr_byte|X}i;
        ok($chr_utf8 =~ $rx, "utf8/latin, codepoint $chr");
    }
    }

    {
        our $a = 3; "" =~ /(??{ $a })/;
        our $b = $a;
        iseq $b, $a, "Copy of scalar used for postponed subexpression";
    }


    {
        our @ctl_n = ();
        our @plus = ();
        our $nested_tags;
        $nested_tags = qr{
            <
               (\w+)
               (?{
                       push @ctl_n,$^N;
                       push @plus,$+;
               })
            >
            (??{$nested_tags})*
            </\s* \w+ \s*>
        }x;

        my $match = '<bla><blubb></blubb></bla>' =~ m/^$nested_tags$/;
        ok $match, 'nested construct matches';
        iseq "@ctl_n", "bla blubb", '$^N inside of (?{}) works as expected';
        iseq "@plus",  "bla blubb", '$+  inside of (?{}) works as expected';
    }


    SKIP: {
        # XXX: This set of tests is essentially broken, POSIX character classes
        # should not have differing definitions under Unicode.
        # There are property names for that.
        skip "Tests assume ASCII", 4 unless $IS_ASCII;

        my @notIsPunct = grep {/[[:punct:]]/ and not /\p{IsPunct}/}
                                map {chr} 0x20 .. 0x7f;
        iseq join ('', @notIsPunct), '$+<=>^`|~',
            '[:punct:] disagress with IsPunct on Symbols';

        my @isPrint = grep {not /[[:print:]]/ and /\p{IsPrint}/}
                            map {chr} 0 .. 0x1f, 0x7f .. 0x9f;
        iseq join ('', @isPrint), "",
            'IsPrint agrees with [:print:] on control characters';

        my @isPunct = grep {/[[:punct:]]/ != /\p{IsPunct}/}
                            map {chr} 0x80 .. 0xff;
        iseq join ('', @isPunct), "\xa1\xab\xb7\xbb\xbf",    # ¡ « · » ¿
            'IsPunct disagrees with [:punct:] outside ASCII';

        my @isPunctLatin1 = eval q {
            use encoding 'latin1';
            grep {/[[:punct:]]/ != /\p{IsPunct}/} map {chr} 0x80 .. 0xff;
        };
        skip "Eval failed ($@)", 1 if $@;
        skip "PERL_LEGACY_UNICODE_CHARCLASS_MAPPINGS set to 0", 1
              if !$ENV{PERL_TEST_LEGACY_POSIX_CC};
        iseq join ('', @isPunctLatin1), '',
            'IsPunct agrees with [:punct:] with explicit Latin1';
    }

    #
    # Keep the following tests last -- they may crash perl
    #
    print "# Tests that follow may crash perl\n";
    {
        eval '/\k/';
        ok $@ =~ /\QSequence \k... not terminated in regex;\E/,
           'Lone \k not allowed';
    }

    {
        local $Message = "Substitution with lookahead (possible segv)";
        $_ = "ns1ns1ns1";
        s/ns(?=\d)/ns_/g;
        iseq $_, "ns_1ns_1ns_1";
        $_ = "ns1";
        s/ns(?=\d)/ns_/;
        iseq $_, "ns_1";
        $_ = "123";
        s/(?=\d+)|(?<=\d)/!Bang!/g;
        iseq $_, "!Bang!1!Bang!2!Bang!3!Bang!";
    }

    { 
        # Earlier versions of Perl said this was fatal.
        local $Message = "U+0FFFF shouldn't crash the regex engine";
        no warnings 'utf8';
        my $a = eval "chr(65535)";
        use warnings;
        my $warning_message;
        local $SIG{__WARN__} = sub { $warning_message = $_[0] };
        eval $a =~ /[a-z]/;
        ok(1);  # If it didn't crash, it worked.
    }
} # End of sub run_tests

1;
