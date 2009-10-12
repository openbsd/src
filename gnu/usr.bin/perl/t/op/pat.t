#!./perl
#
# This is a home for regular expression tests that don't fit into
# the format supported by op/regexp.t.  If you want to add a test
# that does fit that format, add it to op/re_tests, not here.

use strict;
use warnings;
use 5.010;


sub run_tests;

$| = 1;

my $EXPECTED_TESTS = 4065;  # Update this when adding/deleting tests.

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}
our $TODO;
our $Message = "Noname test";
our $Error;
our $DiePattern;
our $WarnPattern;
our $BugId;
our $PatchId;
our $running_as_thread;

my $ordA = ord ('A');  # This defines ASCII/UTF-8 vs EBCDIC/UTF-EBCDIC
# This defined the platform.
my $IS_ASCII  = $ordA ==  65;
my $IS_EBCDIC = $ordA == 193;

use vars '%Config';
eval 'use Config';          #  Defaults assumed if this fails

my $test = 0;

print "1..$EXPECTED_TESTS\n";

run_tests unless caller ();

END {
}

sub pretty {
    my ($mess) = @_;
    $mess =~ s/\n/\\n/g;
    $mess =~ s/\r/\\r/g;
    $mess =~ s/\t/\\t/g;
    $mess =~ s/([\00-\37\177])/sprintf '\%03o', ord $1/eg;
    $mess =~ s/#/\\#/g;
    $mess;
}

sub safe_globals {
    defined($_) and s/#/\\#/g for $BugId, $PatchId, $TODO;
}

sub _ok {
    my ($ok, $mess, $error) = @_;
    safe_globals();
    $mess    = pretty ($mess // $Message);
    $mess   .= "; Bug $BugId"     if defined $BugId;
    $mess   .= "; Patch $PatchId" if defined $PatchId;
    $mess   .= " # TODO $TODO"     if defined $TODO;

    my $line_nr = (caller(1)) [2];

    printf "%sok %d - %s\n",
              ($ok ? "" : "not "),
              ++ $test,
              "$mess\tLine $line_nr";

    unless ($ok) {
        print "# Failed test at line $line_nr\n" unless defined $TODO;
        if ($error //= $Error) {
            no warnings 'utf8';
            chomp $error;
            $error = join "\n#", map {pretty $_} split /\n\h*#/ => $error;
            $error = "# $error" unless $error =~ /^\h*#/;
            print $error, "\n";
        }
    }

    return $ok;
}

# Force scalar context on the pattern match
sub  ok ($;$$) {_ok  $_ [0], $_ [1], $_ [2]}
sub nok ($;$$) {_ok !$_ [0], "Failed: " . ($_ [1] // $Message), $_ [2]}


sub skip {
    my $why = shift;
    safe_globals();
    $why =~ s/\n.*//s;
    $why .= "; Bug $BugId" if defined $BugId;
    # seems like the new harness code doesnt like todo and skip to be mixed.
    # which seems like a bug in the harness to me. -- dmq
    #$why .= " # TODO $TODO" if defined $TODO;
    
    my $n = shift // 1;
    my $line_nr = (caller(0)) [2];
    for (1 .. $n) {
        ++ $test;
        #print "not " if defined $TODO;
        print "ok $test # skip $why\tLine $line_nr\n";
    }
    no warnings "exiting";
    last SKIP;
}

sub iseq ($$;$) { 
    my ($got, $expect, $name) = @_;
    
    $_ = defined ($_) ? "'$_'" : "undef" for $got, $expect;
        
    my $ok    = $got eq $expect;
    my $error = "# expected: $expect\n" .
                "#   result: $got";

    _ok $ok, $name, $error;
}   

sub isneq ($$;$) { 
    my ($got, $expect, $name) = @_;
    my $todo = $TODO ? " # TODO $TODO" : '';
    
    $_ = defined ($_) ? "'$_'" : "undef" for $got, $expect;
        
    my $ok    = $got ne $expect;
    my $error = "# results are equal ($got)";

    _ok $ok, $name, $error;
}   


sub eval_ok ($;$) {
    my ($code, $name) = @_;
    local $@;
    if (ref $code) {
        _ok eval {&$code} && !$@, $name;
    }
    else {
        _ok eval  ($code) && !$@, $name;
    }
}

sub must_die {
    my ($code, $pattern, $name) = @_;
    $pattern //= $DiePattern;
    undef $@;
    ref $code ? &$code : eval $code;
    my  $r = $@ && $@ =~ /$pattern/;
    _ok $r, $name // $Message // "\$\@ =~ /$pattern/";
}

sub must_warn {
    my ($code, $pattern, $name) = @_;
    $pattern //= $WarnPattern;
    my $w;
    local $SIG {__WARN__} = sub {$w .= join "" => @_};
    use warnings 'all';
    ref $code ? &$code : eval $code;
    my $r = $w && $w =~ /$pattern/;
    $w //= "UNDEF";
    _ok $r, $name // $Message // "Got warning /$pattern/",
            "# expected: /$pattern/\n" .
            "#   result: $w";
}

sub may_not_warn {
    my ($code, $name) = @_;
    my $w;
    local $SIG {__WARN__} = sub {$w .= join "" => @_};
    use warnings 'all';
    ref $code ? &$code : eval $code;
    _ok !$w, $name // ($Message ? "$Message (did not warn)"
                                : "Did not warn"),
             "Got warning '$w'";
}


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
        /def/;		# optimized up to cmd
        iseq "$`:$&:$'", 'abc:def:ghi';

        no warnings 'void';
        /cde/ + 0;	# optimized only to spat
        iseq "$`:$&:$'", 'ab:cde:fghi';

        /[d][e][f]/;	# not optimized
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
                    'ax13876y25677y21378zbc' => 0,	# Not followed by [k-o]
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
               (?{ $c = 1 })	# Initialize
               (?:
                 (?(?{ $c == 0 })   # PREVIOUS iteration was OK, stop the loop
                   (?!
                   )		# Fail: will unwind one iteration back
                 )	
                 (?:
                   [^()]+		# Match a big chunk
                   (?=
                     [()]
                   )		# Do not try to match subchunks
                 |
                   \(
                   (?{ ++$c })
                 |
                   \)
                   (?{ --$c })
                 )
               )+		# This may not match with different subblocks
             )
             (?(?{ $c != 0 })
               (?!
               )		# Fail
             )			# Otherwise the chunk 1 may succeed with $c>0
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
        my @ans = ('a/b' =~ m%(.*/)?(.*)%);	# Stack may be bad
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
        local $Message =  "Call code from qr //";
        $a = qr/(?{++$b})/;
        $b = 7;
        ok /$a$a/ && $b eq '9';

        $c="$a";
        ok /$a$a/ && $b eq '11';

        undef $@;
        eval {/$c/};
        ok $@ && $@ =~ /not allowed at runtime/;

        use re "eval";
        /$a$c$a/;
        iseq $b, '14';

        our $lex_a = 43;
        our $lex_b = 17;
        our $lex_c = 27;
        my $lex_res = ($lex_b =~ qr/$lex_b(?{ $lex_c = $lex_a++ })/);

        iseq $lex_res, 1;
        iseq $lex_a, 44;
        iseq $lex_c, 43;

        no re "eval";
        undef $@;
        my $match = eval { /$a$c$a/ };
        ok $@ && $@ =~ /Eval-group not allowed/ && !$match;
        iseq $b, '14';
     
        $lex_a = 2;
        $lex_a = 43;
        $lex_b = 17;
        $lex_c = 27;
        $lex_res = ($lex_b =~ qr/17(?{ $lex_c = $lex_a++ })/);

        iseq $lex_res, 1;
        iseq $lex_a, 44;
        iseq $lex_c, 43;

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
        my @b = grep /\S/, @a;
        my @c = grep /[^\s]/, @a;
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
     
        local $Message  = "Ref stringification";
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
        local $BugId = '20000731.001';
        ok "A \x{263a} B z C" =~ /A . B (??{ "z" }) C/,
           "Match UTF-8 char in presense of (??{ })";
    }


    {
        local $BugId = '20001021.005';
        no warnings 'uninitialized';
        ok undef =~ /^([^\/]*)(.*)$/, "Used to cause a SEGV";
    }


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
        local $Message = "No SEGV in s/// and UTF-8";
        my $s = "s#\x{100}" x 4;
        ok $s =~ s/[^\w]/ /g;
        if ($ENV {REAL_POSIX_CC}) {
            iseq $s, "s  " x 4;
        }
        else {
            iseq $s, "s \x{100}" x 4;
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
        our $a = bless qr /foo/ => 'Foo';
        ok 'goodfood' =~ $a,     "Reblessed qr // matches";
        iseq $a, '(?-xism:foo)', "Reblessed qr // stringifies";
        my $x = "\x{3fe}";
        my $z = my $y = "\317\276";  # Byte representation of $x
        $a = qr /$x/;
        ok $x =~ $a, "UTF-8 interpolation in qr //";
        ok "a$a" =~ $x, "Stringified qr // preserves UTF-8";
        ok "a$x" =~ /^a$a\z/, "Interpolated qr // preserves UTF-8";
        ok "a$x" =~ /^a(??{$a})\z/,
                        "Postponed interpolation of qr // preserves UTF-8";
        {
            local $BugId = '17776';
            iseq length qr /##/x, 12, "## in qr // doesn't corrupt memory";
        }
        {
            use re 'eval';
            ok "$x$x" =~ /^$x(??{$x})\z/,
               "Postponed UTF-8 string in UTF-8 re matches UTF-8";
            ok "$y$x" =~ /^$y(??{$x})\z/, 
               "Postponed UTF-8 string in non-UTF-8 re matches UTF-8";
            ok "$y$x" !~ /^$y(??{$y})\z/,
               "Postponed non-UTF-8 string in non-UTF-8 re doesn't match UTF-8";
            ok "$x$x" !~ /^$x(??{$y})\z/,
               "Postponed non-UTF-8 string in UTF-8 re doesn't match UTF-8";
            ok "$y$y" =~ /^$y(??{$y})\z/,
               "Postponed non-UTF-8 string in non-UTF-8 re matches non-UTF8";
            ok "$x$y" =~ /^$x(??{$y})\z/,
               "Postponed non-UTF-8 string in UTF-8 re matches non-UTF8";

            $y = $z;  # Reset $y after upgrade.
            ok "$x$y" !~ /^$x(??{$x})\z/,
               "Postponed UTF-8 string in UTF-8 re doesn't match non-UTF-8";
            ok "$y$y" !~ /^$y(??{$x})\z/,
               "Postponed UTF-8 string in non-UTF-8 re doesn't match non-UTF-8";
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
        local $\;
        $_ = 'aaaaaaaaaa';
        utf8::upgrade($_); chop $_; $\="\n";
        ok /[^\s]+/, 'm/[^\s]/ utf8';
        ok /[^\d]+/, 'm/[^\d]/ utf8';
        ok +($a = $_, $_ =~ s/[^\s]+/./g), 's/[^\s]/ utf8';
        ok +($a = $_, $a =~ s/[^\d]+/./g), 's/[^\s]/ utf8';
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
        no warnings 'deprecated', 'syntax';
        split /(?{'WOW'})/, 'abc';
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
        local $BugId = '17757';
        $_ = "code:   'x' { '...' }\n"; study;
        my @x; push @x, $& while m/'[^\']*'/gx;
        local $" = ":";
        iseq "@x", "'x':'...'", "Parse::RecDescent triggered infinite loop";
    }


    {
        my $re = qq /^([^X]*)X/;
        utf8::upgrade ($re);
        ok "\x{100}X" =~ /$re/, "S_cl_and ANYOF_UNICODE & ANYOF_INVERTED";
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


  SKIP:
    {
        print "# Set PERL_SKIP_PSYCHO_TEST to skip this test\n";
        my @normal = qw [the are some normal words];

        skip "Skipped Psycho", 2 * @normal if $ENV {PERL_SKIP_PSYCHO_TEST};

        local $" = "|";

        my @psycho = (@normal, map chr $_, 255 .. 20000);
        my $psycho1 = "@psycho";
        for (my $i = @psycho; -- $i;) {
            my $j = int rand (1 + $i);
            @psycho [$i, $j] = @psycho [$j, $i];
        }
        my $psycho2 = "@psycho";

        foreach my $word (@normal) {
            ok $word =~ /($psycho1)/ && $1 eq $word, 'Psycho';
            ok $word =~ /($psycho2)/ && $1 eq $word, 'Psycho';
        }
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
        use lib 'lib';
        use Cname;
        
        ok 'fooB'  =~ /\N{foo}[\N{B}\N{b}]/, "Passthrough charname";
        my $test   = 1233;
        #
        # Why doesn't must_warn work here?
        #
        my $w;
        local $SIG {__WARN__} = sub {$w .= "@_"};
        eval 'q(xxWxx) =~ /[\N{WARN}]/';
        ok $w && $w =~ /^Ignoring excess chars from/,
                 "Ignoring excess chars warning";

        undef $w;
        eval q [ok "\0" !~ /[\N{EMPTY-STR}XY]/,
                   "Zerolength charname in charclass doesn't match \\0"];
        ok $w && $w =~ /^Ignoring zero length/,
                 'Ignoring zero length \N{%} in character class warning';

        ok 'AB'  =~ /(\N{EVIL})/ && $1 eq 'A', 'Charname caching $1';
        ok 'ABC' =~ /(\N{EVIL})/,              'Charname caching $1';
        ok 'xy'  =~ /x\N{EMPTY-STR}y/,
                    'Empty string charname produces NOTHING node';
        ok ''    =~ /\N{EMPTY-STR}/,
                    'Empty string charname produces NOTHING node';
            
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
            'a'.pack('U', 0x00B2),        # a + superscript two
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


  SKIP:
    {
        # stress test CURLYX/WHILEM.
        #
        # This test includes varying levels of nesting, and according to
        # profiling done against build 28905, exercises every code line in the
        # CURLYX and WHILEM blocks, except those related to LONGJMP, the
        # super-linear cache and warnings. It executes about 0.5M regexes

        skip "No psycho tests" if $ENV {PERL_SKIP_PSYCHO_TEST};
        print "# Set PERL_SKIP_PSYCHO_TEST to skip this test\n";
        my $r = qr/^
                    (?:
                        ( (?:a|z+)+ )
                        (?:
                            ( (?:b|z+){3,}? )
                            (
                                (?:
                                    (?:
                                        (?:c|z+){1,1}?z
                                    )?
                                    (?:c|z+){1,1}
                                )*
                            )
                            (?:z*){2,}
                            ( (?:z+|d)+ )
                            (?:
                                ( (?:e|z+)+ )
                            )*
                            ( (?:f|z+)+ )
                        )*
                        ( (?:z+|g)+ )
                        (?:
                            ( (?:h|z+)+ )
                        )*
                        ( (?:i|z+)+ )
                    )+
                    ( (?:j|z+)+ )
                    (?:
                        ( (?:k|z+)+ )
                    )*
                    ( (?:l|z+)+ )
              $/x;
          
        my $ok = 1;
        my $msg = "CURLYX stress test";
        OUTER:
          for my $a ("x","a","aa") {
            for my $b ("x","bbb","bbbb") {
              my $bs = $a.$b;
              for my $c ("x","c","cc") {
                my $cs = $bs.$c;
                for my $d ("x","d","dd") {
                  my $ds = $cs.$d;
                  for my $e ("x","e","ee") {
                    my $es = $ds.$e;
                    for my $f ("x","f","ff") {
                      my $fs = $es.$f;
                      for my $g ("x","g","gg") {
                        my $gs = $fs.$g;
                        for my $h ("x","h","hh") {
                          my $hs = $gs.$h;
                          for my $i ("x","i","ii") {
                            my $is = $hs.$i;
                            for my $j ("x","j","jj") {
                              my $js = $is.$j;
                              for my $k ("x","k","kk") {
                                my $ks = $js.$k;
                                for my $l ("x","l","ll") {
                                  my $ls = $ks.$l;
                                  if ($ls =~ $r) {
                                    if ($ls =~ /x/) {
                                      $msg .= ": unexpected match for [$ls]";
                                      $ok = 0;
                                      last OUTER;
                                    }
                                    my $cap = "$1$2$3$4$5$6$7$8$9$10$11$12";
                                    unless ($ls eq $cap) {
                                      $msg .= ": capture: [$ls], got [$cap]";
                                      $ok = 0;
                                      last OUTER;
                                    }
                                  }
                                  else {
                                    unless ($ls =~ /x/) {
                                      $msg = ": failed for [$ls]";
                                      $ok = 0;
                                      last OUTER;
                                    }
                                  }
                                }
                              }
                            }
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
        }
        ok($ok, $msg);
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
        local $BugId = '40684';
        local $Message = '/m in precompiled regexp';
        my $s = "abc\ndef";
        my $rex = qr'^abc$'m;
        ok $s =~ m/$rex/;
        ok $s =~ m/^abc$/m;
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
        ok /foo $parens \s* \+ \s* bar $parens/x;
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
        local $BugId = '45605';
        # [perl #45605] Regexp failure with utf8-flagged and byte-flagged string

        my $utf_8 = "\xd6schel";
        utf8::upgrade ($utf_8);
        $utf_8 =~ m {(\xd6|&Ouml;)schel};
        iseq $1, "\xd6", "Upgrade error";
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
        our $a = 3; "" =~ /(??{ $a })/;
        our $b = $a;
        iseq $b, $a, "Copy of scalar used for postponed subexpression";
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
	# for 5.10.x, add a dummy test indead
        #must_warn 'qr/\400/', '^Use of octal value above 377';
	$Message=""; ok 1;
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
        iseq join ('', @isPrint), "\x09\x0a\x0b\x0c\x0d\x85",
            'IsPrint disagrees with [:print:] on control characters';

        my @isPunct = grep {/[[:punct:]]/ != /\p{IsPunct}/}
                            map {chr} 0x80 .. 0xff;
        iseq join ('', @isPunct), "\xa1\xab\xb7\xbb\xbf",	# ¡ « · » ¿
            'IsPunct disagrees with [:punct:] outside ASCII';

        my @isPunctLatin1 = eval q {
            use encoding 'latin1';
            grep {/[[:punct:]]/ != /\p{IsPunct}/} map {chr} 0x80 .. 0xff;
        };
        skip "Eval failed ($@)", 1 if $@;
        skip "PERL_LEGACY_UNICODE_CHARCLASS_MAPPINGS set to 0", 1
              if $ENV {REAL_POSIX_CC};
        iseq join ('', @isPunctLatin1), '', 
            'IsPunct agrees with [:punct:] with explicit Latin1';
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


    {
        use re 'eval';
        local $Message = 'Test if $^N and $+ work in (?{{})';
        our @ctl_n = ();
        our @plus = ();
        our $nested_tags;
        $nested_tags = qr{
            <
               ((\w)+)
               (?{
                       push @ctl_n, (defined $^N ? $^N : "undef");
                       push @plus, (defined $+ ? $+ : "undef");
               })
            >
            (??{$nested_tags})*
            </\s* \w+ \s*>
        }x;


        my $c = 0;
        for my $test (
            # Test structure:
            #  [ Expected result, Regex, Expected value(s) of $^N, Expected value(s) of $+ ]
            [ 1, qr#^$nested_tags$#, "bla blubb bla", "a b a" ],
            [ 1, qr#^($nested_tags)$#, "bla blubb <bla><blubb></blubb></bla>", "a b a" ],
            [ 1, qr#^(|)$nested_tags$#, "bla blubb bla", "a b a" ],
            [ 1, qr#^(?:|)$nested_tags$#, "bla blubb bla", "a b a" ],
            [ 1, qr#^<(bl|bla)>$nested_tags<(/\1)>$#, "blubb /bla", "b /bla" ],
            [ 1, qr#(??{"(|)"})$nested_tags$#, "bla blubb bla", "a b a" ],
            [ 1, qr#^(??{"(bla|)"})$nested_tags$#, "bla blubb bla", "a b a" ],
            [ 1, qr#^(??{"(|)"})(??{$nested_tags})$#, "bla blubb undef", "a b undef" ],
            [ 1, qr#^(??{"(?:|)"})$nested_tags$#, "bla blubb bla", "a b a" ],
            [ 1, qr#^((??{"(?:bla|)"}))((??{$nested_tags}))$#, "bla blubb <bla><blubb></blubb></bla>", "a b <bla><blubb></blubb></bla>" ],
            [ 1, qr#^((??{"(?!)?"}))((??{$nested_tags}))$#, "bla blubb <bla><blubb></blubb></bla>", "a b <bla><blubb></blubb></bla>" ],
            [ 1, qr#^((??{"(?:|<(/?bla)>)"}))((??{$nested_tags}))\1$#, "bla blubb <bla><blubb></blubb></bla>", "a b <bla><blubb></blubb></bla>" ],
            [ 0, qr#^((??{"(?!)"}))?((??{$nested_tags}))(?!)$#, "bla blubb undef", "a b undef" ],

        ) { #"#silence vim highlighting
            $c++;
            @ctl_n = ();
            @plus = ();
            my $match = (("<bla><blubb></blubb></bla>" =~ $test->[1]) ? 1 : 0);
            push @ctl_n, (defined $^N ? $^N : "undef");
            push @plus, (defined $+ ? $+ : "undef");
            ok($test->[0] == $match, "match $c");
            if ($test->[0] != $match) {
              # unset @ctl_n and @plus
              @ctl_n = @plus = ();
            }
            iseq("@ctl_n", $test->[2], "ctl_n $c");
            iseq("@plus", $test->[3], "plus $c");
        }
    }

    {
        use re 'eval';
        local $BugId = '56194';

	our $f;
	local $f;
	$f = sub {
            defined $_[0] ? $_[0] : "undef";
        };

        ok("123" =~ m/^(\d)(((??{1 + $^N})))+$/);

        our @ctl_n;
        our @plus;

        my $re  = qr#(1)((??{ push @ctl_n, $f->($^N); push @plus, $f->($+); $^N + 1}))*(?{$^N})#;
        my $re2 = qr#(1)((??{ push @ctl_n, $f->($^N); push @plus, $f->($+); $^N + 1}))*(?{$^N})(|a(b)c|def)(??{"$^R"})#;
        my $re3 = qr#(1)((??{ push @ctl_n, $f->($^N); push @plus, $f->($+); $^N + 1})){2}(?{$^N})(|a(b)c|def)(??{"$^R"})#;
        our $re5;
        local $re5 = qr#(1)((??{ push @ctl_n, $f->($^N); push @plus, $f->($+); $^N + 1})){2}(?{$^N})#;
        my $re6 = qr#(??{ push @ctl_n, $f->($^N); push @plus, $f->($+); $^N + 1})#;
        my $re7 = qr#(??{ push @ctl_n, $f->($^N); push @plus, $f->($+); $^N + 1})#;
        my $re8 = qr/(\d+)/;
        my $c = 0;
        for my $test (
             # Test structure:
             #  [
             #    String to match
             #    Regex too match
             #    Expected values of $^N
             #    Expected values of $+
             #    Expected values of $1, $2, $3, $4 and $5
             #  ]
             [
                  "1233",
                  qr#^(1)((??{ push @ctl_n, $f->($^N); push @plus, $f->($+); $^N + 1}))+(??{$^N})$#,
                  "1 2 3 3",
                  "1 2 3 3",
                  "\$1 = 1, \$2 = 3, \$3 = undef, \$4 = undef, \$5 = undef",
             ],
             [
                  "1233",
                  qr#^(1)((??{ push @ctl_n, $f->($^N); push @plus, $f->($+); $^N + 1}))+(abc|def|)?(??{$+})$#,
                  "1 2 3 3",
                  "1 2 3 3",
                  "\$1 = 1, \$2 = 3, \$3 = undef, \$4 = undef, \$5 = undef",
             ],
             [
                  "1233",
                  qr#^(1)((??{ push @ctl_n, $f->($^N); push @plus, $f->($+); $^N + 1}))+(|abc|def)?(??{$+})$#,
                  "1 2 3 3",
                  "1 2 3 3",
                  "\$1 = 1, \$2 = 3, \$3 = undef, \$4 = undef, \$5 = undef",
             ],
             [
                  "1233",
                  qr#^(1)((??{ push @ctl_n, $f->($^N); push @plus, $f->($+); $^N + 1}))+(abc|def|)?(??{$^N})$#,
                  "1 2 3 3",
                  "1 2 3 3",
                  "\$1 = 1, \$2 = 3, \$3 = undef, \$4 = undef, \$5 = undef",
             ],
             [
                  "1233",
                  qr#^(1)((??{ push @ctl_n, $f->($^N); push @plus, $f->($+); $^N + 1}))+(|abc|def)?(??{$^N})$#,
                  "1 2 3 3",
                  "1 2 3 3",
                  "\$1 = 1, \$2 = 3, \$3 = undef, \$4 = undef, \$5 = undef",
              ],
              [
                  "123abc3",
                   qr#^($re)(|a(b)c|def)(??{$^R})$#,
                   "1 2 3 abc",
                   "1 2 3 b",
                   "\$1 = 123, \$2 = 1, \$3 = 3, \$4 = abc, \$5 = b",
              ],
              [
                  "123abc3",
                   qr#^($re2)$#,
                   "1 2 3 123abc3",
                   "1 2 3 b",
                   "\$1 = 123abc3, \$2 = 1, \$3 = 3, \$4 = abc, \$5 = b",
              ],
              [
                  "123abc3",
                   qr#^($re3)$#,
                   "1 2 123abc3",
                   "1 2 b",
                   "\$1 = 123abc3, \$2 = 1, \$3 = 3, \$4 = abc, \$5 = b",
              ],
              [
                  "123abc3",
                   qr#^(??{$re5})(|abc|def)(??{"$^R"})$#,
                   "1 2 abc",
                   "1 2 abc",
                   "\$1 = abc, \$2 = undef, \$3 = undef, \$4 = undef, \$5 = undef",
              ],
              [
                  "123abc3",
                   qr#^(??{$re5})(|a(b)c|def)(??{"$^R"})$#,
                   "1 2 abc",
                   "1 2 b",
                   "\$1 = abc, \$2 = b, \$3 = undef, \$4 = undef, \$5 = undef",
              ],
              [
                  "1234",
                   qr#^((\d+)((??{push @ctl_n, $f->($^N); push @plus, $f->($+);$^N + 1}))((??{push @ctl_n, $f->($^N); push @plus, $f->($+);$^N + 1}))((??{push @ctl_n, $f->($^N); push @plus, $f->($+);$^N + 1})))$#,
                   "1234 123 12 1 2 3 1234",
                   "1234 123 12 1 2 3 4",
                   "\$1 = 1234, \$2 = 1, \$3 = 2, \$4 = 3, \$5 = 4",
              ],
              [
                   "1234556",
                   qr#^(\d+)($re6)($re6)($re6)$re6(($re6)$re6)$#,
                   "1234556 123455 12345 1234 123 12 1 2 3 4 4 5 56",
                   "1234556 123455 12345 1234 123 12 1 2 3 4 4 5 5",
                   "\$1 = 1, \$2 = 2, \$3 = 3, \$4 = 4, \$5 = 56",
              ],
              [
                  "12345562",
                   qr#^((??{$re8}))($re7)($re7)($re7)$re7($re7)($re7(\2))$#,
                   "12345562 1234556 123455 12345 1234 123 12 1 2 3 4 4 5 62",
                   "12345562 1234556 123455 12345 1234 123 12 1 2 3 4 4 5 2",
                   "\$1 = 1, \$2 = 2, \$3 = 3, \$4 = 4, \$5 = 5",
              ],
        ) {
            $c++;
            @ctl_n = ();
            @plus = ();
            undef $^R;
            my $match = $test->[0] =~ $test->[1];
            my $str = join(", ", '$1 = '.$f->($1), '$2 = '.$f->($2), '$3 = '.$f->($3), '$4 = '.$f->($4),'$5 = '.$f->($5));
            push @ctl_n, $f->($^N);
            push @plus, $f->($+);
            ok($match, "match $c");
            if (not $match) {
                # unset $str, @ctl_n and @plus
                $str = "";
                @ctl_n = @plus = ();
            }
            iseq("@ctl_n", $test->[2], "ctl_n $c");
            iseq("@plus", $test->[3], "plus $c");
            iseq($str, $test->[4], "str $c");
        }
        SKIP: {
            if ($] le '5.010') {
                skip "test segfaults on perl < 5.10", 4;
            }

            @ctl_n = ();
            @plus = ();

            our $re4;
            local $re4 = qr#(1)((??{push @ctl_n, $f->($^N); push @plus, $f->($+);$^N + 1})){2}(?{$^N})(|abc|def)(??{"$^R"})#;
            undef $^R;
            my $match = "123abc3" =~ m/^(??{$re4})$/;
            my $str = join(", ", '$1 = '.$f->($1), '$2 = '.$f->($2), '$3 = '.$f->($3), '$4 = '.$f->($4),'$5 = '.$f->($5),'$^R = '.$f->($^R));
            push @ctl_n, $f->($^N);
            push @plus, $f->($+);
            ok($match);
            if (not $match) {
                # unset $str
                @ctl_n = ();
                @plus = ();
                $str = "";
            }
            iseq("@ctl_n", "1 2 undef");
            iseq("@plus", "1 2 undef");
            iseq($str, "\$1 = undef, \$2 = undef, \$3 = undef, \$4 = undef, \$5 = undef, \$^R = undef");
       }
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
    #
    # This should be the last test.
    #
    iseq $test + 1, $EXPECTED_TESTS, "Got the right number of tests!";

} # End of sub run_tests

1;
