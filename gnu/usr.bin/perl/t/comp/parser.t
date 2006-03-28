#!./perl

# Checks if the parser behaves correctly in edge cases
# (including weird syntax errors)

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

require "./test.pl";
plan( tests => 53 );

eval '%@x=0;';
like( $@, qr/^Can't modify hash dereference in repeat \(x\)/, '%@x=0' );

# Bug 20010422.005
eval q{{s//${}/; //}};
like( $@, qr/syntax error/, 'syntax error, used to dump core' );

# Bug 20010528.007
eval q/"\x{"/;
like( $@, qr/^Missing right brace on \\x/,
    'syntax error in string, used to dump core' );

eval "a.b.c.d.e.f;sub";
like( $@, qr/^Illegal declaration of anonymous subroutine/,
    'found by Markov chain stress testing' );

# Bug 20010831.001
eval '($a, b) = (1, 2);';
like( $@, qr/^Can't modify constant item in list assignment/,
    'bareword in list assignment' );

eval 'tie FOO, "Foo";';
like( $@, qr/^Can't modify constant item in tie /,
    'tying a bareword causes a segfault in 5.6.1' );

eval 'undef foo';
like( $@, qr/^Can't modify constant item in undef operator /,
    'undefing constant causes a segfault in 5.6.1 [ID 20010906.019]' );

eval 'read($bla, FILE, 1);';
like( $@, qr/^Can't modify constant item in read /,
    'read($var, FILE, 1) segfaults on 5.6.1 [ID 20011025.054]' );

# This used to dump core (bug #17920)
eval q{ sub { sub { f1(f2();); my($a,$b,$c) } } };
like( $@, qr/error/, 'lexical block discarded by yacc' );

# bug #18573, used to corrupt memory
eval q{ "\c" };
like( $@, qr/^Missing control char name in \\c/, q("\c" string) );

eval q{ qq(foo$) };
like( $@, qr/Final \$ should be \\\$ or \$name/, q($ at end of "" string) );

# two tests for memory corruption problems in the said variables
# (used to dump core or produce strange results)

is( "\Q\Q\Q\Q\Q\Q\Q\Q\Q\Q\Q\Q\Qa", "a", "PL_lex_casestack" );

eval {
{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{
{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{
{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{
}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}
}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}
}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}
};
is( $@, '', 'PL_lex_brackstack' );

{
    # tests for bug #20716
    undef $a;
    undef @b;
    my $a="A";
    is("${a}{", "A{", "interpolation, qq//");
    is("${a}[", "A[", "interpolation, qq//");
    my @b=("B");
    is("@{b}{", "B{", "interpolation, qq//");
    is(qr/${a}{/, '(?-xism:A{)', "interpolation, qr//");
    my $c = "A{";
    $c =~ /${a}{/;
    is($&, 'A{', "interpolation, m//");
    $c =~ s/${a}{/foo/;
    is($c, 'foo', "interpolation, s/...//");
    $c =~ s/foo/${a}{/;
    is($c, 'A{', "interpolation, s//.../");
    is(<<"${a}{", "A{ A[ B{\n", "interpolation, here doc");
${a}{ ${a}[ @{b}{
${a}{
}

eval q{ sub a(;; &) { } a { } };
is($@, '', "';&' sub prototype confuses the lexer");

# Bug #21575
# ensure that the second print statement works, by playing a bit
# with the test output.
my %data = ( foo => "\n" );
print "#";
print(
$data{foo});
pass();

# Bug #21875
# { q.* => ... } should be interpreted as hash, not block

foreach my $line (split /\n/, <<'EOF')
1 { foo => 'bar' }
1 { qoo => 'bar' }
1 { q   => 'bar' }
1 { qq  => 'bar' }
0 { q,'bar', }
0 { q=bar= }
0 { qq=bar= }
1 { q=bar= => 'bar' }
EOF
{
    my ($expect, $eval) = split / /, $line, 2;
    my $result = eval $eval;
    ok($@ eq  '', "eval $eval");
    is(ref $result, $expect ? 'HASH' : '', $eval);
}

# Bug #24212
{
    local $SIG{__WARN__} = sub { }; # silence mandatory warning
    eval q{ my $x = -F 1; };
    like( $@, qr/(?:syntax|parse) error .* near "F 1"/, "unknown filetest operators" );
    is(
        eval q{ sub F { 42 } -F 1 },
	'-42',
	'-F calls the F function'
    );
}

# Bug #24762
{
    eval q{ *foo{CODE} ? 1 : 0 };
    is( $@, '', "glob subscript in conditional" );
}

# Bug #27024
{
    # this used to segfault (because $[=1 is optimized away to a null block)
    my $x;
    $[ = 1 while $x;
    pass();
    $[ = 0; # restore the original value for less side-effects
}

# [perl #2738] perl segfautls on input
{
    eval q{ sub _ <> {} };
    like($@, qr/Illegal declaration of subroutine main::_/, "readline operator as prototype");

    eval q{ $s = sub <> {} };
    like($@, qr/Illegal declaration of anonymous subroutine/, "readline operator as prototype");

    eval q{ sub _ __FILE__ {} };
    like($@, qr/Illegal declaration of subroutine main::_/, "__FILE__ as prototype");
}

# [perl #36313] perl -e "1for$[=0" crash
{
    my $x;
    $x = 1 for ($[) = 0;
    pass('optimized assignment to $[ used to segfault in list context');
    if ($[ = 0) { $x = 1 }
    pass('optimized assignment to $[ used to segfault in scalar context');
    $x = ($[=2.4);
    is($x, 2, 'scalar assignment to $[ behaves like other variables');
    $x = (($[) = 0);
    is($x, 1, 'list assignment to $[ behaves like other variables');
    $x = eval q{ ($[, $x) = (0) };
    like($@, qr/That use of \$\[ is unsupported/,
             'cannot assign to $[ in a list');
    eval q{ ($[) = (0, 1) };
    like($@, qr/That use of \$\[ is unsupported/,
             'cannot assign list of >1 elements to $[');
    eval q{ ($[) = () };
    like($@, qr/That use of \$\[ is unsupported/,
             'cannot assign list of <1 elements to $[');
}
