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


plan tests => 2511;  # Update this when adding/deleting tests.

run_tests() unless caller;

#
# Tests start here.
#
sub run_tests {


    {
        local $BugId = '20000731.001';
        ok "A \x{263a} B z C" =~ /A . B (??{ "z" }) C/,
           "Match UTF-8 char in presense of (??{ })";
    }


    {
        local $BugId = '20001021.005';
        no warnings 'uninitialized';
        ok undef =~ /^([^\/]*)(.*)$/, "Used to cause a SEGV";
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
        local $BugId   = '20001028.003';

        # Fist half of the bug.
        local $Message = 'HEBREW ACCENT QADMA matched by .*';
        my $X = chr (1448);
        ok my ($Y) = $X =~ /(.*)/;
        iseq $Y, v1448;
        iseq length ($Y), 1;

        # Second half of the bug.
        $Message = 'HEBREW ACCENT QADMA in replacement';
        $X = '';
        $X =~ s/^/chr(1488)/e;
        iseq length $X, 1;
        iseq ord ($X), 1488;
    }


    {   
        local $BugId   = '20001108.001';
        local $Message = 'Repeated s///';
        my $X = "Szab\x{f3},Bal\x{e1}zs";
        my $Y = $X;
        $Y =~ s/(B)/$1/ for 0 .. 3;
        iseq $Y, $X;
        iseq $X, "Szab\x{f3},Bal\x{e1}zs";
    }


    {
        local $BugId   = '20000517.001';
        local $Message = 's/// on UTF-8 string';
        my $x = "\x{100}A";
        $x =~ s/A/B/;
        iseq $x, "\x{100}B";
        iseq length $x, 2;
    }


    {
        local $BugId   = '20001230.002';
        local $Message = '\C and É';
        ok "École" =~ /^\C\C(.)/ && $1 eq 'c';
        ok "École" =~ /^\C\C(c)/;
    }


    {
        # The original bug report had 'no utf8' here but that was irrelevant.
        local $BugId   = '20010306.008';
        local $Message = "Don't dump core";
        my $a = "a\x{1234}";
        ok $a =~ m/\w/;  # used to core dump.
    }


    {
        local $BugId = '20010410.006';
        local $Message = '/g in scalar context';
        for my $rx ('/(.*?)\{(.*?)\}/csg',
		    '/(.*?)\{(.*?)\}/cg',
		    '/(.*?)\{(.*?)\}/sg',
		    '/(.*?)\{(.*?)\}/g',
		    '/(.+?)\{(.+?)\}/csg',) {
            my $i = 0;
            my $input = "a{b}c{d}";
            eval <<"            --";
                while (eval \$input =~ $rx) {
                    \$i ++;
                }
            --
            iseq $i, 2;
        }
    }

    {
        local $BugId = "20010619.003";
        # Amazingly vertical tabulator is the same in ASCII and EBCDIC.
        for ("\n", "\t", "\014", "\r") {
            ok !/[[:print:]]/, "'$_' not in [[:print:]]";
        }
        for (" ") {
            ok  /[[:print:]]/, "'$_' in [[:print:]]";
        }
    }



    {
        # [ID 20010814.004] pos() doesn't work when using =~m// in list context
        local $BugId = '20010814.004';
        $_ = "ababacadaea";
        my $a = join ":", /b./gc;
        my $b = join ":", /a./gc;
        my $c = pos;
        iseq "$a $b $c", 'ba:ba ad:ae 10', "pos() works with () = m//";
    }


    {
        # [ID 20010407.006] matching utf8 return values from
        # functions does not work
        local $BugId   = '20010407.006';
        local $Message = 'UTF-8 return values from functions';
        package ID_20010407_006;
        sub x {"a\x{1234}"}
        my $x = x;
        my $y;
      ::ok $x =~ /(..)/;
        $y = $1;
      ::ok length ($y) == 2 && $y eq $x;
      ::ok x =~ /(..)/;
        $y = $1;
      ::ok length ($y) == 2 && $y eq $x;
    }

    {
        # High bit bug -- japhy
        my $x = "ab\200d";
        ok $x =~ /.*?\200/, "High bit fine";
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
        local $BugId   = "20020124.005";
        local $PatchId = "14795";
        local $Message = "s///eg";

        for my $char ("a", "\x{df}", "\x{100}") {
            my $x = "$char b $char";
            $x =~ s{($char)}{
                  "c" =~ /c/;
                  "x";
            }ge;
            iseq substr ($x, 0, 1), substr ($x, -1, 1);
        }
    }


    {
        local $BugId = "20020412.005";
        local $Message = "Correct pmop flags checked when empty pattern";

        # Requires reuse of last successful pattern.
        my $num = 123;
        $num =~ /\d/;
        for (0 .. 1) {
            my $match = ?? + 0;
            ok $match != $_, $Message, 
                sprintf "'match one' %s on %s iteration" =>
                               $match ? 'succeeded' : 'failed',
                               $_     ? 'second'    : 'first';
        }
        $num =~ /(\d)/;
        my $result = join "" => $num =~ //g;
        iseq $result, $num;
    }


    {
        local $BugId   = '20020630.002';
        local $Message = 'UTF-8 regex matches above 32k';
        for (['byte', "\x{ff}"], ['utf8', "\x{1ff}"]) {
            my ($type, $char) = @$_;
            for my $len (32000, 32768, 33000) {
                my  $s = $char . "f" x $len;
                my  $r = $s =~ /$char([f]*)/gc;
                ok  $r, $Message, "<$type x $len>";
                ok !$r || pos ($s) == $len + 1, $Message,
                        "<$type x $len>; pos = @{[pos $s]}";
            }
        }
    }

    {
        local $PatchId = '18179';
        my $s = "\x{100}" x 5;
        my $ok = $s =~ /(\x{100}{4})/;
        my ($ord, $len) = (ord $1, length $1);
        ok $ok && $ord == 0x100 && $len == 4, "No panic: end_shift";
    }


    {
        local $BugId = '15763';
        our $a = "x\x{100}";
        chop $a;    # Leaves the UTF-8 flag
        $a .= "y";  # 1 byte before 'y'.

        ok $a =~ /^\C/,        'match one \C on 1-byte UTF-8';
        ok $a =~ /^\C{1}/,     'match \C{1}';

        ok $a =~ /^\Cy/,       'match \Cy';
        ok $a =~ /^\C{1}y/,    'match \C{1}y';

        ok $a !~ /^\C\Cy/,     q {don't match two \Cy};
        ok $a !~ /^\C{2}y/,    q {don't match \C{2}y};

        $a = "\x{100}y"; # 2 bytes before "y"

        ok $a =~ /^\C/,        'match one \C on 2-byte UTF-8';
        ok $a =~ /^\C{1}/,     'match \C{1}';
        ok $a =~ /^\C\C/,      'match two \C';
        ok $a =~ /^\C{2}/,     'match \C{2}';

        ok $a =~ /^\C\C\C/,    'match three \C on 2-byte UTF-8 and a byte';
        ok $a =~ /^\C{3}/,     'match \C{3}';

        ok $a =~ /^\C\Cy/,     'match two \C';
        ok $a =~ /^\C{2}y/,    'match \C{2}';

        ok $a !~ /^\C\C\Cy/,   q {don't match three \Cy};
        ok $a !~ /^\C{2}\Cy/,  q {don't match \C{2}\Cy};
        ok $a !~ /^\C{3}y/,    q {don't match \C{3}y};

        $a = "\x{1000}y"; # 3 bytes before "y"

        ok $a =~ /^\C/,        'match one \C on three-byte UTF-8';
        ok $a =~ /^\C{1}/,     'match \C{1}';
        ok $a =~ /^\C\C/,      'match two \C';
        ok $a =~ /^\C{2}/,     'match \C{2}';
        ok $a =~ /^\C\C\C/,    'match three \C';
        ok $a =~ /^\C{3}/,     'match \C{3}';

        ok $a =~ /^\C\C\C\C/,  'match four \C on three-byte UTF-8 and a byte';
        ok $a =~ /^\C{4}/,     'match \C{4}';

        ok $a =~ /^\C\C\Cy/,   'match three \Cy';
        ok $a =~ /^\C{3}y/,    'match \C{3}y';

        ok $a !~ /^\C\C\C\Cy/, q {don't match four \Cy};
        ok $a !~ /^\C{4}y/,    q {don't match \C{4}y};
    }

    
    {
        local $BugId   = '15397';
        local $Message = 'UTF-8 matching';
        ok "\x{100}" =~ /\x{100}/;
        ok "\x{100}" =~ /(\x{100})/;
        ok "\x{100}" =~ /(\x{100}){1}/;
        ok "\x{100}\x{100}" =~ /(\x{100}){2}/;
        ok "\x{100}\x{100}" =~ /(\x{100})(\x{100})/;
    }


    {
        local $BugId   = '7471';
        local $Message = 'Neither ()* nor ()*? sets $1 when matched 0 times';
        local $_       = 'CD';
        ok /(AB)*?CD/ && !defined $1;
        ok /(AB)*CD/  && !defined $1;
    }


    {
        local $BugId   = '3547';
        local $Message = "Caching shouldn't prevent match";
        my $pattern = "^(b+?|a){1,2}c";
        ok "bac"    =~ /$pattern/ && $1 eq 'a';
        ok "bbac"   =~ /$pattern/ && $1 eq 'a';
        ok "bbbac"  =~ /$pattern/ && $1 eq 'a';
        ok "bbbbac" =~ /$pattern/ && $1 eq 'a';
    }



    {
        local $BugId   = '18232';
        local $Message = '$1 should keep UTF-8 ness';
        ok "\x{100}" =~ /(.)/;
        iseq  $1, "\x{100}",  '$1 is UTF-8';
        { 'a' =~ /./; }
        iseq  $1, "\x{100}",  '$1 is still UTF-8';
        isneq $1, "\xC4\x80", '$1 is not non-UTF-8';
    }


    {
        local $BugId   = '19767';
        local $Message = "Optimizer doesn't prematurely reject match";
        use utf8;

        my $attr = 'Name-1';
        my $NormalChar      = qr /[\p{IsDigit}\p{IsLower}\p{IsUpper}]/;
        my $NormalWord      = qr /${NormalChar}+?/;
        my $PredNameHyphen  = qr /^${NormalWord}(\-${NormalWord})*?$/;

        $attr =~ /^$/;
        ok $attr =~ $PredNameHyphen;  # Original test.

        "a" =~ m/[b]/;
        ok "0" =~ /\p{N}+\z/;         # Variant.
    }


    {
        local $BugId   = '20683';
        local $Message = "(??{ }) doesn't return stale values";
        our $p = 1;
        foreach (1, 2, 3, 4) {
            $p ++ if /(??{ $p })/
        }
        iseq $p, 5;

        {
            package P;
            $a = 1;
            sub TIESCALAR {bless []}
            sub FETCH     {$a ++}
        }
        tie $p, "P";
        foreach (1, 2, 3, 4) {
            /(??{ $p })/
        }
        iseq $p, 5;
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
        local $BugId = '21411';
        local $Message = "(??{ .. }) in split doesn't corrupt its stack";
        our $i;
        ok '-1-3-5-' eq join '', split /((??{$i++}))/, '-1-3-5-';
        no warnings 'syntax';
        @_ = split /(?{'WOW'})/, 'abc';
        local $" = "|";
        iseq "@_", "a|b|c";
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
        local $BugId = '17757';
        $_ = "code:   'x' { '...' }\n"; study;
        my @x; push @x, $& while m/'[^\']*'/gx;
        local $" = ":";
        iseq "@x", "'x':'...'", "Parse::RecDescent triggered infinite loop";
    }


    {
        local $BugId = '22354';
        sub func ($) {
            ok "a\nb" !~ /^b/,  "Propagated modifier; $_[0]";
            ok "a\nb" =~ /^b/m, "Propagated modifier; $_[0] - with /m";
        }
        func "standalone";
        $_ = "x"; s/x/func "in subst"/e;
        $_ = "x"; s/x/func "in multiline subst"/em;

        #
        # Next two give 'panic: malloc'.
        # Outcommented, using two TODOs.
        #
        local $TODO    = 'panic: malloc';
        local $Message = 'Postponed regexp and propaged modifier';
      # ok 0 for 1 .. 2;
      SKIP: {
            skip "panic: malloc", 2;
            $_ = "x"; /x(?{func "in regexp"})/;
            $_ = "x"; /x(?{func "in multiline regexp"})/m;
        }
    }


    {
        local $BugId = '19049';
        $_    = "abcdef\n";
        my @x = m/./g;
        iseq "abcde", $`, 'Global match sets $`';
    }


    {
        # [perl #23769] Unicode regex broken on simple example
        # regrepeat() didn't handle UTF-8 EXACT case right.
        local $BugId   = '23769';
        my $Mess       = 'regrepeat() handles UTF-8 EXACT case right';
        local $Message = $Mess;

        my $s = "\x{a0}\x{a0}\x{a0}\x{100}"; chop $s;

        ok $s =~ /\x{a0}/;
        ok $s =~ /\x{a0}+/;
        ok $s =~ /\x{a0}\x{a0}/;

        $Message = "$Mess (easy variant)";
        ok "aaa\x{100}" =~ /(a+)/;
        iseq $1, "aaa";

        $Message = "$Mess (easy invariant)";
        ok "aaa\x{100}     " =~ /(a+?)/;
        iseq $1, "a";

        $Message = "$Mess (regrepeat variant)";
        ok "\xa0\xa0\xa0\x{100}    " =~ /(\xa0+?)/;
        iseq $1, "\xa0";

        $Message = "$Mess (regrepeat invariant)";
        ok "\xa0\xa0\xa0\x{100}" =~ /(\xa0+)/;
        iseq $1, "\xa0\xa0\xa0";

        $Message = "$Mess (hard variant)";
        ok "\xa0\xa1\xa0\xa1\xa0\xa1\x{100}" =~ /((?:\xa0\xa1)+?)/;
        iseq $1, "\xa0\xa1";

        $Message = "$Mess (hard invariant)";
        ok "ababab\x{100}  " =~ /((?:ab)+)/;
        iseq $1, 'ababab';

        ok "\xa0\xa1\xa0\xa1\xa0\xa1\x{100}" =~ /((?:\xa0\xa1)+)/;
        iseq $1, "\xa0\xa1\xa0\xa1\xa0\xa1";

        ok "ababab\x{100}  " =~ /((?:ab)+?)/;
        iseq $1, "ab";

        $Message = "Don't match first byte of UTF-8 representation";
        ok "\xc4\xc4\xc4" !~ /(\x{100}+)/;
        ok "\xc4\xc4\xc4" !~ /(\x{100}+?)/;
        ok "\xc4\xc4\xc4" !~ /(\x{100}++)/;
    }


    {
        # perl panic: pp_match start/end pointers
        local $BugId = '25269';
        iseq "a-bc", eval {my ($x, $y) = "bca" =~ /^(?=.*(a)).*(bc)/; "$x-$y"},
             'Captures can move backwards in string';
    }


    {
        local $BugId   = '27940'; # \cA not recognized in character classes
        ok "a\cAb" =~ /\cA/, '\cA in pattern';
        ok "a\cAb" =~ /[\cA]/, '\cA in character class';
        ok "a\cAb" =~ /[\cA-\cB]/, '\cA in character class range';
        ok "abc" =~ /[^\cA-\cB]/, '\cA in negated character class range';
        ok "a\cBb" =~ /[\cA-\cC]/, '\cB in character class range';
        ok "a\cCbc" =~ /[^\cA-\cB]/, '\cC in negated character class range';
        ok "a\cAb" =~ /(??{"\cA"})/, '\cA in ??{} pattern';
        ok "ab" !~ /a\cIb/x, '\cI in pattern';
    }


    {
        # perl #28532: optional zero-width match at end of string is ignored
        local $BugId = '28532';
        ok "abc" =~ /^abc(\z)?/ && defined($1),
           'Optional zero-width match at end of string';
        ok "abc" =~ /^abc(\z)??/ && !defined($1),
           'Optional zero-width match at end of string';
    }



    {
        local $BugId = '36207';
        my $utf8 = "\xe9\x{100}"; chop $utf8;
        my $latin1 = "\xe9";

        ok $utf8 =~ /\xe9/i, "utf8/latin";
        ok $utf8 =~ /$latin1/i, "utf8/latin runtime";
        ok $utf8 =~ /(abc|\xe9)/i, "utf8/latin trie";
        ok $utf8 =~ /(abc|$latin1)/i, "utf8/latin trie runtime";

        ok "\xe9" =~ /$utf8/i, "latin/utf8";
        ok "\xe9" =~ /(abc|$utf8)/i, "latin/utf8 trie";
        ok $latin1 =~ /$utf8/i, "latin/utf8 runtime";
        ok $latin1 =~ /(abc|$utf8)/i, "latin/utf8 trie runtime";
    }


    {
        local $BugId = '37038';
        my $s = "abcd";
        $s =~ /(..)(..)/g;
        $s = $1;
        $s = $2;
        iseq $2, 'cd',
             "Assigning to original string does not corrupt match vars";
    }


    {
        local $PatchId = '26410';
        {
            package wooosh;
            sub gloople {"!"}
        }
        my $aeek = bless {} => 'wooosh';
        eval_ok sub {$aeek -> gloople () =~ /(.)/g},
               "//g match against return value of sub";

        sub gloople {"!"}
        eval_ok sub {gloople () =~ /(.)/g},
               "26410 didn't affect sub calls for some reason";
    }


    {
        local $TODO = "See changes 26925-26928, which reverted change 26410";
        {
            package lv;
            our $var = "abc";
            sub variable : lvalue {$var}
        }
        my $o = bless [] => 'lv';
        my $f = "";
        my $r = eval {
            for (1 .. 2) {
                $f .= $1 if $o -> variable =~ /(.)/g;
            }
            1;
        };
        if ($r) {
            iseq $f, "ab", "pos() retained between calls";
        }
        else {
            local $TODO;
            ok 0, "Code failed: $@";
        }

        our $var = "abc";
        sub variable : lvalue {$var}
        my $g = "";
        my $s = eval {
            for (1 .. 2) {
                $g .= $1 if variable =~ /(.)/g;
            }
            1;
        };
        if ($s) {
            iseq $g, "ab", "pos() retained between calls";
        }
        else {
            local $TODO;
            ok 0, "Code failed: $@";
        }
    }


  SKIP:
    {
        local $BugId = '37836';
        skip "In EBCDIC" if $IS_EBCDIC;
        no warnings 'utf8';
        $_ = pack 'U0C2', 0xa2, 0xf8;  # Ill-formed UTF-8
        my $ret = 0;
        eval_ok sub {!($ret = s/[\0]+//g)},
                "Ill-formed UTF-8 doesn't match NUL in class";
    }


    {
        # chr(65535) should be allowed in regexes
        local $BugId = '38293';
        no warnings 'utf8'; # To allow non-characters
        my ($c, $r, $s);

        $c = chr 0xffff;
        $c =~ s/$c//g;
        ok $c eq "", "U+FFFF, parsed as atom";

        $c = chr 0xffff;
        $r = "\\$c";
        $c =~ s/$r//g;
        ok $c eq "", "U+FFFF backslashed, parsed as atom";

        $c = chr 0xffff;
        $c =~ s/[$c]//g;
        ok $c eq "", "U+FFFF, parsed in class";

        $c = chr 0xffff;
        $r = "[\\$c]";
        $c =~ s/$r//g;
        ok $c eq "", "U+FFFF backslashed, parsed in class";

        $s = "A\x{ffff}B";
        $s =~ s/\x{ffff}//i;
        ok $s eq "AB", "U+FFFF, EXACTF";

        $s = "\x{ffff}A";
        $s =~ s/\bA//;
        ok $s eq "\x{ffff}", "U+FFFF, BOUND";

        $s = "\x{ffff}!";
        $s =~ s/\B!//;
        ok $s eq "\x{ffff}", "U+FFFF, NBOUND";
    }


    {
        local $BugId = '39583';
        
        # The printing characters
        my @chars = ("A" .. "Z");
        my $delim = ",";
        my $size = 32771 - 4;
        my $str = '';

        # Create some random junk. Inefficient, but it works.
        for (my $i = 0; $i < $size; $ i++) {
            $str .= $chars [rand @chars];
        }

        $str .= ($delim x 4);
        my $res;
        my $matched;
        ok $str =~ s/^(.*?)${delim}{4}//s, "Pattern matches";
        iseq $str, "", "Empty string";
        ok defined $1 && length ($1) == $size, '$1 is correct size';
    }


    {
        local $BugId = '27940';
        ok "\0-A"  =~ /\c@-A/, '@- should not be interpolated in a pattern';
        ok "\0\0A" =~ /\c@+A/, '@+ should not be interpolated in a pattern';
        ok "X\@-A"  =~ /X@-A/, '@- should not be interpolated in a pattern';
        ok "X\@\@A" =~ /X@+A/, '@+ should not be interpolated in a pattern';

        ok "X\0A" =~ /X\c@?A/,  '\c@?';
        ok "X\0A" =~ /X\c@*A/,  '\c@*';
        ok "X\0A" =~ /X\c@(A)/, '\c@(';
        ok "X\0A" =~ /X(\c@)A/, '\c@)';
        ok "X\0A" =~ /X\c@|ZA/, '\c@|';

        ok "X\@A" =~ /X@?A/,  '@?';
        ok "X\@A" =~ /X@*A/,  '@*';
        ok "X\@A" =~ /X@(A)/, '@(';
        ok "X\@A" =~ /X(@)A/, '@)';
        ok "X\@A" =~ /X@|ZA/, '@|';

        local $" = ','; # non-whitespace and non-RE-specific
        ok 'abc' =~ /(.)(.)(.)/, 'The last successful match is bogus';
        ok "A@+B"  =~ /A@{+}B/,  'Interpolation of @+ in /@{+}/';
        ok "A@-B"  =~ /A@{-}B/,  'Interpolation of @- in /@{-}/';
        ok "A@+B"  =~ /A@{+}B/x, 'Interpolation of @+ in /@{+}/x';
        ok "A@-B"  =~ /A@{-}B/x, 'Interpolation of @- in /@{-}/x';
    }


    {
        local $BugId = '50496';
        my $s = 'foo bar baz';
        my (@k, @v, @fetch, $res);
        my $count = 0;
        my @names = qw ($+{A} $+{B} $+{C});
        if ($s =~ /(?<A>foo)\s+(?<B>bar)?\s+(?<C>baz)/) {
            while (my ($k, $v) = each (%+)) {
                $count++;
            }
            @k = sort keys   (%+);
            @v = sort values (%+);
            $res = 1;
            push @fetch,
                ["$+{A}", "$1"],
                ["$+{B}", "$2"],
                ["$+{C}", "$3"],
            ;
        } 
        foreach (0 .. 2) {
            if ($fetch [$_]) {
                iseq $fetch [$_] [0], $fetch [$_] [1], $names [$_];
            } else {
                ok 0, $names[$_];
            }
        }
        iseq $res, 1, "'$s' =~ /(?<A>foo)\\s+(?<B>bar)?\\s+(?<C>baz)/";
        iseq $count, 3, "Got 3 keys in %+ via each";
        iseq 0 + @k, 3, 'Got 3 keys in %+ via keys';
        iseq "@k", "A B C", "Got expected keys";
        iseq "@v", "bar baz foo", "Got expected values";
        eval '
            no warnings "uninitialized";
            print for $+ {this_key_doesnt_exist};
        ';
        ok !$@, 'lvalue $+ {...} should not throw an exception';
    }


    {
        #
        # Almost the same as the block above, except that the capture is nested.
        #
        local $BugId = '50496';
        my $s = 'foo bar baz';
        my (@k, @v, @fetch, $res);
        my $count = 0;
        my @names = qw ($+{A} $+{B} $+{C} $+{D});
        if ($s =~ /(?<D>(?<A>foo)\s+(?<B>bar)?\s+(?<C>baz))/) {
            while (my ($k,$v) = each(%+)) {
                $count++;
            }
            @k = sort keys   (%+);
            @v = sort values (%+);
            $res = 1;
            push @fetch,
                ["$+{A}", "$2"],
                ["$+{B}", "$3"],
                ["$+{C}", "$4"],
                ["$+{D}", "$1"],
            ;
        }
        foreach (0 .. 3) {
            if ($fetch [$_]) {
                iseq $fetch [$_] [0], $fetch [$_] [1], $names [$_];
            } else {
                ok 0, $names [$_];
            }
        }
        iseq $res, 1, "'$s' =~ /(?<D>(?<A>foo)\\s+(?<B>bar)?\\s+(?<C>baz))/";
        iseq $count, 4, "Got 4 keys in %+ via each";
        iseq @k, 4, 'Got 4 keys in %+ via keys';
        iseq "@k", "A B C D", "Got expected keys";
        iseq "@v", "bar baz foo foo bar baz", "Got expected values";
        eval '
            no warnings "uninitialized";
            print for $+ {this_key_doesnt_exist};
        ';
        ok !$@,'lvalue $+ {...} should not throw an exception';
    }


    {
        local $BugId = '36046';
        my $str = 'abc'; 
        my $count = 0;
        my $mval = 0;
        my $pval = 0;
        while ($str =~ /b/g) {$mval = $#-; $pval = $#+; $count ++}
        iseq $mval,  0, '@- should be empty';
        iseq $pval,  0, '@+ should be empty';
        iseq $count, 1, 'Should have matched once only';
    }




    {
        local $BugId = '40684';
        local $Message = '/m in precompiled regexp';
        my $s = "abc\ndef";
        my $rex = qr'^abc$'m;
        ok $s =~ m/$rex/;
        ok $s =~ m/^abc$/m;
    }


    {
        local $BugId   = '36909';
        local $Message = '(?: ... )? should not lose $^R';
        $^R = 'Nothing';
        {
            local $^R = "Bad";
            ok 'x foofoo y' =~ m {
                      (foo) # $^R correctly set
                      (?{ "last regexp code result" })
            }x;
            iseq $^R, 'last regexp code result';
        }
        iseq $^R, 'Nothing';

        {
            local $^R = "Bad";

            ok 'x foofoo y' =~ m {
                      (?:foo|bar)+ # $^R correctly set
                      (?{ "last regexp code result" })
            }x;
            iseq $^R, 'last regexp code result';
        }
        iseq $^R, 'Nothing';

        {
            local $^R = "Bad";
            ok 'x foofoo y' =~ m {
                      (foo|bar)\1+ # $^R undefined
                      (?{ "last regexp code result" })
            }x;
            iseq $^R, 'last regexp code result';
        }
        iseq $^R, 'Nothing';

        {
            local $^R = "Bad";
            ok 'x foofoo y' =~ m {
                      (foo|bar)\1 # This time without the +
                      (?{"last regexp code result"})
            }x;
            iseq $^R, 'last regexp code result';
        }
        iseq $^R, 'Nothing';
    }


    {
        local $BugId   = '22395';
        local $Message = 'Match is linear, not quadratic';
        our $count;
        for my $l (10, 100, 1000) {
            $count = 0;
            ('a' x $l) =~ /(.*)(?{$count++})[bc]/;
            local $TODO = "Should be L+1 not L*(L+3)/2 (L=$l)";
            iseq $count, $l + 1;
        }
    }


    {
        local $BugId   = '22614';
        local $Message = '@-/@+ should not have undefined values';
        local $_ = 'ab';
        our @len = ();
        /(.){1,}(?{push @len,0+@-})(.){1,}(?{})^/;
        iseq "@len", "2 2 2";
    }


    {
        local $BugId   = '18209';
        local $Message = '$& set on s///';
        my $text = ' word1 word2 word3 word4 word5 word6 ';

        my @words = ('word1', 'word3', 'word5');
        my $count;
        foreach my $word (@words) {
            $text =~ s/$word\s//gi; # Leave a space to seperate words
                                    # in the resultant str.
            # The following block is not working.
            if ($&) {
                $count ++;
            }
            # End bad block
        }
        iseq $count, 3;
        iseq $text, ' word2 word4 word6 ';
    }


    {
        # RT#6893
        local $BugId = '6893';
        local $_ = qq (A\nB\nC\n); 
        my @res;
        while (m#(\G|\n)([^\n]*)\n#gsx) { 
            push @res, "$2"; 
            last if @res > 3;
        }
        iseq "@res", "A B C", "/g pattern shouldn't infinite loop";
    }



    {
        local $BugId   = '41010';
        local $Message = 'No optimizer bug';
        my @tails  = ('', '(?(1))', '(|)', '()?');    
        my @quants = ('*','+');
        my $doit = sub {
            my $pats = shift;
            for (@_) {
                for my $pat (@$pats) {
                    for my $quant (@quants) {
                        for my $tail (@tails) {
                            my $re = "($pat$quant\$)$tail";
                            ok /$re/  && $1 eq $_, "'$_' =~ /$re/";
                            ok /$re/m && $1 eq $_, "'$_' =~ /$re/m";
                        }
                    }
                }
            }
        };    
        
        my @dpats = ('\d',
                     '[1234567890]',
                     '(1|[23]|4|[56]|[78]|[90])',
                     '(?:1|[23]|4|[56]|[78]|[90])',
                     '(1|2|3|4|5|6|7|8|9|0)',
                     '(?:1|2|3|4|5|6|7|8|9|0)');
        my @spats = ('[ ]', ' ', '( |\t)', '(?: |\t)', '[ \t]', '\s');
        my @sstrs = ('  ');
        my @dstrs = ('12345');
        $doit -> (\@spats, @sstrs);
        $doit -> (\@dpats, @dstrs);
    }



    {
        local $BugId = '45605';
        # [perl #45605] Regexp failure with utf8-flagged and byte-flagged string

        my $utf_8 = "\xd6schel";
        utf8::upgrade ($utf_8);
        $utf_8 =~ m {(\xd6|&Ouml;)schel};
        iseq $1, "\xd6", "Upgrade error";
    }

    {
        # Regardless of utf8ness any character matches itself when 
        # doing a case insensitive match. See also [perl #36207] 
        local $BugId = '36207';
        for my $o (0 .. 255) {
            my @ch = (chr ($o), chr ($o));
            utf8::upgrade ($ch [1]);
            for my $u_str (0, 1) {
                for my $u_pat (0, 1) {
                    ok $ch [$u_str] =~ /\Q$ch[$u_pat]\E/i,
                    "\$c =~ /\$c/i : chr ($o) : u_str = $u_str u_pat = $u_pat";
                    ok $ch [$u_str] =~ /\Q$ch[$u_pat]\E|xyz/i,
                    "\$c=~/\$c|xyz/i : chr($o) : u_str = $u_str u_pat = $u_pat";
                }
            }
        }
    }


    {
         local $BugId   = '49190';
         local $Message = '$REGMARK in replacement';
         our $REGMARK;
         my $_ = "A";
         ok s/(*:B)A/$REGMARK/;
         iseq $_, "B";
         $_ = "CCCCBAA";
         ok s/(*:X)A+|(*:Y)B+|(*:Z)C+/$REGMARK/g;
         iseq $_, "ZYX";
    }


    {
        local $BugId   = '52658';
        local $Message = 'Substitution evaluation in list context';
        my $reg = '../xxx/';
        my @te  = ($reg =~ m{^(/?(?:\.\./)*)},
                   $reg =~ s/(x)/'b'/eg > 1 ? '##' : '++');
        iseq $reg, '../bbb/';
        iseq $te [0], '../';
    }

	# This currently has to come before any "use encoding" in this file.
    {
        local $Message;
        local $BugId   = '59342';
        must_warn 'qr/\400/', '^Use of octal value above 377';
    }



    {
        local $BugId =  '60034';
        my $a = "xyzt" x 8192;
        ok $a =~ /\A(?>[a-z])*\z/,
                '(?>) does not cause wrongness on long string';
        my $b = $a . chr 256;
        chop $b;
        {
            iseq $a, $b;
        }
        ok $b =~ /\A(?>[a-z])*\z/,
           '(?>) does not cause wrongness on long string with UTF-8';
    }


    #
    # Keep the following tests last -- they may crash perl
    #
    print "# Tests that follow may crash perl\n";
    {   
        local $BugId   = '19049/38869';
        local $Message = 'Pattern in a loop, failure should not ' .
                         'affect previous success';
        my @list = (
            'ab cdef',             # Matches regex
            ('e' x 40000 ) .'ab c' # Matches not, but 'ab c' matches part of it
        );
        my $y;
        my $x;
        foreach (@list) {
            m/ab(.+)cd/i; # The ignore-case seems to be important
            $y = $1;      # Use $1, which might not be from the last match!
            $x = substr ($list [0], $- [0], $+ [0] - $- [0]);
        }
        iseq $y, ' ';
        iseq $x, 'ab cd';
    }


    {
        local $BugId = '24274';

        ok (("a" x (2 ** 15 - 10)) =~ /^()(a|bb)*$/, "Recursive stack cracker");
        ok ((q(a)x 100) =~ /^(??{'(.)'x 100})/, 
            "Regexp /^(??{'(.)'x 100})/ crashes older perls");
    }


    {
        # [perl #45337] utf8 + "[a]a{2}" + /$.../ = panic: sv_len_utf8 cache
        local $BugId = '45337';
        local ${^UTF8CACHE} = -1;
        local $Message = "Shouldn't panic";
        my $s = "[a]a{2}";
        utf8::upgrade $s;
        ok "aaa" =~ /$s/;
    }
    {
        local $BugId = '57042';
	local $Message = "Check if tree logic breaks \$^R";
	my $cond_re = qr/\s*
	    \s* (?:
		   \( \s* A  (?{1})
		 | \( \s* B  (?{2})
	       )
	   /x;
	my @res;
	for my $line ("(A)","(B)") {
	   if ($line =~ m/$cond_re/) {
	       push @res, $^R ? "#$^R" : "UNDEF";
	   }
	}
	iseq "@res","#1 #2";
    }
    {
	no warnings 'closure';
	my $re = qr/A(??{"1"})/;
	ok "A1B" =~ m/^((??{ $re }))((??{"B"}))$/;
	ok $1 eq "A1";
	ok $2 eq "B";
    }



    # This only works under -DEBUGGING because it relies on an assert().
    {
        local $BugId = '60508';
	local $Message = "Check capture offset re-entrancy of utf8 code.";

        sub fswash { $_[0] =~ s/([>X])//g; }

        my $k1 = "." x 4 . ">>";
        fswash($k1);

        my $k2 = "\x{f1}\x{2022}";
        $k2 =~ s/([\360-\362])/>/g;
        fswash($k2);

        iseq($k2, "\x{2022}", "utf8::SWASHNEW doesn't cause capture leaks");
    }


    {
	local $BugId = 65372;	# minimal CURLYM limited to 32767 matches
	my @pat = (
	    qr{a(x|y)*b},	# CURLYM
	    qr{a(x|y)*?b},	# .. with minmod
	    qr{a([wx]|[yz])*b},	# .. and without tries
	    qr{a([wx]|[yz])*?b},
	);
	my $len = 32768;
	my $s = join '', 'a', 'x' x $len, 'b';
	for my $pat (@pat) {
	    ok($s =~ $pat, $pat);
	}
    }

    {
        local $TODO = "[perl #38133]";

        "A" =~ /(((?:A))?)+/;
        my $first = $2;

        "A" =~ /(((A))?)+/;
        my $second = $2;

        iseq($first, $second);
    }    
} # End of sub run_tests

1;
