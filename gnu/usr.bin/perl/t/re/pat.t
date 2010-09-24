#!./perl
#
# This is a home for regular expression tests that don't fit into
# the format supported by re/regexp.t.  If you want to add a test
# that does fit that format, add it to re/re_tests, not here.  Tests for \N
# should be added here because they are treated as single quoted strings
# there, which means they avoid the lexer which otherwise would look at them.

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


plan tests => 309;  # Update this when adding/deleting tests.

run_tests() unless caller;

#
# Tests start here.
#
sub run_tests {

    {

        my $x = "abc\ndef\n";

        ok $x =~ /^abc/,  qq ["$x" =~ /^abc/];
        ok $x !~ /^def/,  qq ["$x" !~ /^def/];

        # used to be a test for $*
        ok $x =~ /^def/m, qq ["$x" =~ /^def/m];

        nok $x =~ /^xxx/, qq ["$x" =~ /^xxx/];
        nok $x !~ /^abc/, qq ["$x" !~ /^abc/];

         ok $x =~ /def/, qq ["$x" =~ /def/];
        nok $x !~ /def/, qq ["$x" !~ /def/];

         ok $x !~ /.def/, qq ["$x" !~ /.def/];
        nok $x =~ /.def/, qq ["$x" =~ /.def/];

         ok $x =~ /\ndef/, qq ["$x" =~ /\ndef/];
        nok $x !~ /\ndef/, qq ["$x" !~ /\ndef/];
    }

    {
        $_ = '123';
        ok /^([0-9][0-9]*)/, qq [\$_ = '$_'; /^([0-9][0-9]*)/];
    }

    {
        $_ = 'aaabbbccc';
         ok /(a*b*)(c*)/ && $1 eq 'aaabbb' && $2 eq 'ccc',
                                             qq [\$_ = '$_'; /(a*b*)(c*)/];
         ok /(a+b+c+)/ && $1 eq 'aaabbbccc', qq [\$_ = '$_'; /(a+b+c+)/];
        nok /a+b?c+/,                        qq [\$_ = '$_'; /a+b?c+/];

        $_ = 'aaabccc';
         ok /a+b?c+/, qq [\$_ = '$_'; /a+b?c+/];
         ok /a*b?c*/, qq [\$_ = '$_'; /a*b?c*/];

        $_ = 'aaaccc';
         ok /a*b?c*/, qq [\$_ = '$_'; /a*b?c*/];
        nok /a*b+c*/, qq [\$_ = '$_'; /a*b+c*/];

        $_ = 'abcdef';
         ok /bcd|xyz/, qq [\$_ = '$_'; /bcd|xyz/];
         ok /xyz|bcd/, qq [\$_ = '$_'; /xyz|bcd/];
         ok m|bc/*d|,  qq [\$_ = '$_'; m|bc/*d|];
         ok /^$_$/,    qq [\$_ = '$_'; /^\$_\$/];
    }

    {
        # used to be a test for $*
        ok "ab\ncd\n" =~ /^cd/m, qq ["ab\ncd\n" =~ /^cd/m];
    }

    {
        our %XXX = map {($_ => $_)} 123, 234, 345;

        our @XXX = ('ok 1','not ok 1', 'ok 2','not ok 2','not ok 3');
        while ($_ = shift(@XXX)) {
            my $f = index ($_, 'not') >= 0 ? \&nok : \&ok;
            my $r = ?(.*)?;
            &$f ($r, "?(.*)?");
            /not/ && reset;
            if (/not ok 2/) {
                if ($^O eq 'VMS') {
                    $_ = shift(@XXX);
                }
                else {
                    reset 'X';
                }
            }
        }

        SKIP: {
            if ($^O eq 'VMS') {
                skip "Reset 'X'", 1;
            }
            ok !keys %XXX, "%XXX is empty";
        }

    }

    {
        local $Message = "Test empty pattern";
        my $xyz = 'xyz';
        my $cde = 'cde';

        $cde =~ /[^ab]*/;
        $xyz =~ //;
        iseq $&, $xyz;

        my $foo = '[^ab]*';
        $cde =~ /$foo/;
        $xyz =~ //;
        iseq $&, $xyz;

        $cde =~ /$foo/;
        my $null;
        no warnings 'uninitialized';
        $xyz =~ /$null/;
        iseq $&, $xyz;

        $null = "";
        $xyz =~ /$null/;
        iseq $&, $xyz;
    }

    {
        local $Message = q !Check $`, $&, $'!;
        $_ = 'abcdefghi';
        /def/;        # optimized up to cmd
        iseq "$`:$&:$'", 'abc:def:ghi';

        no warnings 'void';
        /cde/ + 0;    # optimized only to spat
        iseq "$`:$&:$'", 'ab:cde:fghi';

        /[d][e][f]/;    # not optimized
        iseq "$`:$&:$'", 'abc:def:ghi';
    }

    {
        $_ = 'now is the {time for all} good men to come to.';
        / {([^}]*)}/;
        iseq $1, 'time for all', "Match braces";
    }

    {
        local $Message = "{N,M} quantifier";
        $_ = 'xxx {3,4}  yyy   zzz';
        ok /( {3,4})/;
        iseq $1, '   ';
        ok !/( {4,})/;
        ok /( {2,3}.)/;
        iseq $1, '  y';
        ok /(y{2,3}.)/;
        iseq $1, 'yyy ';
        ok !/x {3,4}/;
        ok !/^xxx {3,4}/;
    }

    {
        local $Message = "Test /g";
        local $" = ":";
        $_ = "now is the time for all good men to come to.";
        my @words = /(\w+)/g;
        my $exp   = "now:is:the:time:for:all:good:men:to:come:to";

        iseq "@words", $exp;

        @words = ();
        while (/\w+/g) {
            push (@words, $&);
        }
        iseq "@words", $exp;

        @words = ();
        pos = 0;
        while (/to/g) {
            push(@words, $&);
        }
        iseq "@words", "to:to";

        pos $_ = 0;
        @words = /to/g;
        iseq "@words", "to:to";
    }

    {
        $_ = "abcdefghi";

        my $pat1 = 'def';
        my $pat2 = '^def';
        my $pat3 = '.def.';
        my $pat4 = 'abc';
        my $pat5 = '^abc';
        my $pat6 = 'abc$';
        my $pat7 = 'ghi';
        my $pat8 = '\w*ghi';
        my $pat9 = 'ghi$';

        my $t1 = my $t2 = my $t3 = my $t4 = my $t5 =
        my $t6 = my $t7 = my $t8 = my $t9 = 0;

        for my $iter (1 .. 5) {
            $t1++ if /$pat1/o;
            $t2++ if /$pat2/o;
            $t3++ if /$pat3/o;
            $t4++ if /$pat4/o;
            $t5++ if /$pat5/o;
            $t6++ if /$pat6/o;
            $t7++ if /$pat7/o;
            $t8++ if /$pat8/o;
            $t9++ if /$pat9/o;
        }
        my $x = "$t1$t2$t3$t4$t5$t6$t7$t8$t9";
        iseq $x, '505550555', "Test /o";
    }


    SKIP: {
        my $xyz = 'xyz';
        ok "abc" =~ /^abc$|$xyz/, "| after \$";

        # perl 4.009 says "unmatched ()"
        local $Message = '$ inside ()';

        my $result;
        eval '"abc" =~ /a(bc$)|$xyz/; $result = "$&:$1"';
        iseq $@, "" or skip "eval failed", 1;
        iseq $result, "abc:bc";
    }


    {
        local $Message = "Scalar /g";
        $_ = "abcfooabcbar";

        ok  /abc/g && $` eq "";
        ok  /abc/g && $` eq "abcfoo";
        ok !/abc/g;

        local $Message = "Scalar /gi";
        pos = 0;
        ok  /ABC/gi && $` eq "";
        ok  /ABC/gi && $` eq "abcfoo";
        ok !/ABC/gi;

        local $Message = "Scalar /g";
        pos = 0;
        ok  /abc/g && $' eq "fooabcbar";
        ok  /abc/g && $' eq "bar";

        $_ .= '';
        my @x = /abc/g;
        iseq @x, 2, "/g reset after assignment";
    }

    {
        local $Message = '/g, \G and pos';
        $_ = "abdc";
        pos $_ = 2;
        /\Gc/gc;
        iseq pos $_, 2;
        /\Gc/g;
        ok !defined pos $_;
    }

    {
        local $Message = '(?{ })';
        our $out = 1;
        'abc' =~ m'a(?{ $out = 2 })b';
        iseq $out, 2;

        $out = 1;
        'abc' =~ m'a(?{ $out = 3 })c';
        iseq $out, 1;
    }


    {
        $_ = 'foobar1 bar2 foobar3 barfoobar5 foobar6';
        my @out = /(?<!foo)bar./g;
        iseq "@out", 'bar2 barf', "Negative lookbehind";
    }

    {
        local $Message = "REG_INFTY tests";
        # Tests which depend on REG_INFTY
        $::reg_infty   = $Config {reg_infty} // 32767;
        $::reg_infty_m = $::reg_infty - 1;
        $::reg_infty_p = $::reg_infty + 1;
        $::reg_infty_m = $::reg_infty_m;   # Surpress warning.

        # As well as failing if the pattern matches do unexpected things, the
        # next three tests will fail if you should have picked up a lower-than-
        # default value for $reg_infty from Config.pm, but have not.

        eval_ok q (('aaa' =~ /(a{1,$::reg_infty_m})/)[0] eq 'aaa');
        eval_ok q (('a' x $::reg_infty_m) =~ /a{$::reg_infty_m}/);
        eval_ok q (('a' x ($::reg_infty_m - 1)) !~ /a{$::reg_infty_m}/);
        eval "'aaa' =~ /a{1,$::reg_infty}/";
        ok $@ =~ /^\QQuantifier in {,} bigger than/;
        eval "'aaa' =~ /a{1,$::reg_infty_p}/";
        ok $@ =~ /^\QQuantifier in {,} bigger than/;
    }

    {
        # Poke a couple more parse failures
        my $context = 'x' x 256;
        eval qq("${context}y" =~ /(?<=$context)y/);
        ok $@ =~ /^\QLookbehind longer than 255 not/, "Lookbehind limit";
    }

    {
        # Long Monsters
        local $Message = "Long monster";
        for my $l (125, 140, 250, 270, 300000, 30) { # Ordered to free memory
            my $a = 'a' x $l;
            local $Error = "length = $l";
             ok "ba$a=" =~ /a$a=/;
            nok "b$a="  =~ /a$a=/;
             ok "b$a="  =~ /ba+=/;

             ok "ba$a=" =~ /b(?:a|b)+=/;
        }
    }


    {
        # 20000 nodes, each taking 3 words per string, and 1 per branch
        my $long_constant_len = join '|', 12120 .. 32645;
        my $long_var_len = join '|', 8120 .. 28645;
        my %ans = ( 'ax13876y25677lbc' => 1,
                    'ax13876y25677mcb' => 0, # not b.
                    'ax13876y35677nbc' => 0, # Num too big
                    'ax13876y25677y21378obc' => 1,
                    'ax13876y25677y21378zbc' => 0,    # Not followed by [k-o]
                    'ax13876y25677y21378y21378kbc' => 1,
                    'ax13876y25677y21378y21378kcb' => 0, # Not b.
                    'ax13876y25677y21378y21378y21378kbc' => 0, # 5 runs
                  );

        local $Message = "20000 nodes";
        for (keys %ans) {
            local $Error = "const-len '$_'";
            ok !($ans{$_} xor /a(?=([yx]($long_constant_len)){2,4}[k-o]).*b./o);

            local $Error = "var-len '$_'";
            ok !($ans{$_} xor /a(?=([yx]($long_var_len)){2,4}[k-o]).*b./o);
        }
    }

    {
        local $Message = "Complicated backtracking";
        $_ = " a (bla()) and x(y b((l)u((e))) and b(l(e)e)e";
        my $expect = "(bla()) ((l)u((e))) (l(e)e)";

        use vars '$c';
        sub matchit {
          m/
             (
               \(
               (?{ $c = 1 })    # Initialize
               (?:
                 (?(?{ $c == 0 })   # PREVIOUS iteration was OK, stop the loop
                   (?!
                   )        # Fail: will unwind one iteration back
                 )
                 (?:
                   [^()]+        # Match a big chunk
                   (?=
                     [()]
                   )        # Do not try to match subchunks
                 |
                   \(
                   (?{ ++$c })
                 |
                   \)
                   (?{ --$c })
                 )
               )+        # This may not match with different subblocks
             )
             (?(?{ $c != 0 })
               (?!
               )        # Fail
             )            # Otherwise the chunk 1 may succeed with $c>0
           /xg;
        }

        my @ans = ();
        my $res;
        push @ans, $res while $res = matchit;
        iseq "@ans", "1 1 1";

        @ans = matchit;
        iseq "@ans", $expect;

        local $Message = "Recursion with (??{ })";
        our $matched;
        $matched = qr/\((?:(?>[^()]+)|(??{$matched}))*\)/;

        @ans = my @ans1 = ();
        push (@ans, $res), push (@ans1, $&) while $res = m/$matched/g;

        iseq "@ans", "1 1 1";
        iseq "@ans1", $expect;

        @ans = m/$matched/g;
        iseq "@ans", $expect;

    }

    {
        ok "abc" =~ /^(??{"a"})b/, '"abc" =~ /^(??{"a"})b/';
    }

    {
        my @ans = ('a/b' =~ m%(.*/)?(.*)%);    # Stack may be bad
        iseq "@ans", 'a/ b', "Stack may be bad";
    }

    {
        local $Message = "Eval-group not allowed at runtime";
        my $code = '{$blah = 45}';
        our $blah = 12;
        eval { /(?$code)/ };
        ok $@ && $@ =~ /not allowed at runtime/ && $blah == 12;

        for $code ('{$blah = 45}','=xx') {
            $blah = 12;
            my $res = eval { "xx" =~ /(?$code)/o };
            no warnings 'uninitialized';
            local $Error = "'$@', '$res', '$blah'";
            if ($code eq '=xx') {
                ok !$@ && $res;
            }
            else {
                ok $@ && $@ =~ /not allowed at runtime/ && $blah == 12;
            }
        }

        $code = '{$blah = 45}';
        $blah = 12;
        eval "/(?$code)/";
        iseq $blah, 45;

        $blah = 12;
        /(?{$blah = 45})/;
        iseq $blah, 45;
    }

    {
        local $Message = "Pos checks";
        my $x = 'banana';
        $x =~ /.a/g;
        iseq pos ($x), 2;

        $x =~ /.z/gc;
        iseq pos ($x), 2;

        sub f {
            my $p = $_[0];
            return $p;
        }

        $x =~ /.a/g;
        iseq f (pos ($x)), 4;
    }

    {
        local $Message = 'Checking $^R';
        our $x = $^R = 67;
        'foot' =~ /foo(?{$x = 12; 75})[t]/;
        iseq $^R, 75;

        $x = $^R = 67;
        'foot' =~ /foo(?{$x = 12; 75})[xy]/;
        ok $^R eq '67' && $x eq '12';

        $x = $^R = 67;
        'foot' =~ /foo(?{ $^R + 12 })((?{ $x = 12; $^R + 17 })[xy])?/;
        ok $^R eq '79' && $x eq '12';
    }

    {
        iseq qr/\b\v$/i,    '(?i-xsm:\b\v$)', 'qr/\b\v$/i';
        iseq qr/\b\v$/s,    '(?s-xim:\b\v$)', 'qr/\b\v$/s';
        iseq qr/\b\v$/m,    '(?m-xis:\b\v$)', 'qr/\b\v$/m';
        iseq qr/\b\v$/x,    '(?x-ism:\b\v$)', 'qr/\b\v$/x';
        iseq qr/\b\v$/xism, '(?msix:\b\v$)',  'qr/\b\v$/xism';
        iseq qr/\b\v$/,     '(?-xism:\b\v$)', 'qr/\b\v$/';
    }


    {
        local $Message = "Look around";
        $_ = 'xabcx';
      SKIP:
        foreach my $ans ('', 'c') {
            ok /(?<=(?=a)..)((?=c)|.)/g or skip "Match failed", 1;
            iseq $1, $ans;
        }
    }

    {
        local $Message = "Empty clause";
        $_ = 'a';
        foreach my $ans ('', 'a', '') {
            ok /^|a|$/g or skip "Match failed", 1;
            iseq $&, $ans;
        }
    }

    {
        local $Message = "Prefixify";
        sub prefixify {
            SKIP: {
                my ($v, $a, $b, $res) = @_;
                ok $v =~ s/\Q$a\E/$b/ or skip "Match failed", 1;
                iseq $v, $res;
            }
        }

        prefixify ('/a/b/lib/arch', "/a/b/lib", 'X/lib', 'X/lib/arch');
        prefixify ('/a/b/man/arch', "/a/b/man", 'X/man', 'X/man/arch');
    }

    {
        $_ = 'var="foo"';
        /(\")/;
        ok $1 && /$1/, "Capture a quote";
    }

    {
        no warnings 'closure';
        local $Message = '(?{ $var } refers to package vars';
        package aa;
        our $c = 2;
        $::c = 3;
        '' =~ /(?{ $c = 4 })/;
        main::iseq $c, 4;
        main::iseq $::c, 3;
    }

    {
        must_die 'q(a:[b]:) =~ /[x[:foo:]]/',
                 'POSIX class \[:[^:]+:\] unknown in regex',
                 'POSIX class [: :] must have valid name';

        for my $d (qw [= .]) {
            must_die "/[[${d}foo${d}]]/",
                     "\QPOSIX syntax [$d $d] is reserved for future extensions",
                     "POSIX syntax [[$d $d]] is an error";
        }
    }


    {
        # test if failure of patterns returns empty list
        local $Message = "Failed pattern returns empty list";
        $_ = 'aaa';
        @_ = /bbb/;
        iseq "@_", "";

        @_ = /bbb/g;
        iseq "@_", "";

        @_ = /(bbb)/;
        iseq "@_", "";

        @_ = /(bbb)/g;
        iseq "@_", "";
    }


    {
        local $Message = '@- and @+ tests';

        /a(?=.$)/;
        iseq $#+, 0;
        iseq $#-, 0;
        iseq $+ [0], 2;
        iseq $- [0], 1;
        ok !defined $+ [1] && !defined $- [1] &&
           !defined $+ [2] && !defined $- [2];

        /a(a)(a)/;
        iseq $#+, 2;
        iseq $#-, 2;
        iseq $+ [0], 3;
        iseq $- [0], 0;
        iseq $+ [1], 2;
        iseq $- [1], 1;
        iseq $+ [2], 3;
        iseq $- [2], 2;
        ok !defined $+ [3] && !defined $- [3] &&
           !defined $+ [4] && !defined $- [4];


        /.(a)(b)?(a)/;
        iseq $#+, 3;
        iseq $#-, 3;
        iseq $+ [1], 2;
        iseq $- [1], 1;
        iseq $+ [3], 3;
        iseq $- [3], 2;
        ok !defined $+ [2] && !defined $- [2] &&
           !defined $+ [4] && !defined $- [4];


        /.(a)/;
        iseq $#+, 1;
        iseq $#-, 1;
        iseq $+ [0], 2;
        iseq $- [0], 0;
        iseq $+ [1], 2;
        iseq $- [1], 1;
        ok !defined $+ [2] && !defined $- [2] &&
           !defined $+ [3] && !defined $- [3];

        /.(a)(ba*)?/;
        iseq $#+, 2;
        iseq $#-, 1;
    }


    {
        local $DiePattern = '^Modification of a read-only value attempted';
        local $Message    = 'Elements of @- and @+ are read-only';
        must_die '$+[0] = 13';
        must_die '$-[0] = 13';
        must_die '@+ = (7, 6, 5)';
        must_die '@- = qw (foo bar)';
    }


    {
        local $Message = '\G testing';
        $_ = 'aaa';
        pos = 1;
        my @a = /\Ga/g;
        iseq "@a", "a a";

        my $str = 'abcde';
        pos $str = 2;
        ok $str !~ /^\G/;
        ok $str !~ /^.\G/;
        ok $str =~ /^..\G/;
        ok $str !~ /^...\G/;
        ok $str =~ /\G../ && $& eq 'cd';

        local $TODO = $running_as_thread;
        ok $str =~ /.\G./ && $& eq 'bc';
    }


    {
        local $Message = 'pos inside (?{ })';
        my $str = 'abcde';
        our ($foo, $bar);
        ok $str =~ /b(?{$foo = $_; $bar = pos})c/;
        iseq $foo, $str;
        iseq $bar, 2;
        ok !defined pos ($str);

        undef $foo;
        undef $bar;
        pos $str = undef;
        ok $str =~ /b(?{$foo = $_; $bar = pos})c/g;
        iseq $foo, $str;
        iseq $bar, 2;
        iseq pos ($str), 3;

        $_ = $str;
        undef $foo;
        undef $bar;
        ok /b(?{$foo = $_; $bar = pos})c/;
        iseq $foo, $str;
        iseq $bar, 2;

        undef $foo;
        undef $bar;
        ok /b(?{$foo = $_; $bar = pos})c/g;
        iseq $foo, $str;
        iseq $bar, 2;
        iseq pos, 3;

        undef $foo;
        undef $bar;
        pos = undef;
        1 while /b(?{$foo = $_; $bar = pos})c/g;
        iseq $foo, $str;
        iseq $bar, 2;
        ok !defined pos;

        undef $foo;
        undef $bar;
        $_ = 'abcde|abcde';
        ok s/b(?{$foo = $_; $bar = pos})c/x/g;
        iseq $foo, 'abcde|abcde';
        iseq $bar, 8;
        iseq $_, 'axde|axde';

        # List context:
        $_ = 'abcde|abcde';
        our @res;
        () = /([ace]).(?{push @res, $1,$2})([ce])(?{push @res, $1,$2})/g;
        @res = map {defined $_ ? "'$_'" : 'undef'} @res;
        iseq "@res", "'a' undef 'a' 'c' 'e' undef 'a' undef 'a' 'c'";

        @res = ();
        () = /([ace]).(?{push @res, $`,$&,$'})([ce])(?{push @res, $`,$&,$'})/g;
        @res = map {defined $_ ? "'$_'" : 'undef'} @res;
        iseq "@res", "'' 'ab' 'cde|abcde' " .
                     "'' 'abc' 'de|abcde' " .
                     "'abcd' 'e|' 'abcde' " .
                     "'abcde|' 'ab' 'cde' " .
                     "'abcde|' 'abc' 'de'" ;
    }


    {
        local $Message = '\G anchor checks';
        my $foo = 'aabbccddeeffgg';
        pos ($foo) = 1;
        {
            local $TODO = $running_as_thread;
            no warnings 'uninitialized';
            ok $foo =~ /.\G(..)/g;
            iseq $1, 'ab';

            pos ($foo) += 1;
            ok $foo =~ /.\G(..)/g;
            iseq $1, 'cc';

            pos ($foo) += 1;
            ok $foo =~ /.\G(..)/g;
            iseq $1, 'de';

            ok $foo =~ /\Gef/g;
        }

        undef pos $foo;
        ok $foo =~ /\G(..)/g;
        iseq $1, 'aa';

        ok $foo =~ /\G(..)/g;
        iseq $1, 'bb';

        pos ($foo) = 5;
        ok $foo =~ /\G(..)/g;
        iseq $1, 'cd';
    }


    {
        $_ = '123x123';
        my @res = /(\d*|x)/g;
        local $" = '|';
        iseq "@res", "123||x|123|", "0 match in alternation";
    }


    {
        local $Message = "Match against temporaries (created via pp_helem())" .
                         " is safe";
        ok {foo => "bar\n" . $^X} -> {foo} =~ /^(.*)\n/g;
        iseq $1, "bar";
    }


    {
        local $Message = 'package $i inside (?{ }), ' .
                         'saved substrings and changing $_';
        our @a = qw [foo bar];
        our @b = ();
        s/(\w)(?{push @b, $1})/,$1,/g for @a;
        iseq "@b", "f o o b a r";
        iseq "@a", ",f,,o,,o, ,b,,a,,r,";

        local $Message = 'lexical $i inside (?{ }), ' .
                         'saved substrings and changing $_';
        no warnings 'closure';
        my @c = qw [foo bar];
        my @d = ();
        s/(\w)(?{push @d, $1})/,$1,/g for @c;
        iseq "@d", "f o o b a r";
        iseq "@c", ",f,,o,,o, ,b,,a,,r,";
    }


    {
        local $Message = 'Brackets';
        our $brackets;
        $brackets = qr {
            {  (?> [^{}]+ | (??{ $brackets }) )* }
        }x;

        ok "{{}" =~ $brackets;
        iseq $&, "{}";
        ok "something { long { and } hairy" =~ $brackets;
        iseq $&, "{ and }";
        ok "something { long { and } hairy" =~ m/((??{ $brackets }))/;
        iseq $&, "{ and }";
    }


    {
        $_ = "a-a\nxbb";
        pos = 1;
        nok m/^-.*bb/mg, '$_ = "a-a\nxbb"; m/^-.*bb/mg';
    }


    {
        local $Message = '\G anchor checks';
        my $text = "aaXbXcc";
        pos ($text) = 0;
        ok $text !~ /\GXb*X/g;
    }


    {
        $_ = "xA\n" x 500;
        nok /^\s*A/m, '$_ = "xA\n" x 500; /^\s*A/m"';

        my $text = "abc dbf";
        my @res = ($text =~ /.*?(b).*?\b/g);
        iseq "@res", "b b", '\b is not special';
    }


    {
        local $Message = '\S, [\S], \s, [\s]';
        my @a = map chr, 0 .. 255;
        my @b = grep m/\S/, @a;
        my @c = grep m/[^\s]/, @a;
        iseq "@b", "@c";

        @b = grep /\S/, @a;
        @c = grep /[\S]/, @a;
        iseq "@b", "@c";

        @b = grep /\s/, @a;
        @c = grep /[^\S]/, @a;
        iseq "@b", "@c";

        @b = grep /\s/, @a;
        @c = grep /[\s]/, @a;
        iseq "@b", "@c";
    }
    {
        local $Message = '\D, [\D], \d, [\d]';
        my @a = map chr, 0 .. 255;
        my @b = grep /\D/, @a;
        my @c = grep /[^\d]/, @a;
        iseq "@b", "@c";

        @b = grep /\D/, @a;
        @c = grep /[\D]/, @a;
        iseq "@b", "@c";

        @b = grep /\d/, @a;
        @c = grep /[^\D]/, @a;
        iseq "@b", "@c";

        @b = grep /\d/, @a;
        @c = grep /[\d]/, @a;
        iseq "@b", "@c";
    }
    {
        local $Message = '\W, [\W], \w, [\w]';
        my @a = map chr, 0 .. 255;
        my @b = grep /\W/, @a;
        my @c = grep /[^\w]/, @a;
        iseq "@b", "@c";

        @b = grep /\W/, @a;
        @c = grep /[\W]/, @a;
        iseq "@b", "@c";

        @b = grep /\w/, @a;
        @c = grep /[^\W]/, @a;
        iseq "@b", "@c";

        @b = grep /\w/, @a;
        @c = grep /[\w]/, @a;
        iseq "@b", "@c";
    }


    {
        # see if backtracking optimization works correctly
        local $Message = 'Backtrack optimization';
        ok "\n\n" =~ /\n   $ \n/x;
        ok "\n\n" =~ /\n*  $ \n/x;
        ok "\n\n" =~ /\n+  $ \n/x;
        ok "\n\n" =~ /\n?  $ \n/x;
        ok "\n\n" =~ /\n*? $ \n/x;
        ok "\n\n" =~ /\n+? $ \n/x;
        ok "\n\n" =~ /\n?? $ \n/x;
        ok "\n\n" !~ /\n*+ $ \n/x;
        ok "\n\n" !~ /\n++ $ \n/x;
        ok "\n\n" =~ /\n?+ $ \n/x;
    }


    {
        package S;
        use overload '""' => sub {'Object S'};
        sub new {bless []}

        local $::Message  = "Ref stringification";
      ::ok do { \my $v} =~ /^SCALAR/,   "Scalar ref stringification";
      ::ok do {\\my $v} =~ /^REF/,      "Ref ref stringification";
      ::ok []           =~ /^ARRAY/,    "Array ref stringification";
      ::ok {}           =~ /^HASH/,     "Hash ref stringification";
      ::ok 'S' -> new   =~ /^Object S/, "Object stringification";
    }


    {
        local $Message = "Test result of match used as match";
        ok 'a1b' =~ ('xyz' =~ /y/);
        iseq $`, 'a';
        ok 'a1b' =~ ('xyz' =~ /t/);
        iseq $`, 'a';
    }


    {
        local $Message = '"1" is not \s';
        may_not_warn sub {ok ("1\n" x 102) !~ /^\s*\n/m};
    }


    {
        local $Message = '\s, [[:space:]] and [[:blank:]]';
        my %space = (spc   => " ",
                     tab   => "\t",
                     cr    => "\r",
                     lf    => "\n",
                     ff    => "\f",
        # There's no \v but the vertical tabulator seems miraculously
        # be 11 both in ASCII and EBCDIC.
                     vt    => chr(11),
                     false => "space");

        my @space0 = sort grep {$space {$_} =~ /\s/         } keys %space;
        my @space1 = sort grep {$space {$_} =~ /[[:space:]]/} keys %space;
        my @space2 = sort grep {$space {$_} =~ /[[:blank:]]/} keys %space;

        iseq "@space0", "cr ff lf spc tab";
        iseq "@space1", "cr ff lf spc tab vt";
        iseq "@space2", "spc tab";
    }

    {
        use charnames ":full";
        local $Message = 'Delayed interpolation of \N';
        my $r1 = qr/\N{THAI CHARACTER SARA I}/;
        my $s1 = "\x{E34}\x{E34}\x{E34}\x{E34}";

        # Bug #56444
        ok $s1 =~ /$r1+/, 'my $r1 = qr/\N{THAI CHARACTER SARA I}/; my $s1 = "\x{E34}\x{E34}\x{E34}\x{E34}; $s1 =~ /$r1+/';

        # Bug #62056
        ok "${s1}A" =~ m/$s1\N{LATIN CAPITAL LETTER A}/, '"${s1}A" =~ m/$s1\N{LATIN CAPITAL LETTER A}/';

        ok "abbbbc" =~ m/\N{1}/ && $& eq "a", '"abbbbc" =~ m/\N{1}/ && $& eq "a"';
        ok "abbbbc" =~ m/\N{3,4}/ && $& eq "abbb", '"abbbbc" =~ m/\N{3,4}/ && $& eq "abbb"';
    }

    {
        use charnames ":full";
        local $Message = '[perl #74982] Period coming after \N{}';
        ok "\x{ff08}." =~ m/\N{FULLWIDTH LEFT PARENTHESIS}./ && $& eq "\x{ff08}.";
        ok "\x{ff08}." =~ m/[\N{FULLWIDTH LEFT PARENTHESIS}]./ && $& eq "\x{ff08}.";
    }
    {
        # we are actually testing that we dont die when executing these patterns
        use utf8;
        my $e = "Böck";
        ok(utf8::is_utf8($e),"got a unicode string - rt75680");

        ok($e !~ m/.*?[x]$/, "unicode string against /.*?[x]\$/ - rt75680");
        ok($e !~ m/.*?\p{Space}$/i, "unicode string against /.*?\\p{space}\$/i - rt75680");
        ok($e !~ m/.*?[xyz]$/, "unicode string against /.*?[xyz]\$/ - rt75680");
        ok($e !~ m/(.*?)[,\p{isSpace}]+((?:\p{isAlpha}[\p{isSpace}\.]{1,2})+)\p{isSpace}*$/, "unicode string against big pattern - rt75680");
    }
    {
        # we are actually testing that we dont die when executing these patterns
        my $e = "B\x{f6}ck";
        ok(!utf8::is_utf8($e), "got a latin string - rt75680");

        ok($e !~ m/.*?[x]$/, "latin string against /.*?[x]\$/ - rt75680");
        ok($e !~ m/.*?\p{Space}$/i, "latin string against /.*?\\p{space}\$/i - rt75680");
        ok($e !~ m/.*?[xyz]$/,"latin string against /.*?[xyz]\$/ - rt75680");
        ok($e !~ m/(.*?)[,\p{isSpace}]+((?:\p{isAlpha}[\p{isSpace}\.]{1,2})+)\p{isSpace}*$/,"latin string against big pattern - rt75680");
    }
} # End of sub run_tests

1;
