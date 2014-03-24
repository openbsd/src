#!./perl -w

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

plan( tests => 16 );

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
