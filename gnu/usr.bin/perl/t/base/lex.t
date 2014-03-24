#!./perl

print "1..85\n";

$x = 'x';

print "#1	:$x: eq :x:\n";
if ($x eq 'x') {print "ok 1\n";} else {print "not ok 1\n";}

$x = $#[0];

if ($x eq '') {print "ok 2\n";} else {print "not ok 2\n";}

$x = $#x;

if ($x eq '-1') {print "ok 3\n";} else {print "not ok 3\n";}

$x = '\\'; # ';

if (length($x) == 1) {print "ok 4\n";} else {print "not ok 4\n";}

eval 'while (0) {
    print "foo\n";
}
/^/ && (print "ok 5\n");
';

eval '$foo{1} / 1;';
if (!$@) {print "ok 6\n";} else {print "not ok 6 $@\n";}

eval '$foo = 123+123.4+123e4+123.4E5+123.4e+5+.12;';

$foo = int($foo * 100 + .5);
if ($foo eq 2591024652) {print "ok 7\n";} else {print "not ok 7 :$foo:\n";}

print <<'EOF';
ok 8
EOF

$foo = 'ok 9';
print <<EOF;
$foo
EOF

eval <<\EOE, print $@;
print <<'EOF';
ok 10
EOF

$foo = 'ok 11';
print <<EOF;
$foo
EOF
EOE

print <<'EOS' . <<\EOF;
ok 12 - make sure single quotes are honored \nnot ok
EOS
ok 13
EOF

print qq/ok 14\n/;
print qq(ok 15\n);

print qq
[ok 16\n]
;

print q<ok 17
>;

print "ok 18 - was the test for the deprecated use of bare << to mean <<\"\"\n";
#print <<;   # Yow!
#ok 18
#
## previous line intentionally left blank.

print <<E1 eq "foo\n\n" ? "ok 19\n" : "not ok 19\n";
@{[ <<E2 ]}
foo
E2
E1

print <<E1 eq "foo\n\n" ? "ok 20\n" : "not ok 20\n";
@{[
  <<E2
foo
E2
]}
E1

$foo = FOO;
$bar = BAR;
$foo{$bar} = BAZ;
$ary[0] = ABC;

print "$foo{$bar}" eq "BAZ" ? "ok 21\n" : "not ok 21\n";

print "${foo}{$bar}" eq "FOO{BAR}" ? "ok 22\n" : "not ok 22\n";
print "${foo{$bar}}" eq "BAZ" ? "ok 23\n" : "not ok 23\n";

print "FOO:" =~ /$foo[:]/ ? "ok 24\n" : "not ok 24\n";
print "ABC" =~ /^$ary[$A]$/ ? "ok 25\n" : "not ok 25\n";
print "FOOZ" =~ /^$foo[$A-Z]$/ ? "ok 26\n" : "not ok 26\n";

# MJD 19980425
($X, @X) = qw(a b c d); 
print "d" =~ /^$X[-1]$/ ? "ok 27\n" : "not ok 27\n";
print "a1" !~ /^$X[-1]$/ ? "ok 28\n" : "not ok 28\n";

print (((q{{\{\(}} . q{{\)\}}}) eq '{{\(}{\)}}') ? "ok 29\n" : "not ok 29\n");


$foo = "not ok 30\n";
$foo =~ s/^not /substr(<<EOF, 0, 0)/e;
  Ignored
EOF
print $foo;

# Tests for new extended control-character variables
# MJD 19990227

{ my $CX = "\cX";
  my $CXY  ="\cXY";
  $ {$CX} = 17;
  $ {$CXY} = 23;
  if ($ {^XY} != 23) { print "not "  }
  print "ok 31\n";
 
# Does the syntax where we use the literal control character still work?
  if (eval "\$ {\cX}" != 17 or $@) { print "not "  }
  print "ok 32\n";

  eval "\$\cQ = 24";                 # Literal control character
  if ($@ or ${"\cQ"} != 24) {  print "not "  }
  print "ok 33\n";
  if ($^Q != 24) {  print "not "  }  # Control character escape sequence
  print "ok 34\n";

# Does the old UNBRACED syntax still do what it used to?
  if ("$^XY" ne "17Y") { print "not " }
  print "ok 35\n";

  sub XX () { 6 }
  $ {"\cQ\cXX"} = 119; 
  $^Q = 5; #  This should be an unused ^Var.
  $N = 5;
  # The second caret here should be interpreted as an xor
  if (($^Q^XX) != 3) { print "not " } 
  print "ok 36\n";
#  if (($N  ^  XX()) != 3) { print "not " } 
#  print "ok 32\n";

  # These next two tests are trying to make sure that
  # $^FOO is always global; it doesn't make sense to 'my' it.
  # 

  eval 'my $^X;';
  print "not " unless index ($@, 'Can\'t use global $^X in "my"') > -1;
  print "ok 37\n";
#  print "($@)\n" if $@;

  eval 'my $ {^XYZ};';
  print "not " unless index ($@, 'Can\'t use global $^XYZ in "my"') > -1;
  print "ok 38\n";
#  print "($@)\n" if $@;

# Now let's make sure that caret variables are all forced into the main package.
  package Someother;
  $^Q = 'Someother';
  $ {^Quixote} = 'Someother 2';
  $ {^M} = 'Someother 3';
  package main;
  print "not " unless $^Q eq 'Someother';
  print "ok 39\n";
  print "not " unless $ {^Quixote} eq 'Someother 2';
  print "ok 40\n";
  print "not " unless $ {^M} eq 'Someother 3';
  print "ok 41\n";

  
}

# see if eval '', s///e, and heredocs mix

sub T {
    my ($where, $num) = @_;
    my ($p,$f,$l) = caller;
    print "# $p:$f:$l vs /$where/\nnot " unless "$p:$f:$l" =~ /$where/;
    print "ok $num\n";
}

my $test = 42;

{
# line 42 "plink"
    local $_ = "not ok ";
    eval q{
	s/^not /<<EOT/e and T '^main:\(eval \d+\):2$', $test++;
# uggedaboudit
EOT
        print $_, $test++, "\n";
	T('^main:\(eval \d+\):6$', $test++);
# line 1 "plunk"
	T('^main:plunk:1$', $test++);
    };
    print "# $@\nnot ok $test\n" if $@;
    T '^main:plink:53$', $test++;
}

# tests 47--51 start here
# tests for new array interpolation semantics:
# arrays now *always* interpolate into "..." strings.
# 20000522 MJD (mjd@plover.com)
{
  my $test = 47;
  eval(q(">@nosuch<" eq "><")) || print "# $@", "not ";
  print "ok $test\n";
  ++$test;

  # Look at this!  This is going to be a common error in the future:
  eval(q("fred@example.com" eq "fred.com")) || print "# $@", "not ";
  print "ok $test\n";
  ++$test;

  # Let's make sure that normal array interpolation still works right
  # For some reason, this appears not to be tested anywhere else.
  my @a = (1,2,3);
  print +((">@a<" eq ">1 2 3<") ? '' : 'not '), "ok $test\n";
  ++$test;

  # Ditto.
  eval(q{@nosuch = ('a', 'b', 'c'); ">@nosuch<" eq ">a b c<"}) 
      || print "# $@", "not ";
  print "ok $test\n";
  ++$test;

  # This isn't actually a lex test, but it's testing the same feature
  sub makearray {
    my @array = ('fish', 'dog', 'carrot');
    *R::crackers = \@array;
  }

  eval(q{makearray(); ">@R::crackers<" eq ">fish dog carrot<"})
    || print "# $@", "not ";
  print "ok $test\n";
  ++$test;
}

# Tests 52-54
# => should only quote foo::bar if it isn't a real sub. AMS, 20010621

sub xyz::foo { "bar" }
my %str = (
    foo      => 1,
    xyz::foo => 1,
    xyz::bar => 1,
);

my $test = 52;
print ((exists $str{foo}      ? "" : "not ")."ok $test\n"); ++$test;
print ((exists $str{bar}      ? "" : "not ")."ok $test\n"); ++$test;
print ((exists $str{xyz::bar} ? "" : "not ")."ok $test\n"); ++$test;

sub foo::::::bar { print "ok $test\n"; $test++ }
foo::::::bar;

eval "\$x =\xE2foo";
if ($@ =~ /Unrecognized character \\xE2; marked by <-- HERE after \$x =<-- HERE near column 5/) { print "ok $test\n"; } else { print "not ok $test\n"; }
$test++;

# Is "[~" scanned correctly?
@a = (1,2,3);
print "not " unless($a[~~2] == 3);
print "ok 57\n";

$_ = "";
eval 's/(?:)/"${\q||}".<<\END/e;
ok 58 - heredoc after "" in s/// in eval
END
';
print $_ || "not ok 58\n";

$_ = "";
eval 's|(?:)|"${\<<\END}"
ok 59 - heredoc in "" in multiline s///e in eval
END
|e
';
print $_ || "not ok 59\n";

$_ = "";
eval "s/(?:)/<<foo/e #\0
ok 60 - null on same line as heredoc in s/// in eval
foo
";
print $_ || "not ok 60\n";

$_ = "";
eval ' s/(?:)/"${\<<END}"/e;
ok 61 - heredoc in "" in single-line s///e in eval
END
';
print $_ || "not ok 61\n";

$_ = "";
s|(?:)|"${\<<END}"
ok 62 - heredoc in "" in multiline s///e outside eval
END
|e;
print $_ || "not ok 62\n";

$_ = "not ok 63 - s/// in s/// pattern\n";
s/${s|||;\""}not //;
print;

/(?{print <<END
ok 64 - here-doc in re-eval
END
})/;

eval '/(?{print <<END
ok 65 - here-doc in re-eval in string eval
END
})/';

eval 'print qq ;ok 66 - eval ending with semicolon\n;'
  or print "not ok 66 - eval ending with semicolon\n";

print "not " unless qr/(?{<<END})/ eq '(?^:(?{<<END}))';
foo
END
print "ok 67 - here-doc in single-line re-eval\n";

$_ = qr/(?{"${<<END}"
foo
END
})/;
print "not " unless /foo/;
print "ok 68 - here-doc in quotes in multiline re-eval\n";

eval 's//<<END/e if 0; $_ = "a
END
b"';
print "not " if $_ =~ /\n\n/;
print "ok 69 - eval 's//<<END/' does not leave extra newlines\n";

$_ = a;
eval "s/a/'b\0'#/e";
print 'not ' unless $_ eq "b\0";
print "ok 70 - # after null in s/// repl\n";

s//"#" . <<END/e;
foo
END
print "ok 71 - s//'#' . <<END/e\n";

eval "s//3}->{3/e";
print "not " unless $@;
print "ok 72 - s//3}->{3/e\n";

$_ = "not ok 73";
$x{3} = "not ";
eval 's/${\%x}{3}//e';
print "$_ - s//\${\\%x}{3}/e\n";

eval 's/${foo#}//e';
print "not " unless $@;
print "ok 74 - s/\${foo#}//e\n";

eval 'warn ({$_ => 1} + 1) if 0';
print "not " if $@;
print "ok 75 - listop({$_ => 1} + 1)\n";
print "# $@" if $@;

$test = 76;
for(qw< require goto last next redo dump >) {
    eval "sub { $_ foo << 2 }";
    print "not " if $@;
    print "ok ", $test++, " - [perl #105924] $_ WORD << ...\n";
    print "# $@" if $@;
}

# http://rt.perl.org/rt3/Ticket/Display.html?id=56880
my $counter = 0;
eval 'v23: $counter++; goto v23 unless $counter == 2';
print "not " unless $counter == 2;
print "ok 82 - Use v[0-9]+ as a label\n";
$counter = 0;
eval 'v23 : $counter++; goto v23 unless $counter == 2';
print "not " unless $counter == 2;
print "ok 83 - Use v[0-9]+ as a label with space before colon\n";
 
my $output = "";
eval "package v10::foo; sub test2 { return 'v10::foo' }
      package v10; sub test { return v10::foo::test2(); }
      package main; \$output = v10::test(); "; 
print "not " unless $output eq 'v10::foo';
print "ok 84 - call a function in package v10::foo\n";

print "not " unless (1?v65:"bar") eq 'A';
print "ok 85 - colon detection after vstring does not break ? vstring :\n";
