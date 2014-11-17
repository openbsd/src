#!perl

BEGIN {
    chdir 't';
    @INC = '../lib';
    require './test.pl';
    *bar::is = *is;
    *bar::like = *like;
}
plan 120;

# -------------------- Errors with feature disabled -------------------- #

eval "#line 8 foo\nmy sub foo";
is $@, qq 'Experimental "my" subs not enabled at foo line 8.\n',
  'my sub unexperimental error';
eval "#line 8 foo\nCORE::state sub foo";
is $@, qq 'Experimental "state" subs not enabled at foo line 8.\n',
  'state sub unexperimental error';
eval "#line 8 foo\nour sub foo";
is $@, qq 'Experimental "our" subs not enabled at foo line 8.\n',
  'our sub unexperimental error';

# -------------------- our -------------------- #

no warnings "experimental::lexical_subs";
use feature 'lexical_subs';
{
  our sub foo { 42 }
  is foo, 42, 'calling our sub from same package';
  is &foo, 42, 'calling our sub from same package (amper)';
  package bar;
  sub bar::foo { 43 }
  is foo, 42, 'calling our sub from another package';
  is &foo, 42, 'calling our sub from another package (amper)';
}
package bar;
is foo, 43, 'our sub falling out of scope';
is &foo, 43, 'our sub falling out of scope (called via amper)';
package main;
{
  sub bar::a { 43 }
  our sub a {
    if (shift) {
      package bar;
      is a, 43, 'our sub invisible inside itself';
      is &a, 43, 'our sub invisible inside itself (called via amper)';
    }
    42
  }
  a(1);
  sub bar::b { 43 }
  our sub b;
  our sub b {
    if (shift) {
      package bar;
      is b, 42, 'our sub visible inside itself after decl';
      is &b, 42, 'our sub visible inside itself after decl (amper)';
    }
    42
  }
  b(1)
}
sub c { 42 }
sub bar::c { 43 }
{
  our sub c;
  package bar;
  is c, 42, 'our sub foo; makes lex alias for existing sub';
  is &c, 42, 'our sub foo; makes lex alias for existing sub (amper)';
}
{
  our sub d;
  sub bar::d { 'd43' }
  package bar;
  sub d { 'd42' }
  is eval ::d, 'd42', 'our sub foo; applies to subsequent sub foo {}';
}
{
  our sub e ($);
  is prototype "::e", '$', 'our sub with proto';
}
{
  our sub if() { 42 }
  my $x = if if if;
  is $x, 42, 'lexical subs (even our) override all keywords';
  package bar;
  my $y = if if if;
  is $y, 42, 'our subs from other packages override all keywords';
}

# -------------------- state -------------------- #

use feature 'state'; # state
{
  state sub foo { 44 }
  isnt \&::foo, \&foo, 'state sub is not stored in the package';
  is eval foo, 44, 'calling state sub from same package';
  is eval &foo, 44, 'calling state sub from same package (amper)';
  package bar;
  is eval foo, 44, 'calling state sub from another package';
  is eval &foo, 44, 'calling state sub from another package (amper)';
}
package bar;
is foo, 43, 'state sub falling out of scope';
is &foo, 43, 'state sub falling out of scope (called via amper)';
{
  sub sa { 43 }
  state sub sa {
    if (shift) {
      is sa, 43, 'state sub invisible inside itself';
      is &sa, 43, 'state sub invisible inside itself (called via amper)';
    }
    44
  }
  sa(1);
  sub sb { 43 }
  state sub sb;
  state sub sb {
    if (shift) {
      # ‘state sub foo{}’ creates a new pad entry, not reusing the forward
      #  declaration.  Being invisible inside itself, it sees the stub.
      eval{sb};
      like $@, qr/^Undefined subroutine &sb called at /,
        'state sub foo {} after forward declaration';
      eval{&sb};
      like $@, qr/^Undefined subroutine &sb called at /,
        'state sub foo {} after forward declaration (amper)';
    }
    44
  }
  sb(1);
  sub sb2 { 43 }
  state sub sb2;
  sub sb2 {
    if (shift) {
      package bar;
      is sb2, 44, 'state sub visible inside itself after decl';
      is &sb2, 44, 'state sub visible inside itself after decl (amper)';
    }
    44
  }
  sb2(1);
  state sub sb3;
  {
    state sub sb3 { # new pad entry
      # The sub containing this comment is invisible inside itself.
      # So this one here will assign to the outer pad entry:
      sub sb3 { 47 }
    }
  }
  is eval{sb3}, 47,
    'sub foo{} applying to "state sub foo;" even inside state sub foo{}';
  # Same test again, but inside an anonymous sub
  sub {
    state sub sb4;
    {
      state sub sb4 {
        sub sb4 { 47 }
      }
    }
    is sb4, 47,
      'sub foo{} applying to "state sub foo;" even inside state sub foo{}';
  }->();
}
sub sc { 43 }
{
  state sub sc;
  eval{sc};
  like $@, qr/^Undefined subroutine &sc called at /,
     'state sub foo; makes no lex alias for existing sub';
  eval{&sc};
  like $@, qr/^Undefined subroutine &sc called at /,
     'state sub foo; makes no lex alias for existing sub (amper)';
}
package main;
{
  state sub se ($);
  is prototype eval{\&se}, '$', 'state sub with proto';
  is prototype "se", undef, 'prototype "..." ignores state subs';
}
{
  state sub if() { 44 }
  my $x = if if if;
  is $x, 44, 'state subs override all keywords';
  package bar;
  my $y = if if if;
  is $y, 44, 'state subs from other packages override all keywords';
}
{
  use warnings; no warnings "experimental::lexical_subs";
  state $w ;
  local $SIG{__WARN__} = sub { $w .= shift };
  eval '#line 87 squidges
    state sub foo;
    state sub foo {};
  ';
  is $w,
     '"state" subroutine &foo masks earlier declaration in same scope at '
   . "squidges line 88.\n",
     'warning for state sub masking earlier declaration';
}
# Since state vars inside anonymous subs are cloned at the same time as the
# anonymous subs containing them, the same should happen for state subs.
sub make_closure {
  my $x = shift;
  sub {
    state sub foo { $x }
    foo
  }
}
$sub1 = make_closure 48;
$sub2 = make_closure 49;
is &$sub1, 48, 'state sub in closure (1)';
is &$sub2, 49, 'state sub in closure (2)';
# But we need to test that state subs actually do persist from one invoca-
# tion of a named sub to another (i.e., that they are not my subs).
{
  use warnings; no warnings "experimental::lexical_subs";
  state $w;
  local $SIG{__WARN__} = sub { $w .= shift };
  eval '#line 65 teetet
    sub foom {
      my $x = shift;
      state sub poom { $x }
      eval{\&poom}
    }
  ';
  is $w, "Variable \"\$x\" will not stay shared at teetet line 67.\n",
         'state subs get "Variable will not stay shared" messages';
  my $poom = foom(27);
  my $poom2 = foom(678);
  is eval{$poom->()}, eval {$poom2->()},
    'state subs close over the first outer my var, like pkg subs';
  my $x = 43;
  for $x (765) {
    state sub etetetet { $x }
    is eval{etetetet}, 43, 'state sub ignores for() localisation';
  }
}
# And we also need to test that multiple state subs can close over each
# other’s entries in the parent subs pad, and that cv_clone is not con-
# fused by that.
sub make_anon_with_state_sub{
  sub {
    state sub s1;
    state sub s2 { \&s1 }
    sub s1 { \&s2 }
    if (@_) { return \&s1 }
    is s1,\&s2, 'state sub in anon closure closing over sibling state sub';
    is s2,\&s1, 'state sub in anon closure closing over sibling state sub';
  }
}
{
  my $s = make_anon_with_state_sub;
  &$s;

  # And make sure the state subs were actually cloned.
  isnt make_anon_with_state_sub->(0), &$s(0),
    'state subs in anon subs are cloned';
  is &$s(0), &$s(0), 'but only when the anon sub is cloned';
}
{
  state sub BEGIN { exit };
  pass 'state subs are never special blocks';
  state sub END { shift }
  is eval{END('jkqeudth')}, jkqeudth,
    'state sub END {shift} implies @_, not @ARGV';
  state sub CORE { scalar reverse shift }
  is CORE::uc("hello"), "HELLO",
    'lexical CORE does not interfere with CORE::...';
}
{
  state sub redef {}
  use warnings; no warnings "experimental::lexical_subs";
  state $w;
  local $SIG{__WARN__} = sub { $w .= shift };
  eval "#line 56 pygpyf\nsub redef {}";
  is $w, "Subroutine redef redefined at pygpyf line 56.\n",
         "sub redefinition warnings from state subs";
}
{
  state sub p (\@) {
    is ref $_[0], 'ARRAY', 'state sub with proto';
  }
  p(my @a);
  p my @b;
  state sub q () { 45 }
  is q(), 45, 'state constant called with parens';
}
{
  state sub x;
  eval 'sub x {3}';
  is x, 3, 'state sub defined inside eval';

  sub r {
    state sub foo { 3 };
    if (@_) { # outer call
      r();
      is foo(), 42,
         'state sub run-time redefinition applies to all recursion levels';
    }
    else { # inner call
      eval 'sub foo { 42 }';
    }
  }
  r(1);
}
like runperl(
      switches => [ '-Mfeature=lexical_subs,state' ],
      prog     => 'state sub a { foo ref } a()',
      stderr   => 1
     ),
     qr/syntax error/,
    'referencing a state sub after a syntax error does not crash';

# -------------------- my -------------------- #

{
  my sub foo { 44 }
  isnt \&::foo, \&foo, 'my sub is not stored in the package';
  is foo, 44, 'calling my sub from same package';
  is &foo, 44, 'calling my sub from same package (amper)';
  package bar;
  is foo, 44, 'calling my sub from another package';
  is &foo, 44, 'calling my sub from another package (amper)';
}
package bar;
is foo, 43, 'my sub falling out of scope';
is &foo, 43, 'my sub falling out of scope (called via amper)';
{
  sub ma { 43 }
  my sub ma {
    if (shift) {
      is ma, 43, 'my sub invisible inside itself';
      is &ma, 43, 'my sub invisible inside itself (called via amper)';
    }
    44
  }
  ma(1);
  sub mb { 43 }
  my sub mb;
  my sub mb {
    if (shift) {
      # ‘my sub foo{}’ creates a new pad entry, not reusing the forward
      #  declaration.  Being invisible inside itself, it sees the stub.
      eval{mb};
      like $@, qr/^Undefined subroutine &mb called at /,
        'my sub foo {} after forward declaration';
      eval{&mb};
      like $@, qr/^Undefined subroutine &mb called at /,
        'my sub foo {} after forward declaration (amper)';
    }
    44
  }
  mb(1);
  sub mb2 { 43 }
  my sub sb2;
  sub mb2 {
    if (shift) {
      package bar;
      is mb2, 44, 'my sub visible inside itself after decl';
      is &mb2, 44, 'my sub visible inside itself after decl (amper)';
    }
    44
  }
  mb2(1);
  my sub mb3;
  {
    my sub mb3 { # new pad entry
      # The sub containing this comment is invisible inside itself.
      # So this one here will assign to the outer pad entry:
      sub mb3 { 47 }
    }
  }
  is eval{mb3}, 47,
    'sub foo{} applying to "my sub foo;" even inside my sub foo{}';
  # Same test again, but inside an anonymous sub
  sub {
    my sub mb4;
    {
      my sub mb4 {
        sub mb4 { 47 }
      }
    }
    is mb4, 47,
      'sub foo{} applying to "my sub foo;" even inside my sub foo{}';
  }->();
}
sub mc { 43 }
{
  my sub mc;
  eval{mc};
  like $@, qr/^Undefined subroutine &mc called at /,
     'my sub foo; makes no lex alias for existing sub';
  eval{&mc};
  like $@, qr/^Undefined subroutine &mc called at /,
     'my sub foo; makes no lex alias for existing sub (amper)';
}
package main;
{
  my sub me ($);
  is prototype eval{\&me}, '$', 'my sub with proto';
  is prototype "me", undef, 'prototype "..." ignores my subs';

  my $coderef = eval "my sub foo (\$\x{30cd}) {1}; \\&foo";
  my $proto = prototype $coderef;
  ok(utf8::is_utf8($proto), "my sub with UTF8 proto maintains the UTF8ness");
  is($proto, "\$\x{30cd}", "check the prototypes actually match");
}
{
  my sub if() { 44 }
  my $x = if if if;
  is $x, 44, 'my subs override all keywords';
  package bar;
  my $y = if if if;
  is $y, 44, 'my subs from other packages override all keywords';
}
{
  use warnings; no warnings "experimental::lexical_subs";
  my $w ;
  local $SIG{__WARN__} = sub { $w .= shift };
  eval '#line 87 squidges
    my sub foo;
    my sub foo {};
  ';
  is $w,
     '"my" subroutine &foo masks earlier declaration in same scope at '
   . "squidges line 88.\n",
     'warning for my sub masking earlier declaration';
}
# Test that my subs are cloned inside anonymous subs.
sub mmake_closure {
  my $x = shift;
  sub {
    my sub foo { $x }
    foo
  }
}
$sub1 = mmake_closure 48;
$sub2 = mmake_closure 49;
is &$sub1, 48, 'my sub in closure (1)';
is &$sub2, 49, 'my sub in closure (2)';
# Test that they are cloned in named subs.
{
  use warnings; no warnings "experimental::lexical_subs";
  my $w;
  local $SIG{__WARN__} = sub { $w .= shift };
  eval '#line 65 teetet
    sub mfoom {
      my $x = shift;
      my sub poom { $x }
      \&poom
    }
  ';
  is $w, undef, 'my subs get no "Variable will not stay shared" messages';
  my $poom = mfoom(27);
  my $poom2 = mfoom(678);
  is $poom->(), 27, 'my subs closing over outer my var (1)';
  is $poom2->(), 678, 'my subs closing over outer my var (2)';
  my $x = 43;
  my sub aoeu;
  for $x (765) {
    my sub etetetet { $x }
    sub aoeu { $x }
    is etetetet, 765, 'my sub respects for() localisation';
    is aoeu, 43, 'unless it is declared outside the for loop';
  }
}
# And we also need to test that multiple my subs can close over each
# other’s entries in the parent subs pad, and that cv_clone is not con-
# fused by that.
sub make_anon_with_my_sub{
  sub {
    my sub s1;
    my sub s2 { \&s1 }
    sub s1 { \&s2 }
    if (@_) { return eval { \&s1 } }
    is eval{s1},eval{\&s2}, 'my sub in anon closure closing over sibling my sub';
    is eval{s2},eval{\&s1}, 'my sub in anon closure closing over sibling my sub';
  }
}

# Test my subs inside predeclared my subs
{
  my sub s2;
  sub s2 {
    my $x = 3;
    my sub s3 { eval '$x' }
    s3;
  }
  is s2, 3, 'my sub inside predeclared my sub';
}

{
  my $s = make_anon_with_my_sub;
  &$s;

  # And make sure the my subs were actually cloned.
  isnt make_anon_with_my_sub->(0), &$s(0),
    'my subs in anon subs are cloned';
  isnt &$s(0), &$s(0), 'at each invocation of the enclosing sub';
}
{
  my sub BEGIN { exit };
  pass 'my subs are never special blocks';
  my sub END { shift }
  is END('jkqeudth'), jkqeudth,
    'my sub END {shift} implies @_, not @ARGV';
}
{
  my sub redef {}
  use warnings; no warnings "experimental::lexical_subs";
  my $w;
  local $SIG{__WARN__} = sub { $w .= shift };
  eval "#line 56 pygpyf\nsub redef {}";
  is $w, "Subroutine redef redefined at pygpyf line 56.\n",
         "sub redefinition warnings from my subs";

  undef $w;
  sub {
    my sub x {};
    sub { eval "#line 87 khaki\n\\&x" }
  }->()();
  is $w, "Subroutine \"&x\" is not available at khaki line 87.\n",
         "unavailability warning during compilation of eval in closure";

  undef $w;
  no warnings 'void';
  eval <<'->()();';
#line 87 khaki
    sub {
      my sub x{}
      sub not_lexical8 {
        \&x
      }
    }
->()();
  is $w, "Subroutine \"&x\" is not available at khaki line 90.\n",
         "unavailability warning during compilation of named sub in anon";

  undef $w;
  sub not_lexical9 {
    my sub x {};
    format =
@
&x
.
  }
  eval { write };
  my($f,$l) = (__FILE__,__LINE__ - 1);
  is $w, "Subroutine \"&x\" is not available at $f line $l.\n",
         'unavailability warning during cloning';
  $l -= 3;
  is $@, "Undefined subroutine &x called at $f line $l.\n",
         'Vivified sub is correctly named';
}
sub not_lexical10 {
  my sub foo;
  foo();
  sub not_lexical11 {
    my sub bar {
      my $x = 'khaki car keys for the khaki car';
      not_lexical10();
      sub foo {
       is $x, 'khaki car keys for the khaki car',
       'mysubs in inner clonables use the running clone of their CvOUTSIDE'
      }
    }
    bar()
  }
}
not_lexical11();
{
  my sub p (\@) {
    is ref $_[0], 'ARRAY', 'my sub with proto';
  }
  p(my @a);
  p @a;
  my sub q () { 46 }
  is q(), 46, 'my constant called with parens';
}
{
  my sub x;
  my $count;
  sub x { x() if $count++ < 10 }
  x();
  is $count, 11, 'my recursive subs';
}
{
  my sub x;
  eval 'sub x {3}';
  is x, 3, 'my sub defined inside eval';
}

{
  state $w;
  local $SIG{__WARN__} = sub { $w .= shift };
  eval q{ my sub george () { 2 } };
  is $w, undef, 'no double free from constant my subs';
}
like runperl(
      switches => [ '-Mfeature=lexical_subs,state' ],
      prog     => 'my sub a { foo ref } a()',
      stderr   => 1
     ),
     qr/syntax error/,
    'referencing a my sub after a syntax error does not crash';

# -------------------- Interactions (and misc tests) -------------------- #

is sub {
    my sub s1;
    my sub s2 { 3 };
    sub s1 { state sub foo { \&s2 } foo }
    s1
  }->()(), 3, 'state sub inside my sub closing over my sub uncle';

{
  my sub s2 { 3 };
  sub not_lexical { state sub foo { \&s2 } foo }
  is not_lexical->(), 3, 'state subs that reference my sub from outside';
}

# Test my subs inside predeclared package subs
# This test also checks that CvOUTSIDE pointers are not mangled when the
# inner sub’s CvOUTSIDE points to another sub.
sub not_lexical2;
sub not_lexical2 {
  my $x = 23;
  my sub bar;
  sub not_lexical3 {
    not_lexical2();
    sub bar { $x }
  };
  bar
}
is not_lexical3, 23, 'my subs inside predeclared package subs';

# Test my subs inside predeclared package sub, where the lexical sub is
# declared outside the package sub.
# This checks that CvOUTSIDE pointers are fixed up even when the sub is
# not declared inside the sub that its CvOUTSIDE points to.
sub not_lexical5 {
  my sub foo;
  sub not_lexical4;
  sub not_lexical4 {
    my $x = 234;
    not_lexical5();
    sub foo { $x }
  }
  foo
}
is not_lexical4, 234,
    'my sub defined in predeclared pkg sub but declared outside';

undef *not_lexical6;
{
  my sub foo;
  sub not_lexical6 { sub foo { } }
  pass 'no crash when cloning a mysub declared inside an undef pack sub';
}

undef &not_lexical7;
eval 'sub not_lexical7 { my @x }';
{
  my sub foo;
  foo();
  sub not_lexical7 {
    state $x;
    sub foo {
      is ref \$x, 'SCALAR',
        "redeffing a mysub's outside does not make it use the wrong pad"
    }
  }
}

like runperl(
      switches => [ '-Mfeature=lexical_subs,state', '-Mwarnings=FATAL,all', '-M-warnings=experimental::lexical_subs' ],
      prog     => 'my sub foo; sub foo { foo } foo',
      stderr   => 1
     ),
     qr/Deep recursion on subroutine "foo"/,
    'deep recursion warnings for lexical subs do not crash';

like runperl(
      switches => [ '-Mfeature=lexical_subs,state', '-Mwarnings=FATAL,all', '-M-warnings=experimental::lexical_subs' ],
      prog     => 'my sub foo() { 42 } undef &foo',
      stderr   => 1
     ),
     qr/Constant subroutine foo undefined at /,
    'constant undefinition warnings for lexical subs do not crash';

{
  my sub foo;
  *AutoloadTestSuper::blah = \&foo;
  sub AutoloadTestSuper::AUTOLOAD {
    is $AutoloadTestSuper::AUTOLOAD, "AutoloadTestSuper::blah",
      "Autoloading via inherited lex stub";
  }
  @AutoloadTest::ISA = AutoloadTestSuper::;
  AutoloadTest->blah;
}
