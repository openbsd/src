#!./perl

# Checks if the parser behaves correctly in edge cases
# (including weird syntax errors)

print "1..122\n";

sub failed {
    my ($got, $expected, $name) = @_;

    print "not ok $test - $name\n";
    my @caller = caller(1);
    print "# Failed test at $caller[1] line $caller[2]\n";
    if (defined $got) {
	print "# Got '$got'\n";
    } else {
	print "# Got undef\n";
    }
    print "# Expected $expected\n";
    return;
}

sub like {
    my ($got, $pattern, $name) = @_;
    $test = $test + 1;
    if (defined $got && $got =~ $pattern) {
	print "ok $test - $name\n";
	# Principle of least surprise - maintain the expected interface, even
	# though we aren't using it here (yet).
	return 1;
    }
    failed($got, $pattern, $name);
}

sub is {
    my ($got, $expect, $name) = @_;
    $test = $test + 1;
    if (defined $expect) {
	if (defined $got && $got eq $expect) {
	    print "ok $test - $name\n";
	    return 1;
	}
	failed($got, "'$expect'", $name);
    } else {
	if (!defined $got) {
	    print "ok $test - $name\n";
	    return 1;
	}
	failed($got, 'undef', $name);
    }
}

eval '%@x=0;';
like( $@, qr/^Can't modify hash dereference in repeat \(x\)/, '%@x=0' );

# Bug 20010422.005
eval q{{s//${}/; //}};
like( $@, qr/syntax error/, 'syntax error, used to dump core' );

# Bug 20010528.007
eval q/"\x{"/;
like( $@, qr/^Missing right brace on \\x/,
    'syntax error in string, used to dump core' );

eval q/"\N{"/;
like( $@, qr/^Missing right brace on \\N/,
    'syntax error in string with incomplete \N' );
eval q/"\Nfoo"/;
like( $@, qr/^Missing braces on \\N/,
    'syntax error in string with incomplete \N' );

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
$test = $test + 1;
print "ok $test\n";

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
    is($@, '', "eval $eval");
    is(ref $result, $expect ? 'HASH' : '', $eval);
}

# Bug #24212
{
    local $SIG{__WARN__} = sub { }; # silence mandatory warning
    eval q{ my $x = -F 1; };
    like( $@, qr/(?i:syntax|parse) error .* near "F 1"/, "unknown filetest operators" );
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

# Bug #25824
{
    eval q{ sub f { @a=@b=@c;  {use} } };
    like( $@, qr/syntax error/, "use without body" );
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

# tests for "Bad name"
eval q{ foo::$bar };
like( $@, qr/Bad name after foo::/, 'Bad name after foo::' );
eval q{ foo''bar };
like( $@, qr/Bad name after foo'/, 'Bad name after foo\'' );

# test for ?: context error
eval q{($a ? $x : ($y)) = 5};
like( $@, qr/Assignment to both a list and a scalar/, 'Assignment to both a list and a scalar' );

eval q{ s/x/#/e };
is( $@, '', 'comments in s///e' );

# these five used to coredump because the op cleanup on parse error could
# be to the wrong pad

eval q[
    sub { our $a= 1;$a;$a;$a;$a;$a;$a;$a;$a;$a;$a;$a;$a;$a;$a;$a;$a;$a;$a;$a;
	    sub { my $z
];

like($@, qr/Missing right curly/, 'nested sub syntax error' );

eval q[
    sub { my ($a,$b,$c,$d,$e,$f,$g,$h,$i,$j,$k,$l,$m,$n,$o,$p,$q,$r,$s,$r);
	    sub { my $z
];
like($@, qr/Missing right curly/, 'nested sub syntax error 2' );

eval q[
    sub { our $a= 1;$a;$a;$a;$a;$a;$a;$a;$a;$a;$a;$a;$a;$a;$a;$a;$a;$a;$a;$a;
	    use DieDieDie;
];

like($@, qr/Can't locate DieDieDie.pm/, 'croak cleanup' );

eval q[
    sub { my ($a,$b,$c,$d,$e,$f,$g,$h,$i,$j,$k,$l,$m,$n,$o,$p,$q,$r,$s,$r);
	    use DieDieDie;
];

like($@, qr/Can't locate DieDieDie.pm/, 'croak cleanup 2' );


eval q[
    my @a;
    my ($a,$b,$c,$d,$e,$f,$g,$h,$i,$j,$k,$l,$m,$n,$o,$p,$q,$r,$s,$r);
    @a =~ s/a/b/; # compile-time error
    use DieDieDie;
];

like($@, qr/Can't modify/, 'croak cleanup 3' );

# these might leak, or have duplicate frees, depending on the bugginess of
# the parser stack 'fail in reduce' cleanup code. They're here mainly as
# something to be run under valgrind, with PERL_DESTRUCT_LEVEL=1.

eval q[ BEGIN { } ] for 1..10;
is($@, "", 'BEGIN 1' );

eval q[ BEGIN { my $x; $x = 1 } ] for 1..10;
is($@, "", 'BEGIN 2' );

eval q[ BEGIN { \&foo1 } ] for 1..10;
is($@, "", 'BEGIN 3' );

eval q[ sub foo2 { } ] for 1..10;
is($@, "", 'BEGIN 4' );

eval q[ sub foo3 { my $x; $x=1 } ] for 1..10;
is($@, "", 'BEGIN 5' );

eval q[ BEGIN { die } ] for 1..10;
like($@, qr/BEGIN failed--compilation aborted/, 'BEGIN 6' );

eval q[ BEGIN {\&foo4; die } ] for 1..10;
like($@, qr/BEGIN failed--compilation aborted/, 'BEGIN 7' );

{
  # RT #70934
  # check both the specific case in the ticket, and a few other paths into
  # S_scan_ident()
  # simplify long ids
  my $x100 = "x" x 256;
  my $xFE = "x" x 254;
  my $xFD = "x" x 253;
  my $xFC = "x" x 252;
  my $xFB = "x" x 251;

  eval qq[ \$#$xFB ];
  is($@, "", "251 character \$# sigil ident ok");
  eval qq[ \$#$xFC ];
  like($@, qr/Identifier too long/, "too long id in \$# sigil ctx");

  eval qq[ \$$xFB ];
  is($@, "", "251 character \$ sigil ident ok");
  eval qq[ \$$xFC ];
  like($@, qr/Identifier too long/, "too long id in \$ sigil ctx");

  eval qq[ %$xFB ];
  is($@, "", "251 character % sigil ident ok");
  eval qq[ %$xFC ];
  like($@, qr/Identifier too long/, "too long id in % sigil ctx");

  eval qq[ \\&$xFC ]; # take a ref since I don't want to call it
  is($@, "", "252 character & sigil ident ok");
  eval qq[ \\&$xFD ];
  like($@, qr/Identifier too long/, "too long id in & sigil ctx");

  eval qq[ *$xFC ];
  is($@, "", "252 character glob ident ok");
  eval qq[ *$xFD ];
  like($@, qr/Identifier too long/, "too long id in glob ctx");

  eval qq[ for $xFD ];
  like($@, qr/Missing \$ on loop variable/,
       "253 char id ok, but a different error");
  eval qq[ for $xFE; ];
  like($@, qr/Identifier too long/, "too long id in for ctx");

  # the specific case from the ticket
  my $x = "x" x 257;
  eval qq[ for $x ];
  like($@, qr/Identifier too long/, "too long id ticket case");
}

{
  is(exists &zlonk, '', 'sub not present');
  eval qq[ {sub zlonk} ];
  is($@, '', 'sub declaration followed by a closing curly');
  is(exists &zlonk, 1, 'sub now stubbed');
  is(defined &zlonk, '', 'but no body defined');
}

# bug #71748
eval q{
	$_ = "";
	s/(.)/
	{
	    #
	}->{$1};
	/e;
	1;
};
is($@, "", "multiline whitespace inside substitute expression");

# Add new tests HERE:

# More awkward tests for #line. Keep these at the end, as they will screw
# with sane line reporting for any other test failures

sub check ($$$) {
    my ($file, $line, $name) =  @_;
    my (undef, $got_file, $got_line) = caller;
    like ($got_file, $file, "file of $name");
    is ($got_line, $line, "line of $name");
}

my $this_file = qr/parser\.t(?:\.[bl]eb?)?$/;
#line 3
check($this_file, 3, "bare line");

# line 5
check($this_file, 5, "bare line with leading space");

#line 7 
check($this_file, 7, "trailing space still valid");

# line 11 
check($this_file, 11, "leading and trailing");

#	line 13
check($this_file, 13, "leading tab");

#line	17
check($this_file, 17, "middle tab");

#line                                                                        19
check($this_file, 19, "loadsaspaces");

#line 23 KASHPRITZA
check(qr/^KASHPRITZA$/, 23, "bare filename");

#line 29 "KAHEEEE"
check(qr/^KAHEEEE$/, 29, "filename in quotes");

#line 31 "CLINK CLOINK BZZT"
check(qr/^CLINK CLOINK BZZT$/, 31, "filename with spaces in quotes");

#line 37 "THOOM	THOOM"
check(qr/^THOOM	THOOM$/, 37, "filename with tabs in quotes");

#line 41 "GLINK PLINK GLUNK DINK" 
check(qr/^GLINK PLINK GLUNK DINK$/, 41, "a space after the quotes");

#line 43 "BBFRPRAFPGHPP
check(qr/^"BBFRPRAFPGHPP$/, 43, "actually missing a quote is still valid");

#line 47 bang eth
check(qr/^"BBFRPRAFPGHPP$/, 46, "but spaces aren't allowed without quotes");

#line 77sevenseven
check(qr/^"BBFRPRAFPGHPP$/, 49, "need a space after the line number");

eval <<'EOSTANZA'; die $@ if $@;
#line 51 "With wonderful deathless ditties|We build up the world's great cities,|And out of a fabulous story|We fashion an empire's glory:|One man with a dream, at pleasure,|Shall go forth and conquer a crown;|And three with a new song's measure|Can trample a kingdom down."
check(qr/^With.*down\.$/, 51, "Overflow the second small buffer check");
EOSTANZA

# And now, turn on the debugger flag for long names
$^P = 0x100;

#line 53 "For we are afar with the dawning|And the suns that are not yet high,|And out of the infinite morning|Intrepid you hear us cry-|How, spite of your human scorning,|Once more God's future draws nigh,|And already goes forth the warning|That ye of the past must die."
check(qr/^For we.*must die\.$/, 53, "Our long line is set up");

eval <<'EOT'; die $@ if $@;
#line 59 " "
check(qr/^ $/, 59, "Overflow the first small buffer check only");
EOT

eval <<'EOSTANZA'; die $@ if $@;
#line 61 "Great hail! we cry to the comers|From the dazzling unknown shore;|Bring us hither your sun and your summers;|And renew our world as of yore;|You shall teach us your song's new numbers,|And things that we dreamed not before:|Yea, in spite of a dreamer who slumbers,|And a singer who sings no more."
check(qr/^Great hail!.*no more\.$/, 61, "Overflow both small buffer checks");
EOSTANZA

{
    my @x = 'string';
    is(eval q{ "$x[0]->strung" }, 'string->strung',
	'literal -> after an array subscript within ""');
    @x = ['string'];
    # this used to give "string"
    like("$x[0]-> [0]", qr/^ARRAY\([^)]*\)-> \[0]\z/,
	'literal -> [0] after an array subscript within ""');
}

__END__
# Don't add new tests HERE. See note above
