#!perl

# Test scoping issues with embedded code in regexps.

BEGIN {
    chdir 't';
    @INC = qw(lib ../lib);
    require './test.pl';
    skip_all_if_miniperl("no dynamic loading on miniperl, no re");
}

plan 18;

# Functions for turning to-do-ness on and off (as there are so many
# to-do tests) 
sub on { $::TODO = "(?{}) implementation is screwy" }
sub off { undef $::TODO }


fresh_perl_is <<'CODE', '781745', {}, '(?{}) has its own lexical scope';
 my $x = 7; my $a = 4; my $b = 5;
 print "a" =~ /(?{ print $x; my $x = 8; print $x; my $y })a/;
 print $x,$a,$b;
CODE

on;

fresh_perl_is <<'CODE',
 for my $x("a".."c") {
  $y = 1;
  print scalar
   "abcabc" =~
       /
        (
         a (?{ print $y; local $y = $y+1; print $x; my $x = 8; print $x })
         b (?{ print $y; local $y = $y+1; print $x; my $x = 9; print $x })
         c (?{ print $y; local $y = $y+1; print $x; my $x = 10; print $x })
        ){2}
       /x;
  print "$x ";
 }
CODE
 '1a82a93a104a85a96a101a 1b82b93b104b85b96b101b 1c82c93c104c85c96c101c ',
  {},
 'multiple (?{})s in loop with lexicals';

off;

fresh_perl_is <<'CODE', '781745', {}, 'run-time re-eval has its own scope';
 use re qw(eval);
 my $x = 7;  my $a = 4; my $b = 5;
 my $rest = 'a';
 print "a" =~ /(?{ print $x; my $x = 8; print $x; my $y })$rest/;
 print $x,$a,$b;
CODE

fresh_perl_is <<'CODE', '178279371047857967101745', {},
 use re "eval";
 my $x = 7; $y = 1;
 my $a = 4; my $b = 5;
 print scalar
  "abcabc"
    =~ ${\'(?x)
        (
         a (?{ print $y; local $y = $y+1; print $x; my $x = 8; print $x })
         b (?{ print $y; local $y = $y+1; print $x; my $x = 9; print $x })
         c (?{ print $y; local $y = $y+1; print $x; my $x = 10; print $x })
        ){2}
       '};
 print $x,$a,$b
CODE
 'multiple (?{})s in "foo" =~ $string';

fresh_perl_is <<'CODE', '178279371047857967101745', {},
 use re "eval";
 my $x = 7; $y = 1;
 my $a = 4; my $b = 5;
 print scalar
  "abcabc" =~
      /${\'
        (
         a (?{ print $y; local $y = $y+1; print $x; my $x = 8; print $x })
         b (?{ print $y; local $y = $y+1; print $x; my $x = 9; print $x })
         c (?{ print $y; local $y = $y+1; print $x; my $x = 10; print $x })
        ){2}
      '}/x;
 print $x,$a,$b
CODE
 'multiple (?{})s in "foo" =~ /$string/x';

on;

fresh_perl_is <<'CODE', '123123', {},
  for my $x(1..3) {
   push @regexps = qr/(?{ print $x })a/;
  }
 "a" =~ $_ for @regexps;
 "ba" =~ /b$_/ for @regexps;
CODE
 'qr/(?{})/ is a closure';

off;

"a" =~ do { package foo; qr/(?{ $::pack = __PACKAGE__ })a/ };
is $pack, 'foo', 'qr// inherits package';
"a" =~ do { use re "/x"; qr/(?{ $::re = qr-- })a/ };
is $re, '(?^x:)', 'qr// inherits pragmata';

on;

"ba" =~ /b${\do { package baz; qr|(?{ $::pack = __PACKAGE__ })a| }}/;
is $pack, 'baz', '/text$qr/ inherits package';
"ba" =~ m+b${\do { use re "/i"; qr|(?{ $::re = qr-- })a| }}+;
is $re, '(?^i:)', '/text$qr/ inherits pragmata';

off;
{
  use re 'eval';
  package bar;
  "ba" =~ /${\'(?{ $::pack = __PACKAGE__ })a'}/;
}
is $pack, 'bar', '/$text/ containing (?{}) inherits package';
{
  use re 'eval', "/m";
  "ba" =~ /${\'(?{ $::re = qr -- })a'}/;
}
is $re, '(?^m:)', '/$text/ containing (?{}) inherits pragmata';

on;

fresh_perl_is <<'CODE', '45', { stderr => 1 }, '(?{die})';
 eval { my $a=4; my $b=5; "a" =~ /(?{die})a/ }; print $a,$b"
CODE

SKIP: {
    # The remaining TODO tests crash, which will display an error dialog
    # on Windows that has to be manually dismissed.  We don't want this
    # to happen for release builds: 5.14.x, 5.16.x etc.
    # On UNIX, they produce ugly 'Aborted' shell output mixed in with the
    # test harness output, so skip on all platforms.
    skip "Don't run crashing TODO test on release build", 3
	if $::TODO && (int($]*1000) & 1) == 0;

    fresh_perl_is <<'CODE', '45', { stderr => 1 }, '(?{last})';
     {  my $a=4; my $b=5; "a" =~ /(?{last})a/ }; print $a,$b
CODE
    fresh_perl_is <<'CODE', '45', { stderr => 1 }, '(?{next})';
     {  my $a=4; my $b=5; "a" =~ /(?{last})a/ }; print $a,$b
CODE
    fresh_perl_is <<'CODE', '45', { stderr => 1 }, '(?{return})';
     print sub {  my $a=4; my $b=5; "a" =~ /(?{return $a.$b})a/ }->();
CODE
}

fresh_perl_is <<'CODE', '45', { stderr => 1 }, '(?{goto})';
  my $a=4; my $b=5; "a" =~ /(?{goto _})a/; die; _: print $a,$b
CODE

off;

# [perl #92256]
{ my $y = "a"; $y =~ /a(?{ undef *_ })/ }
pass "undef *_ in a re-eval does not cause a double free";
