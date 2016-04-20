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
    require Config; import Config;
    require './test.pl';
}

plan tests => 727;  # Update this when adding/deleting tests.

run_tests() unless caller;

#
# Tests start here.
#
sub run_tests {

    {
        my $x = "abc\ndef\n";
	(my $x_pretty = $x) =~ s/\n/\\n/g;

        ok $x =~ /^abc/,  qq ["$x_pretty" =~ /^abc/];
        ok $x !~ /^def/,  qq ["$x_pretty" !~ /^def/];

        # used to be a test for $*
        ok $x =~ /^def/m, qq ["$x_pretty" =~ /^def/m];

        ok(!($x =~ /^xxx/), qq ["$x_pretty" =~ /^xxx/]);
        ok(!($x !~ /^abc/), qq ["$x_pretty" !~ /^abc/]);

         ok $x =~ /def/, qq ["$x_pretty" =~ /def/];
        ok(!($x !~ /def/), qq ["$x_pretty" !~ /def/]);

         ok $x !~ /.def/, qq ["$x_pretty" !~ /.def/];
        ok(!($x =~ /.def/), qq ["$x_pretty" =~ /.def/]);

         ok $x =~ /\ndef/, qq ["$x_pretty" =~ /\\ndef/];
        ok(!($x !~ /\ndef/), qq ["$x_pretty" !~ /\\ndef/]);
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
        unlike($_, qr/a+b?c+/, qq [\$_ = '$_'; /a+b?c+/]);

        $_ = 'aaabccc';
         ok /a+b?c+/, qq [\$_ = '$_'; /a+b?c+/];
         ok /a*b?c*/, qq [\$_ = '$_'; /a*b?c*/];

        $_ = 'aaaccc';
         ok /a*b?c*/, qq [\$_ = '$_'; /a*b?c*/];
        unlike($_, qr/a*b+c*/, qq [\$_ = '$_'; /a*b+c*/]);

        $_ = 'abcdef';
         ok /bcd|xyz/, qq [\$_ = '$_'; /bcd|xyz/];
         ok /xyz|bcd/, qq [\$_ = '$_'; /xyz|bcd/];
         ok m|bc/*d|,  qq [\$_ = '$_'; m|bc/*d|];
         ok /^$_$/,    qq [\$_ = '$_'; /^\$_\$/];
    }

    {
        # used to be a test for $*
        ok "ab\ncd\n" =~ /^cd/m, q ["ab\ncd\n" =~ /^cd/m];
    }

    {
        our %XXX = map {($_ => $_)} 123, 234, 345;

        our @XXX = ('ok 1','not ok 1', 'ok 2','not ok 2','not ok 3');
        while ($_ = shift(@XXX)) {
            my $e = index ($_, 'not') >= 0 ? '' : 1;
            my $r = m?(.*)?;
            is($r, $e, "?(.*)?");
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
        my $message = "Test empty pattern";
        my $xyz = 'xyz';
        my $cde = 'cde';

        $cde =~ /[^ab]*/;
        $xyz =~ //;
        is($&, $xyz, $message);

        my $foo = '[^ab]*';
        $cde =~ /$foo/;
        $xyz =~ //;
        is($&, $xyz, $message);

        $cde =~ /$foo/;
        my $null;
        no warnings 'uninitialized';
        $xyz =~ /$null/;
        is($&, $xyz, $message);

        $null = "";
        $xyz =~ /$null/;
        is($&, $xyz, $message);
    }

    {
        my $message = q !Check $`, $&, $'!;
        $_ = 'abcdefghi';
        /def/;        # optimized up to cmd
        is("$`:$&:$'", 'abc:def:ghi', $message);

        no warnings 'void';
        /cde/ + 0;    # optimized only to spat
        is("$`:$&:$'", 'ab:cde:fghi', $message);

        /[d][e][f]/;    # not optimized
        is("$`:$&:$'", 'abc:def:ghi', $message);
    }

    {
        $_ = 'now is the {time for all} good men to come to.';
        / \{([^}]*)}/;
        is($1, 'time for all', "Match braces");
    }

    {
        my $message = "{N,M} quantifier";
        $_ = 'xxx {3,4}  yyy   zzz';
        ok(/( {3,4})/, $message);
        is($1, '   ', $message);
        unlike($_, qr/( {4,})/, $message);
        ok(/( {2,3}.)/, $message);
        is($1, '  y', $message);
        ok(/(y{2,3}.)/, $message);
        is($1, 'yyy ', $message);
        unlike($_, qr/x {3,4}/, $message);
        unlike($_, qr/^xxx {3,4}/, $message);
    }

    {
        my $message = "Test /g";
        local $" = ":";
        $_ = "now is the time for all good men to come to.";
        my @words = /(\w+)/g;
        my $exp   = "now:is:the:time:for:all:good:men:to:come:to";

        is("@words", $exp, $message);

        @words = ();
        while (/\w+/g) {
            push (@words, $&);
        }
        is("@words", $exp, $message);

        @words = ();
        pos = 0;
        while (/to/g) {
            push(@words, $&);
        }
        is("@words", "to:to", $message);

        pos $_ = 0;
        @words = /to/g;
        is("@words", "to:to", $message);
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
        is($x, '505550555', "Test /o");
    }

    {
        my $xyz = 'xyz';
        ok "abc" =~ /^abc$|$xyz/, "| after \$";

        # perl 4.009 says "unmatched ()"
        my $message = '$ inside ()';

        my $result;
        eval '"abc" =~ /a(bc$)|$xyz/; $result = "$&:$1"';
        is($@, "", $message);
        is($result, "abc:bc", $message);
    }

    {
        my $message = "Scalar /g";
        $_ = "abcfooabcbar";

        ok( /abc/g && $` eq "", $message);
        ok( /abc/g && $` eq "abcfoo", $message);
        ok(!/abc/g, $message);

        $message = "Scalar /gi";
        pos = 0;
        ok( /ABC/gi && $` eq "", $message);
        ok( /ABC/gi && $` eq "abcfoo", $message);
        ok(!/ABC/gi, $message);

        $message = "Scalar /g";
        pos = 0;
        ok( /abc/g && $' eq "fooabcbar", $message);
        ok( /abc/g && $' eq "bar", $message);

        $_ .= '';
        my @x = /abc/g;
        is(@x, 2, "/g reset after assignment");
    }

    {
        my $message = '/g, \G and pos';
        $_ = "abdc";
        pos $_ = 2;
        /\Gc/gc;
        is(pos $_, 2, $message);
        /\Gc/g;
        is(pos $_, undef, $message);
    }

    {
        my $message = '(?{ })';
        our $out = 1;
        'abc' =~ m'a(?{ $out = 2 })b';
        is($out, 2, $message);

        $out = 1;
        'abc' =~ m'a(?{ $out = 3 })c';
        is($out, 1, $message);
    }

    {
        $_ = 'foobar1 bar2 foobar3 barfoobar5 foobar6';
        my @out = /(?<!foo)bar./g;
        is("@out", 'bar2 barf', "Negative lookbehind");
    }

    {
        my $message = "REG_INFTY tests";
        # Tests which depend on REG_INFTY

	#  Defaults assumed if this fails
	eval { require Config; };
        $::reg_infty   = $Config::Config{reg_infty} // 32767;
        $::reg_infty_m = $::reg_infty - 1;
        $::reg_infty_p = $::reg_infty + 1;
        $::reg_infty_m = $::reg_infty_m;   # Suppress warning.

        # As well as failing if the pattern matches do unexpected things, the
        # next three tests will fail if you should have picked up a lower-than-
        # default value for $reg_infty from Config.pm, but have not.

        is(eval q{('aaa' =~ /(a{1,$::reg_infty_m})/)[0]}, 'aaa', $message);
        is($@, '', $message);
        is(eval q{('a' x $::reg_infty_m) =~ /a{$::reg_infty_m}/}, 1, $message);
        is($@, '', $message);
        isnt(q{('a' x ($::reg_infty_m - 1)) !~ /a{$::reg_infty_m}/}, 1, $message);
        is($@, '', $message);

        eval "'aaa' =~ /a{1,$::reg_infty}/";
        like($@, qr/^\QQuantifier in {,} bigger than/, $message);
        eval "'aaa' =~ /a{1,$::reg_infty_p}/";
        like($@, qr/^\QQuantifier in {,} bigger than/, $message);
    }

    {
        # Poke a couple more parse failures
        my $context = 'x' x 256;
        eval qq("${context}y" =~ /(?<=$context)y/);
        ok $@ =~ /^\QLookbehind longer than 255 not/, "Lookbehind limit";
    }

    {
        # Long Monsters
        for my $l (125, 140, 250, 270, 300000, 30) { # Ordered to free memory
            my $a = 'a' x $l;
	    my $message = "Long monster, length = $l";
	    like("ba$a=", qr/a$a=/, $message);
            unlike("b$a=", qr/a$a=/, $message);
            like("b$a=", qr/ba+=/, $message);

	    like("ba$a=", qr/b(?:a|b)+=/, $message);
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

        for (keys %ans) {
	    my $message = "20000 nodes, const-len '$_'";
            ok !($ans{$_} xor /a(?=([yx]($long_constant_len)){2,4}[k-o]).*b./o), $message;

	    $message = "20000 nodes, var-len '$_'";
            ok !($ans{$_} xor /a(?=([yx]($long_var_len)){2,4}[k-o]).*b./o,), $message;
        }
    }

    {
        my $message = "Complicated backtracking";
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
        is("@ans", "1 1 1", $message);

        @ans = matchit;
        is("@ans", $expect, $message);

        $message = "Recursion with (??{ })";
        our $matched;
        $matched = qr/\((?:(?>[^()]+)|(??{$matched}))*\)/;

        @ans = my @ans1 = ();
        push (@ans, $res), push (@ans1, $&) while $res = m/$matched/g;

        is("@ans", "1 1 1", $message);
        is("@ans1", $expect, $message);

        @ans = m/$matched/g;
        is("@ans", $expect, $message);

    }

    {
        ok "abc" =~ /^(??{"a"})b/, '"abc" =~ /^(??{"a"})b/';
    }

    {
        my @ans = ('a/b' =~ m%(.*/)?(.*)%);    # Stack may be bad
        is("@ans", 'a/ b', "Stack may be bad");
    }

    {
        my $message = "Eval-group not allowed at runtime";
        my $code = '{$blah = 45}';
        our $blah = 12;
        eval { /(?$code)/ };
        ok($@ && $@ =~ /not allowed at runtime/ && $blah == 12, $message);

	$blah = 12;
	my $res = eval { "xx" =~ /(?$code)/o };
	{
	    no warnings 'uninitialized';
	    chomp $@; my $message = "$message '$@', '$res', '$blah'";
	    ok($@ && $@ =~ /not allowed at runtime/ && $blah == 12, $message);
	}

        $code = '=xx';
	$blah = 12;
	$res = eval { "xx" =~ /(?$code)/o };
	{
	    no warnings 'uninitialized';
	    my $message = "$message '$@', '$res', '$blah'";
	    ok(!$@ && $res, $message);
	}

        $code = '{$blah = 45}';
        $blah = 12;
        eval "/(?$code)/";
        is($blah, 45, $message);

        $blah = 12;
        /(?{$blah = 45})/;
        is($blah, 45, $message);
    }

    {
        my $message = "Pos checks";
        my $x = 'banana';
        $x =~ /.a/g;
        is(pos $x, 2, $message);

        $x =~ /.z/gc;
        is(pos $x, 2, $message);

        sub f {
            my $p = $_[0];
            return $p;
        }

        $x =~ /.a/g;
        is(f (pos $x), 4, $message);
    }

    {
        my $message = 'Checking $^R';
        our $x = $^R = 67;
        'foot' =~ /foo(?{$x = 12; 75})[t]/;
        is($^R, 75, $message);

        $x = $^R = 67;
        'foot' =~ /foo(?{$x = 12; 75})[xy]/;
        ok($^R eq '67' && $x eq '12', $message);

        $x = $^R = 67;
        'foot' =~ /foo(?{ $^R + 12 })((?{ $x = 12; $^R + 17 })[xy])?/;
        ok($^R eq '79' && $x eq '12', $message);
    }

    {
        is(qr/\b\v$/i,    '(?^i:\b\v$)', 'qr/\b\v$/i');
        is(qr/\b\v$/s,    '(?^s:\b\v$)', 'qr/\b\v$/s');
        is(qr/\b\v$/m,    '(?^m:\b\v$)', 'qr/\b\v$/m');
        is(qr/\b\v$/x,    '(?^x:\b\v$)', 'qr/\b\v$/x');
        is(qr/\b\v$/xism, '(?^msix:\b\v$)',  'qr/\b\v$/xism');
        is(qr/\b\v$/,     '(?^:\b\v$)', 'qr/\b\v$/');
    }

    SKIP: {   # Test that charset modifier work, and are interpolated
        if (
            !$Config::Config{d_setlocale}
        || $Config::Config{ccflags} =~ /\bD?NO_LOCALE(_|\b)/
        ) {
            skip "no locale support", 13
        }
        is(qr/\b\v$/, '(?^:\b\v$)', 'Verify no locale, no unicode_strings gives default modifier');
        is(qr/(?l:\b\v$)/, '(?^:(?l:\b\v$))', 'Verify infix l modifier compiles');
        is(qr/(?u:\b\v$)/, '(?^:(?u:\b\v$))', 'Verify infix u modifier compiles');
        is(qr/(?l)\b\v$/, '(?^:(?l)\b\v$)', 'Verify (?l) compiles');
        is(qr/(?u)\b\v$/, '(?^:(?u)\b\v$)', 'Verify (?u) compiles');

        my $dual = qr/\b\v$/;
        my $locale;

      SKIP: {
            skip 'No locale testing without d_setlocale', 1 if(!$Config{d_setlocale});

            BEGIN {
                if($Config{d_setlocale}) {
                    require locale; import locale;
                }
            }
            $locale = qr/\b\v$/;
            is($locale,    '(?^l:\b\v$)', 'Verify has l modifier when compiled under use locale');
            no locale;
        }

        use feature 'unicode_strings';
        my $unicode = qr/\b\v$/;
        is($unicode,    '(?^u:\b\v$)', 'Verify has u modifier when compiled under unicode_strings');
        is(qr/abc$dual/,    '(?^u:abc(?^:\b\v$))', 'Verify retains d meaning when interpolated under locale');

      SKIP: {
            skip 'No locale testing without d_setlocale', 1 if(!$Config{d_setlocale});

            is(qr/abc$locale/,    '(?^u:abc(?^l:\b\v$))', 'Verify retains l when interpolated under unicode_strings');
        }

        no feature 'unicode_strings';
      SKIP: {
            skip 'No locale testing without d_setlocale', 1 if(!$Config{d_setlocale});

            is(qr/abc$locale/,    '(?^:abc(?^l:\b\v$))', 'Verify retains l when interpolated outside locale and unicode strings');
        }

        is(qr/def$unicode/,    '(?^:def(?^u:\b\v$))', 'Verify retains u when interpolated outside locale and unicode strings');

      SKIP: {
            skip 'No locale testing without d_setlocale', 2 if(!$Config{d_setlocale});

             BEGIN {
                if($Config{d_setlocale}) {
                    require locale; import locale;
                }
            }
            is(qr/abc$dual/,    '(?^l:abc(?^:\b\v$))', 'Verify retains d meaning when interpolated under locale');
            is(qr/abc$unicode/,    '(?^l:abc(?^u:\b\v$))', 'Verify retains u when interpolated under locale');
        }
    }

    {
        my $message = "Look around";
        $_ = 'xabcx';
        foreach my $ans ('', 'c') {
            ok(/(?<=(?=a)..)((?=c)|.)/g, $message);
            is($1, $ans, $message);
        }
    }

    {
        my $message = "Empty clause";
        $_ = 'a';
        foreach my $ans ('', 'a', '') {
            ok(/^|a|$/g, $message);
            is($&, $ans, $message);
        }
    }

    {
        sub prefixify {
        my $message = "Prefixify";
            {
                my ($v, $a, $b, $res) = @_;
                ok($v =~ s/\Q$a\E/$b/, $message);
                is($v, $res, $message);
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
        my $message = '(?{ $var } refers to package vars';
        package aa;
        our $c = 2;
        $::c = 3;
        '' =~ /(?{ $c = 4 })/;
        main::is($c, 4, $message);
        main::is($::c, 3, $message);
    }

    {
        is(eval 'q(a:[b]:) =~ /[x[:foo:]]/', undef);
	like ($@, qr/POSIX class \[:[^:]+:\] unknown in regex/,
	      'POSIX class [: :] must have valid name');

        for my $d (qw [= .]) {
            is(eval "/[[${d}foo${d}]]/", undef);
	    like ($@, qr/\QPOSIX syntax [$d $d] is reserved for future extensions/,
		  "POSIX syntax [[$d $d]] is an error");
        }
    }

    {
        # test if failure of patterns returns empty list
        my $message = "Failed pattern returns empty list";
        $_ = 'aaa';
        @_ = /bbb/;
        is("@_", "", $message);

        @_ = /bbb/g;
        is("@_", "", $message);

        @_ = /(bbb)/;
        is("@_", "", $message);

        @_ = /(bbb)/g;
        is("@_", "", $message);
    }

    {
        my $message = '@- and @+ tests';

        /a(?=.$)/;
        is($#+, 0, $message);
        is($#-, 0, $message);
        is($+ [0], 2, $message);
        is($- [0], 1, $message);
        ok(!defined $+ [1] && !defined $- [1] &&
           !defined $+ [2] && !defined $- [2], $message);

        /a(a)(a)/;
        is($#+, 2, $message);
        is($#-, 2, $message);
        is($+ [0], 3, $message);
        is($- [0], 0, $message);
        is($+ [1], 2, $message);
        is($- [1], 1, $message);
        is($+ [2], 3, $message);
        is($- [2], 2, $message);
        ok(!defined $+ [3] && !defined $- [3] &&
           !defined $+ [4] && !defined $- [4], $message);

        # Exists has a special check for @-/@+ - bug 45147
        ok(exists $-[0], $message);
        ok(exists $+[0], $message);
        ok(exists $-[2], $message);
        ok(exists $+[2], $message);
        ok(!exists $-[3], $message);
        ok(!exists $+[3], $message);
        ok(exists $-[-1], $message);
        ok(exists $+[-1], $message);
        ok(exists $-[-3], $message);
        ok(exists $+[-3], $message);
        ok(!exists $-[-4], $message);
        ok(!exists $+[-4], $message);

        /.(a)(b)?(a)/;
        is($#+, 3, $message);
        is($#-, 3, $message);
        is($+ [1], 2, $message);
        is($- [1], 1, $message);
        is($+ [3], 3, $message);
        is($- [3], 2, $message);
        ok(!defined $+ [2] && !defined $- [2] &&
           !defined $+ [4] && !defined $- [4], $message);

        /.(a)/;
        is($#+, 1, $message);
        is($#-, 1, $message);
        is($+ [0], 2, $message);
        is($- [0], 0, $message);
        is($+ [1], 2, $message);
        is($- [1], 1, $message);
        ok(!defined $+ [2] && !defined $- [2] &&
           !defined $+ [3] && !defined $- [3], $message);

        /.(a)(ba*)?/;
        is($#+, 2, $message);
        is($#-, 1, $message);

        # Check that values don’t stick
        "     "=~/()()()(.)(..)/;
        my($m,$p) = (\$-[5], \$+[5]);
        () = "$$_" for $m, $p; # FETCH (or eqv.)
        " " =~ /()/;
        is $$m, undef, 'values do not stick to @- elements';
        is $$p, undef, 'values do not stick to @+ elements';
    }

    foreach ('$+[0] = 13', '$-[0] = 13', '@+ = (7, 6, 5)',
	     '@- = qw (foo bar)', '$^N = 42') {
	is(eval $_, undef);
        like($@, qr/^Modification of a read-only value attempted/,
	     '$^N, @- and @+ are read-only');
    }

    {
        my $message = '\G testing';
        $_ = 'aaa';
        pos = 1;
        my @a = /\Ga/g;
        is("@a", "a a", $message);

        my $str = 'abcde';
        pos $str = 2;
        unlike($str, qr/^\G/, $message);
        unlike($str, qr/^.\G/, $message);
        like($str, qr/^..\G/, $message);
        unlike($str, qr/^...\G/, $message);
        ok($str =~ /\G../ && $& eq 'cd', $message);
        ok($str =~ /.\G./ && $& eq 'bc', $message);

    }

    {
        my $message = '\G and intuit and anchoring';
	$_ = "abcdef";
	pos = 0;
	ok($_ =~ /\Gabc/, $message);
	ok($_ =~ /^\Gabc/, $message);

	pos = 3;
	ok($_ =~ /\Gdef/, $message);
	pos = 3;
	ok($_ =~ /\Gdef$/, $message);
	pos = 3;
	ok($_ =~ /abc\Gdef$/, $message);
	pos = 3;
	ok($_ =~ /^abc\Gdef$/, $message);
	pos = 3;
	ok($_ =~ /c\Gd/, $message);
	pos = 3;
	ok($_ =~ /..\GX?def/, $message);
    }

    {
        my $s = '123';
        pos($s) = 1;
        my @a = $s =~ /(\d)\G/g; # this infinitely looped up till 5.19.1
        is("@a", "1", '\G looping');
    }


    {
        my $message = 'pos inside (?{ })';
        my $str = 'abcde';
        our ($foo, $bar);
        like($str, qr/b(?{$foo = $_; $bar = pos})c/, $message);
        is($foo, $str, $message);
        is($bar, 2, $message);
        is(pos $str, undef, $message);

        undef $foo;
        undef $bar;
        pos $str = undef;
        ok($str =~ /b(?{$foo = $_; $bar = pos})c/g, $message);
        is($foo, $str, $message);
        is($bar, 2, $message);
        is(pos $str, 3, $message);

        $_ = $str;
        undef $foo;
        undef $bar;
        like($_, qr/b(?{$foo = $_; $bar = pos})c/, $message);
        is($foo, $str, $message);
        is($bar, 2, $message);

        undef $foo;
        undef $bar;
        ok(/b(?{$foo = $_; $bar = pos})c/g, $message);
        is($foo, $str, $message);
        is($bar, 2, $message);
        is(pos, 3, $message);

        undef $foo;
        undef $bar;
        pos = undef;
        1 while /b(?{$foo = $_; $bar = pos})c/g;
        is($foo, $str, $message);
        is($bar, 2, $message);
        is(pos, undef, $message);

        undef $foo;
        undef $bar;
        $_ = 'abcde|abcde';
        ok(s/b(?{$foo = $_; $bar = pos})c/x/g, $message);
        is($foo, 'abcde|abcde', $message);
        is($bar, 8, $message);
        is($_, 'axde|axde', $message);

        # List context:
        $_ = 'abcde|abcde';
        our @res;
        () = /([ace]).(?{push @res, $1,$2})([ce])(?{push @res, $1,$2})/g;
        @res = map {defined $_ ? "'$_'" : 'undef'} @res;
        is("@res", "'a' undef 'a' 'c' 'e' undef 'a' undef 'a' 'c'", $message);

        @res = ();
        () = /([ace]).(?{push @res, $`,$&,$'})([ce])(?{push @res, $`,$&,$'})/g;
        @res = map {defined $_ ? "'$_'" : 'undef'} @res;
        is("@res", "'' 'ab' 'cde|abcde' " .
                     "'' 'abc' 'de|abcde' " .
                     "'abcd' 'e|' 'abcde' " .
                     "'abcde|' 'ab' 'cde' " .
                     "'abcde|' 'abc' 'de'", $message);
    }

    {
        my $message = '\G anchor checks';
        my $foo = 'aabbccddeeffgg';
        pos ($foo) = 1;

	ok($foo =~ /.\G(..)/g, $message);
	is($1, 'ab', $message);

	pos ($foo) += 1;
	ok($foo =~ /.\G(..)/g, $message);
	is($1, 'cc', $message);

	pos ($foo) += 1;
	ok($foo =~ /.\G(..)/g, $message);
	is($1, 'de', $message);

	ok($foo =~ /\Gef/g, $message);

        undef pos $foo;
        ok($foo =~ /\G(..)/g, $message);
        is($1, 'aa', $message);

        ok($foo =~ /\G(..)/g, $message);
        is($1, 'bb', $message);

        pos ($foo) = 5;
        ok($foo =~ /\G(..)/g, $message);
        is($1, 'cd', $message);
    }

    {
        my $message = 'basic \G floating checks';
        my $foo = 'aabbccddeeffgg';
        pos ($foo) = 1;

	ok($foo =~ /a+\G(..)/g, "$message: a+\\G");
	is($1, 'ab', "$message: ab");

	pos ($foo) += 1;
	ok($foo =~ /b+\G(..)/g, "$message: b+\\G");
	is($1, 'cc', "$message: cc");

	pos ($foo) += 1;
	ok($foo =~ /d+\G(..)/g, "$message: d+\\G");
	is($1, 'de', "$message: de");

	ok($foo =~ /\Gef/g, "$message: \\Gef");

        pos ($foo) = 1;

	ok($foo =~ /(?=a+\G)(..)/g, "$message: (?a+\\G)");
	is($1, 'aa', "$message: aa");

        pos ($foo) = 2;

	ok($foo =~ /a(?=a+\G)(..)/g, "$message: a(?=a+\\G)");
	is($1, 'ab', "$message: ab");

    }

    {
        $_ = '123x123';
        my @res = /(\d*|x)/g;
        local $" = '|';
        is("@res", "123||x|123|", "0 match in alternation");
    }

    {
        my $message = "Match against temporaries (created via pp_helem())" .
                         " is safe";
        ok({foo => "bar\n" . $^X} -> {foo} =~ /^(.*)\n/g, $message);
        is($1, "bar", $message);
    }

    {
        my $message = 'package $i inside (?{ }), ' .
                         'saved substrings and changing $_';
        our @a = qw [foo bar];
        our @b = ();
        s/(\w)(?{push @b, $1})/,$1,/g for @a;
        is("@b", "f o o b a r", $message);
        is("@a", ",f,,o,,o, ,b,,a,,r,", $message);

        $message = 'lexical $i inside (?{ }), ' .
                         'saved substrings and changing $_';
        no warnings 'closure';
        my @c = qw [foo bar];
        my @d = ();
        s/(\w)(?{push @d, $1})/,$1,/g for @c;
        is("@d", "f o o b a r", $message);
        is("@c", ",f,,o,,o, ,b,,a,,r,", $message);
    }

    {
        my $message = 'Brackets';
        our $brackets;
        $brackets = qr {
            {  (?> [^{}]+ | (??{ $brackets }) )* }
        }x;

        ok("{{}" =~ $brackets, $message);
        is($&, "{}", $message);
        ok("something { long { and } hairy" =~ $brackets, $message);
        is($&, "{ and }", $message);
        ok("something { long { and } hairy" =~ m/((??{ $brackets }))/, $message);
        is($&, "{ and }", $message);
    }

    {
        $_ = "a-a\nxbb";
        pos = 1;
        ok(!m/^-.*bb/mg, '$_ = "a-a\nxbb"; m/^-.*bb/mg');
    }

    {
        my $message = '\G anchor checks';
        my $text = "aaXbXcc";
        pos ($text) = 0;
        ok($text !~ /\GXb*X/g, $message);
    }

    {
        $_ = "xA\n" x 500;
        unlike($_, qr/^\s*A/m, '$_ = "xA\n" x 500; /^\s*A/m"');

        my $text = "abc dbf";
        my @res = ($text =~ /.*?(b).*?\b/g);
        is("@res", "b b", '\b is not special');
    }

    {
        my $message = '\S, [\S], \s, [\s]';
        my @a = map chr, 0 .. 255;
        my @b = grep m/\S/, @a;
        my @c = grep m/[^\s]/, @a;
        is("@b", "@c", $message);

        @b = grep /\S/, @a;
        @c = grep /[\S]/, @a;
        is("@b", "@c", $message);

        @b = grep /\s/, @a;
        @c = grep /[^\S]/, @a;
        is("@b", "@c", $message);

        @b = grep /\s/, @a;
        @c = grep /[\s]/, @a;
        is("@b", "@c", $message);
    }
    {
        my $message = '\D, [\D], \d, [\d]';
        my @a = map chr, 0 .. 255;
        my @b = grep /\D/, @a;
        my @c = grep /[^\d]/, @a;
        is("@b", "@c", $message);

        @b = grep /\D/, @a;
        @c = grep /[\D]/, @a;
        is("@b", "@c", $message);

        @b = grep /\d/, @a;
        @c = grep /[^\D]/, @a;
        is("@b", "@c", $message);

        @b = grep /\d/, @a;
        @c = grep /[\d]/, @a;
        is("@b", "@c", $message);
    }
    {
        my $message = '\W, [\W], \w, [\w]';
        my @a = map chr, 0 .. 255;
        my @b = grep /\W/, @a;
        my @c = grep /[^\w]/, @a;
        is("@b", "@c", $message);

        @b = grep /\W/, @a;
        @c = grep /[\W]/, @a;
        is("@b", "@c", $message);

        @b = grep /\w/, @a;
        @c = grep /[^\W]/, @a;
        is("@b", "@c", $message);

        @b = grep /\w/, @a;
        @c = grep /[\w]/, @a;
        is("@b", "@c", $message);
    }

    {
        # see if backtracking optimization works correctly
        my $message = 'Backtrack optimization';
        like("\n\n", qr/\n   $ \n/x, $message);
        like("\n\n", qr/\n*  $ \n/x, $message);
        like("\n\n", qr/\n+  $ \n/x, $message);
        like("\n\n", qr/\n?  $ \n/x, $message);
        like("\n\n", qr/\n*? $ \n/x, $message);
        like("\n\n", qr/\n+? $ \n/x, $message);
        like("\n\n", qr/\n?? $ \n/x, $message);
        unlike("\n\n", qr/\n*+ $ \n/x, $message);
        unlike("\n\n", qr/\n++ $ \n/x, $message);
        like("\n\n", qr/\n?+ $ \n/x, $message);
    }

    {
        package S;
        use overload '""' => sub {'Object S'};
        sub new {bless []}

        my $message  = "Ref stringification";
      ::ok(do { \my $v} =~ /^SCALAR/,   "Scalar ref stringification") or diag($message);
      ::ok(do {\\my $v} =~ /^REF/,      "Ref ref stringification") or diag($message);
      ::ok([]           =~ /^ARRAY/,    "Array ref stringification") or diag($message);
      ::ok({}           =~ /^HASH/,     "Hash ref stringification") or diag($message);
      ::ok('S' -> new   =~ /^Object S/, "Object stringification") or diag($message);
    }

    {
        my $message = "Test result of match used as match";
        ok('a1b' =~ ('xyz' =~ /y/), $message);
        is($`, 'a', $message);
        ok('a1b' =~ ('xyz' =~ /t/), $message);
        is($`, 'a', $message);
    }

    {
        my $message = '"1" is not \s';
        warning_is(sub {unlike("1\n" x 102, qr/^\s*\n/m, $message)},
		   undef, "$message (did not warn)");
    }

    {
        my $message = '\s, [[:space:]] and [[:blank:]]';
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

        is("@space0", "cr ff lf spc tab vt", $message);
        is("@space1", "cr ff lf spc tab vt", $message);
        is("@space2", "spc tab", $message);
    }

    {
        my $n= 50;
        # this must be a high number and go from 0 to N, as the bug we are looking for doesn't
        # seem to be predictable. Slight changes to the test make it fail earlier or later.
        foreach my $i (0 .. $n)
        {
            my $str= "\n" x $i;
            ok $str=~/.*\z/, "implicit MBOL check string disable does not break things length=$i";
        }
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

    {
        #
        # Tests for bug 77414.
        #

        my $message = '\p property after empty * match';
        {
            like("1", qr/\s*\pN/, $message);
            like("-", qr/\s*\p{Dash}/, $message);
            like(" ", qr/\w*\p{Blank}/, $message);
        }

        like("1", qr/\s*\pN+/, $message);
        like("-", qr/\s*\p{Dash}{1}/, $message);
        like(" ", qr/\w*\p{Blank}{1,4}/, $message);

    }

    SKIP: {   # Some constructs with Latin1 characters cause a utf8 string not
              # to match itself in non-utf8
        if ($::IS_EBCDIC) {
            skip "Needs to be customized to run on EBCDIC", 6;
        }
        my $c = "\xc0";
        my $pattern = my $utf8_pattern = qr/((\xc0)+,?)/;
        utf8::upgrade($utf8_pattern);
        ok $c =~ $pattern, "\\xc0 =~ $pattern; Neither pattern nor target utf8";
        ok $c =~ /$pattern/i, "\\xc0 =~ /$pattern/i; Neither pattern nor target utf8";
        ok $c =~ $utf8_pattern, "\\xc0 =~ $pattern; pattern utf8, target not";
        ok $c =~ /$utf8_pattern/i, "\\xc0 =~ /$pattern/i; pattern utf8, target not";
        utf8::upgrade($c);
        ok $c =~ $pattern, "\\xc0 =~ $pattern; target utf8, pattern not";
        ok $c =~ /$pattern/i, "\\xc0 =~ /$pattern/i; target utf8, pattern not";
        ok $c =~ $utf8_pattern, "\\xc0 =~ $pattern; Both target and pattern utf8";
        ok $c =~ /$utf8_pattern/i, "\\xc0 =~ /$pattern/i; Both target and pattern utf8";
    }

    SKIP: {   # Make sure can override the formatting
        if ($::IS_EBCDIC) {
            skip "Needs to be customized to run on EBCDIC", 2;
        }
        use feature 'unicode_strings';
        ok "\xc0" =~ /\w/, 'Under unicode_strings: "\xc0" =~ /\w/';
        ok "\xc0" !~ /(?d:\w)/, 'Under unicode_strings: "\xc0" !~ /(?d:\w)/';
    }

    {
        my $str= "\x{100}";
        chop $str;
        my $qr= qr/$str/;
        is("$qr", "(?^:)", "Empty pattern qr// stringifies to (?^:) with unicode flag enabled - Bug #80212");
        $str= "";
        $qr= qr/$str/;
        is("$qr", "(?^:)", "Empty pattern qr// stringifies to (?^:) with unicode flag disabled - Bug #80212");

    }

    {
        local $::TODO = "[perl #38133]";

        "A" =~ /(((?:A))?)+/;
        my $first = $2;

        "A" =~ /(((A))?)+/;
        my $second = $2;

        is($first, $second);
    }

    {
	# RT #3516: \G in a m//g expression causes problems
	my $count = 0;
	while ("abc" =~ m/(\G[ac])?/g) {
	    last if $count++ > 10;
	}
	ok($count < 10, 'RT #3516 A');

	$count = 0;
	while ("abc" =~ m/(\G|.)[ac]/g) {
	    last if $count++ > 10;
	}
	ok($count < 10, 'RT #3516 B');

	$count = 0;
	while ("abc" =~ m/(\G?[ac])?/g) {
	    last if $count++ > 10;
	}
	ok($count < 10, 'RT #3516 C');
    }
    {
        # RT #84294: Is this a bug in the simple Perl regex?
        #          : Nested buffers and (?{...}) dont play nicely on partial matches
        our @got= ();
        ok("ab" =~ /((\w+)(?{ push @got, $2 })){2}/,"RT #84294: Pattern should match");
        my $want= "'ab', 'a', 'b'";
        my $got= join(", ", map { defined($_) ? "'$_'" : "undef" } @got);
        is($got,$want,'RT #84294: check that "ab" =~ /((\w+)(?{ push @got, $2 })){2}/ leaves @got in the correct state');
    }

    {
        # Suppress warnings, as the non-unicode one comes out even if turn off
        # warnings here (because the execution is done in another scope).
        local $SIG{__WARN__} = sub {};
        my $str = "\x{110000}";

        unlike($str, qr/\p{ASCII_Hex_Digit=True}/, "Non-Unicode doesn't match \\p{AHEX=True}");
        like($str, qr/\p{ASCII_Hex_Digit=False}/, "Non-Unicode matches \\p{AHEX=False}");
        like($str, qr/\P{ASCII_Hex_Digit=True}/, "Non-Unicode matches \\P{AHEX=True}");
        unlike($str, qr/\P{ASCII_Hex_Digit=False}/, "Non-Unicode matches \\P{AHEX=FALSE}");
    }

    {
        # Test that IDstart works, but because the author (khw) knows
        # regexes much better than the rest of the core, it is being done here
        # in the context of a regex which relies on buffer names beginng with
        # IDStarts.
        use utf8;
        my $str = "abc";
        like($str, qr/(?<a>abc)/, "'a' is legal IDStart");
        like($str, qr/(?<_>abc)/, "'_' is legal IDStart");
        like($str, qr/(?<ß>abc)/, "U+00DF is legal IDStart");
        like($str, qr/(?<ℕ>abc)/, "U+2115' is legal IDStart");

        # This test works on Unicode 6.0 in which U+2118 and U+212E are legal
        # IDStarts there, but are not Word characters, and therefore Perl
        # doesn't allow them to be IDStarts.  But there is no guarantee that
        # Unicode won't change things around in the future so that at some
        # future Unicode revision these tests would need to be revised.
        foreach my $char ("%", "×", chr(0x2118), chr(0x212E)) {
            my $prog = <<"EOP";
use utf8;;
"abc" =~ qr/(?<$char>abc)/;
EOP
            utf8::encode($prog);
            fresh_perl_like($prog, qr!Group name must start with a non-digit word character!, {},
                        sprintf("'U+%04X not legal IDFirst'", ord($char)));
        }
    }

    { # [perl #101710]
        my $pat = "b";
        utf8::upgrade($pat);
        like("\xffb", qr/$pat/i, "/i: utf8 pattern, non-utf8 string, latin1-char preceding matching char in string");
    }

    { # Crash with @a =~ // warning
	local $SIG{__WARN__} = sub {
             pass 'no crash for @a =~ // warning'
        };
	eval ' sub { my @a =~ // } ';
    }

    { # Concat overloading and qr// thingies
	my @refs;
	my $qr = qr//;
        package Cat {
            require overload;
            overload->import(
		'""' => sub { ${$_[0]} },
		'.' => sub {
		    push @refs, ref $_[1] if ref $_[1];
		    bless $_[2] ? \"$_[1]${$_[0]}" : \"${$_[0]}$_[1]"
		}
            );
	}
	my $s = "foo";
	my $o = bless \$s, Cat::;
	/$o$qr/;
	is "@refs", "Regexp", '/$o$qr/ passes qr ref to cat overload meth';
    }

    {
        my $count=0;
        my $str="\n";
        $count++ while $str=~/.*/g;
        is $count, 2, 'test that ANCH_MBOL works properly. We should get 2 from $count++ while "\n"=~/.*/g';
        my $class_count= 0;
        $class_count++ while $str=~/[^\n]*/g;
        is $class_count, $count, 'while "\n"=~/.*/g and while "\n"=~/[^\n]*/g should behave the same';
        my $anch_count= 0;
        $anch_count++ while $str=~/^.*/mg;
        is $anch_count, 1, 'while "\n"=~/^.*/mg should match only once';
    }

    { # [perl #111174]
        use re '/u';
        like "\xe0", qr/(?i:\xc0)/, "(?i: shouldn't lose the passed in /u";
        use re '/a';
        unlike "\x{100}", qr/(?i:\w)/, "(?i: shouldn't lose the passed in /a";
        use re '/aa';
        unlike 'k', qr/(?i:\N{KELVIN SIGN})/, "(?i: shouldn't lose the passed in /aa";
    }

    {
	# the test for whether the pattern should be re-compiled should
	# consider the UTF8ness of the previous and current pattern
	# string, as well as the physical bytes of the pattern string

	for my $s ("\xc4\x80", "\x{100}") {
	    ok($s =~ /^$s$/, "re-compile check is UTF8-aware");
	}
    }

    #  #113682 more overloading and qr//
    # when doing /foo$overloaded/, if $overloaded returns
    # a qr/(?{})/ via qr or "" overloading, then 'use re 'eval'
    # shouldn't be required. Via '.', it still is.
    {
        package Qr0;
	use overload 'qr' => sub { qr/(??{50})/ };

        package Qr1;
	use overload '""' => sub { qr/(??{51})/ };

        package Qr2;
	use overload '.'  => sub { $_[1] . qr/(??{52})/ };

        package Qr3;
	use overload '""' => sub { qr/(??{7})/ },
		     '.'  => sub { $_[1] . qr/(??{53})/ };

        package Qr_indirect;
	use overload '""'  => sub { $_[0][0] };

	package main;

	for my $i (0..3) {
	    my $o = bless [], "Qr$i";
	    if ((0,0,1,1)[$i]) {
		eval { "A5$i" =~ /^A$o$/ };
		like($@, qr/Eval-group not allowed/, "Qr$i");
		eval { "5$i" =~ /$o/ };
		like($@, ($i == 3 ? qr/^$/ : qr/no method found,/),
			"Qr$i bare");
		{
		    use re 'eval';
		    ok("A5$i" =~ /^A$o$/, "Qr$i - with use re eval");
		    eval { "5$i" =~ /$o/ };
		    like($@, ($i == 3 ? qr/^$/ : qr/no method found,/),
			    "Qr$i bare - with use re eval");
		}
	    }
	    else {
		ok("A5$i" =~ /^A$o$/, "Qr$i");
		ok("5$i" =~ /$o/, "Qr$i bare");
	    }
	}

	my $o = bless [ bless [], "Qr1" ], 'Qr_indirect';
	ok("A51" =~ /^A$o/, "Qr_indirect");
	ok("51" =~ /$o/, "Qr_indirect bare");
    }

    {   # Various flags weren't being set when a [] is optimized into an
        # EXACTish node
        ;
        ;
        ok("\x{017F}\x{017F}" =~ qr/^[\x{00DF}]?$/i, "[] to EXACTish optimization");
    }

    {
        for my $char (":", "\x{f7}", "\x{2010}") {
            my $utf8_char = $char;
            utf8::upgrade($utf8_char);
            my $display = $char;
            $display = display($display);
            my $utf8_display = "utf8::upgrade(\"$display\")";

            like($char, qr/^$char?$/, "\"$display\" =~ /^$display?\$/");
            like($char, qr/^$utf8_char?$/, "my \$p = \"$display\"; utf8::upgrade(\$p); \"$display\" =~ /^\$p?\$/");
            like($utf8_char, qr/^$char?$/, "my \$c = \"$display\"; utf8::upgrade(\$c); \"\$c\" =~ /^$display?\$/");
            like($utf8_char, qr/^$utf8_char?$/, "my \$c = \"$display\"; utf8::upgrade(\$c); my \$p = \"$display\"; utf8::upgrade(\$p); \"\$c\" =~ /^\$p?\$/");
        }
    }

    {
	# #116148: Pattern utf8ness sticks around globally
	# the utf8 in the first match was sticking around for the second
	# match

	use feature 'unicode_strings';

	my $x = "\x{263a}";
	$x =~ /$x/;

	my $text = "Perl";
	ok("Perl" =~ /P.*$/i, '#116148');
    }

    { # 117327: Sequence (?#...) not recognized in regex
      # The space between the '(' and '?' is now deprecated; this test should
      # be removed when the deprecation is made fatal.
        no warnings;
        like("ab", qr/a( ?#foo)b/x);
    }

    { # 118297: Mixing up- and down-graded strings in regex
        utf8::upgrade(my $u = "\x{e5}");
        utf8::downgrade(my $d = "\x{e5}");
        my $warned;
        local $SIG{__WARN__} = sub { $warned++ if $_[0] =~ /\AMalformed UTF-8/ };
        my $re = qr/$u$d/;
        ok(!$warned, "no warnings when interpolating mixed up-/downgraded strings in pattern");
        my $c = "\x{e5}\x{e5}";
        utf8::downgrade($c);
        like($c, $re, "mixed up-/downgraded pattern matches downgraded string");
        utf8::upgrade($c);
        like($c, $re, "mixed up-/downgraded pattern matches upgraded string");
    }

    {
        # if we have 87 capture buffers defined then \87 should refer to the 87th.
        # test that this is true for 1..100
        # Note that this test causes the engine to recurse at runtime, and
        # hence use a lot of C stack.
        for my $i (1..100) {
            my $capture= "a";
            $capture= "($capture)" for 1 .. $i;
            for my $mid ("","b") {
                my $str= "a${mid}a";
                my $backref= "\\$i";
                eval {
                    ok($str=~/$capture$mid$backref/,"\\$i works with $i buffers '$str'=~/...$mid$backref/");
                    1;
                } or do {
                    is("$@","","\\$i works with $i buffers works with $i buffers '$str'=~/...$mid$backref/");
                };
            }
        }
    }

    # this mixture of readonly (not COWable) and COWable strings
    # messed up the capture buffers under COW. The actual test results
    # are incidental; the issue is was an AddressSanitizer failure
    {
	my $c ='AB';
	my $res = '';
	for ($c, 'C', $c, 'DE') {
	    ok(/(.)/, "COWable match");
	    $res .= $1;
	}
	is($res, "ACAD");
    }


    {
	# RT #45667
	# /[#$x]/x didn't interpolate the var $x.
	my $b = 'cd';
	my $s = 'abcd$%#&';
	$s =~ s/[a#$b%]/X/g;
	is ($s, 'XbXX$XX&', 'RT #45667 without /x');
	$s = 'abcd$%#&';
	$s =~ s/[a#$b%]/X/gx;
	is ($s, 'XbXX$XX&', 'RT #45667 with /x');
    }

    {
	no warnings "uninitialized";
	my @a;
	$a[1]++;
	/@a/;
	pass('no crash with /@a/ when array has nonexistent elems');
    }

    {
	is runperl(prog => 'delete $::{qq-\cR-}; //; print qq-ok\n-'),
	   "ok\n",
	   'deleting *^R does not result in crashes';
	no warnings 'once';
	*^R = *caretRglobwithnoscalar;
	"" =~ /(?{42})/;
	is $^R, 42, 'assigning to *^R does not result in a crash';
	is runperl(
	     stderr => 1,
	     prog => 'eval q|'
	            .' q-..- =~ /(??{undef *^R;q--})(?{42})/; '
                    .' print qq-$^R\n-'
	            .'|'
	   ),
	   "42\n",
	   'undefining *^R within (??{}) does not result in a crash';
    }

    {
        # [perl #120446]
        # this code should be virtually instantaneous. If it takes 10s of
        # seconds, there a bug in intuit_start.
        # (this test doesn't actually test for slowness - that involves
        # too much danger of false positives on loaded machines - but by
        # putting it here, hopefully someone might notice if it suddenly
        # runs slowly)
        my $s = ('a' x 1_000_000) . 'b';
        my $i = 0;
        for (1..10_000) {
            pos($s) = $_;
            $i++ if $s =~/\Gb/g;
        }
        is($i, 0, "RT 120446: mustn't run slowly");
    }

    {
        # [perl #120692]
        # these tests should be virtually instantaneous. If they take 10s of
        # seconds, there's a bug in intuit_start.

        my $s = 'ab' x 1_000_000;
        utf8::upgrade($s);
        1 while $s =~ m/\Ga+ba+b/g;
        pass("RT#120692 \\G mustn't run slowly");

        $s=~ /^a{1,2}x/ for  1..10_000;
        pass("RT#120692 a{1,2} mustn't run slowly");

        $s=~ /ab.{1,2}x/;
        pass("RT#120692 ab.{1,2} mustn't run slowly");

        $s = "-a-bc" x 250_000;
        $s .= "1a1bc";
        utf8::upgrade($s);
        ok($s =~ /\da\d{0,30000}bc/, "\\d{30000}");

        $s = "-ab\n" x 250_000;
        $s .= "abx";
        ok($s =~ /^ab.*x/m, "distant float with /m");

        my $r = qr/^abcd/;
        $s = "abcd-xyz\n" x 500_000;
        $s =~ /$r\d{1,2}xyz/m for 1..200;
        pass("BOL within //m  mustn't run slowly");

        $s = "abcdefg" x 1_000_000;
        $s =~ /(?-m:^)abcX?fg/m for 1..100;
        pass("BOL within //m  mustn't skip absolute anchored check");

        $s = "abcdefg" x 1_000_000;
        $s =~ /^XX\d{1,10}cde/ for 1..100;
        pass("abs anchored float string should fail quickly");

    }

    # These are based on looking at the code in regcomp.c
    # We don't look for specific code, just the existence of an SSC
    foreach my $re (qw(     qr/a?c/
                            qr/a?c/i
                            qr/[ab]?c/
                            qr/\R?c/
                            qr/\d?c/d
                            qr/\w?c/l
                            qr/\s?c/a
                            qr/[[:alpha:]]?c/u
    )) {
      SKIP: {
        skip "no re-debug under miniperl" if is_miniperl;
        my $prog = <<"EOP";
use re qw(Debug COMPILE);
$re;
EOP
        fresh_perl_like($prog, qr/synthetic stclass/, { stderr=>1 }, "$re generates a synthetic start class");
      }
    }

    {
        like "\x{AA}", qr/a?[\W_]/d, "\\W with /d synthetic start class works";
    }



    {   # Was getting optimized into EXACT (non-folding node)
        my $x = qr/[x]/i;
        utf8::upgrade($x);
        like("X", qr/$x/, "UTF-8 of /[x]/i matches upper case");
    }

    {	# [perl #123539]
        like("TffffffffffffTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTT5TTTTTTTTTTTTTTTTTTTTTTTTT3TTgTTTTTTTTTTTTTTTTTTTTT2TTTTTTTTTTTTTTTTTTTTTTTHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHiHHHHHHHfffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff&ffff", qr/TffffffffffffTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTT5TTTTTTTTTTTTTTTTTTTTTTTTT3TTgTTTTTTTTTTTTTTTTTTTTT2TTTTTTTTTTTTTTTTTTTTTTTHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHiHHHHHHHfffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff&ffff/il, "");
        like("TffffffffffffT\x{100}TTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTT5TTTTTTTTTTTTTTTTTTTTTTTTT3TTgTTTTTTTTTTTTTTTTTTTTT2TTTTTTTTTTTTTTTTTTTTTTTHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHiHHHHHHHfffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff&ffff", qr/TffffffffffffT\x{100}TTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTT5TTTTTTTTTTTTTTTTTTTTTTTTT3TTgTTTTTTTTTTTTTTTTTTTTT2TTTTTTTTTTTTTTTTTTTTTTTHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHiHHHHHHHfffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff&ffff/il, "");
    }

	{	# [perl #123604]
		my($s, $x, @x) = ('abc', 'a', 'd');
		my $long = 'b' x 2000;
		my $eval = q{$s =~ m{$x[bbb]c} ? 1 : 0};
		$eval =~ s{bbb}{$long};
		my $match = eval $eval;
		ok(1, "did not crash");
		ok($match, "[bbb...] resolved as character class, not subscript");
	}

        {   # Test that we handle some malformed UTF-8 without looping [perl
            # #123562]

            my $code='
                BEGIN{require q(test.pl);}
                use Encode qw(_utf8_on);
                my $malformed = "a\x80\n";
                _utf8_on($malformed);
                watchdog(3);
                $malformed =~ /(\n\r|\r)$/;
                print q(No infinite loop here!);
            ';
            fresh_perl_like($code, qr/Malformed UTF-8 character/, {},
                "test that we handle some UTF-8 malformations without looping" );
        }
} # End of sub run_tests

1;
