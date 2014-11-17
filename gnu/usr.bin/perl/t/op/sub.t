#!./perl -w

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

plan( tests => 33 );

sub empty_sub {}

is(empty_sub,undef,"Is empty");
is(empty_sub(1,2,3),undef,"Is still empty");
@test = empty_sub();
is(scalar(@test), 0, 'Didnt return anything');
@test = empty_sub(1,2,3);
is(scalar(@test), 0, 'Didnt return anything');

# RT #63790:  calling PL_sv_yes as a sub is special-cased to silently
# return (so Foo->import() silently fails if import() doesn't exist),
# But make sure it correctly pops the stack and mark stack before returning.

{
    my @a;
    push @a, 4, 5, main->import(6,7);
    ok(eq_array(\@a, [4,5]), "import with args");

    @a = ();
    push @a, 14, 15, main->import;
    ok(eq_array(\@a, [14,15]), "import without args");

    my $x = 1;

    @a = ();
    push @a, 24, 25, &{$x == $x}(26,27);
    ok(eq_array(\@a, [24,25]), "yes with args");

    @a = ();
    push @a, 34, 35, &{$x == $x};
    ok(eq_array(\@a, [34,35]), "yes without args");
}

# [perl #81944] return should always copy
{
    $foo{bar} = 7;
    for my $x ($foo{bar}) {
	# Pity test.pl doesnt have isn't.
	isnt \sub { delete $foo{bar} }->(), \$x,
	   'result of delete(helem) is copied when returned';
    }
    $foo{bar} = 7;
    for my $x ($foo{bar}) {
	isnt \sub { return delete $foo{bar} }->(), \$x,
	   'result of delete(helem) is copied when explicitly returned';
    }
    my $x;
    isnt \sub { delete $_[0] }->($x), \$x,
      'result of delete(aelem) is copied when returned';
    isnt \sub { return delete $_[0] }->($x), \$x,
      'result of delete(aelem) is copied when explicitly returned';
    isnt \sub { ()=\@_; shift }->($x), \$x,
      'result of shift is copied when returned';
    isnt \sub { ()=\@_; return shift }->($x), \$x,
      'result of shift is copied when explicitly returned';
}

fresh_perl_is
  <<'end', "main::foo\n", {}, 'sub redefinition sets CvGV';
*foo = \&baz;
*bar = *foo;
eval 'sub bar { print +(caller 0)[3], "\n" }';
bar();
end

fresh_perl_is
  <<'end', "main::foo\nok\n", {}, 'no double free redefining anon stub';
my $sub = sub { 4 };
*foo = $sub;
*bar = *foo;
undef &$sub;
eval 'sub bar { print +(caller 0)[3], "\n" }';
&$sub;
undef *foo;
undef *bar;
print "ok\n";
end

# The outer call sets the scalar returned by ${\""}.${\""} to the current
# package name.
# The inner call sets it to "road".
# Each call records the value twice, the outer call surrounding the inner
# call.  In 5.10-5.18 under ithreads, what gets pushed is
# qw(main road road road) because the inner call is clobbering the same
# scalar.  If __PACKAGE__ is changed to "main", it works, the last element
# becoming "main".
my @scratch;
sub a {
  for (${\""}.${\""}) {
    $_ = $_[0];
    push @scratch, $_;
    a("road",1) unless $_[1];
    push @scratch, $_;
  }
}
a(__PACKAGE__);
require Config;
is "@scratch", "main road road main",
   'recursive calls do not share shared-hash-key TARGs';

# Another test for the same bug, that does not rely on foreach.  It depends
# on ref returning a shared hash key TARG.
undef @scratch;
sub b {
    my ($pack, $depth) = @_;
    my $o = bless[], $pack;
    $pack++;
    push @scratch, (ref $o, $depth||b($pack,$depth+1))[0];
}
b('n',0);
is "@scratch", "o n", 
   'recursive calls do not share shared-hash-key TARGs (2)';

# [perl #78194] @_ aliasing op return values
sub { is \$_[0], \$_[0],
        '[perl #78194] \$_[0] == \$_[0] when @_ aliases "$x"' }
 ->("${\''}");

# The return statement should make no difference in this case:
sub not_constant () {        42 }
sub not_constantr() { return 42 }
use feature 'lexical_subs'; no warnings 'experimental::lexical_subs';
my sub not_constantm () {        42 }
my sub not_constantmr() { return 42 }
eval { ${\not_constant}++ };
is $@, "", 'sub (){42} returns a mutable value';
eval { ${\not_constantr}++ };
is $@, "", 'sub (){ return 42 } returns a mutable value';
eval { ${\not_constantm}++ };
is $@, "", 'my sub (){42} returns a mutable value';
eval { ${\not_constantmr}++ };
is $@, "", 'my sub (){ return 42 } returns a mutable value';
is eval {
    sub Crunchy () { 1 }
    sub Munchy { $_[0] = 2 }
    eval "Crunchy"; # test that freeing this op does not turn off PADTMP
    Munchy(Crunchy);
} || $@, 2, 'freeing ops does not make sub(){42} immutable';

# [perl #79908]
{
    my $x = 5;
    *_79908 = sub (){$x};
    $x = 7;
    TODO: {
        local $TODO = "Should be fixed with a deprecation cycle, see 'How about having a recommended way to add constant subs dynamically?' on p5p";
        is eval "_79908", 7, 'sub(){$x} does not break closures';
    }
    isnt eval '\_79908', \$x, 'sub(){$x} returns a copy';

    # Test another thing that was broken by $x inlinement
    my $y;
    no warnings 'once';
    local *time = sub():method{$y};
    my $w;
    local $SIG{__WARN__} = sub { $w .= shift };
    eval "()=time";
    TODO: {
        local $TODO = "Should be fixed with a deprecation cycle, see 'How about having a recommended way to add constant subs dynamically?' on p5p";
        is $w, undef,
          '*keyword = sub():method{$y} does not cause ambiguity warnings';
    }
}

# &xsub when @_ has nonexistent elements
{
    no warnings "uninitialized";
    local @_ = ();
    $#_++;
    &utf8::encode;
    is @_, 1, 'num of elems in @_ after &xsub with nonexistent $_[0]';
    is $_[0], "", 'content of nonexistent $_[0] is modified by &xsub';
}

# &xsub when @_ itself does not exist
undef *_;
eval { &utf8::encode };
# The main thing we are testing is that it did not crash.  But make sure 
# *_{ARRAY} was untouched, too.
is *_{ARRAY}, undef, 'goto &xsub when @_ does not exist';

# We do not want re.pm loaded at this point.  Move this test up or find
# another XSUB if this fails.
ok !exists $INC{"re.pm"}, 're.pm not loaded yet';
{
    sub re::regmust{}
    bless \&re::regmust;
    DESTROY {
        no warnings 'redefine', 'prototype';
        my $str1 = "$_[0]";
        *re::regmust = sub{}; # GvSV had no refcount, so this freed it
        my $str2 = "$_[0]";   # used to be UNKNOWN(0x7fdda29310e0)
        @str = ($str1, $str2);
    }
    local $^W; # Suppress redef warnings in XSLoader
    require re;
    is $str[1], $str[0],
      'XSUB clobbering sub whose DESTROY assigns to the glob';
}
{
    no warnings 'redefine';
    sub foo {}
    bless \&foo, 'newATTRSUBbug';
    sub newATTRSUBbug::DESTROY {
        my $str1 = "$_[0]";
        *foo = sub{}; # GvSV had no refcount, so this freed it
        my $str2 = "$_[0]";   # used to be UNKNOWN(0x7fdda29310e0)
        @str = ($str1, $str2);
    }
    splice @str;
    eval "sub foo{}";
    is $str[1], $str[0],
      'Pure-Perl sub clobbering sub whose DESTROY assigns to the glob';
}
