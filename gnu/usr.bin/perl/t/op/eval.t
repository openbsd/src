#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

print "1..91\n";

eval 'print "ok 1\n";';

if ($@ eq '') {print "ok 2\n";} else {print "not ok 2\n";}

eval "\$foo\n    = # this is a comment\n'ok 3';";
print $foo,"\n";

eval "\$foo\n    = # this is a comment\n'ok 4\n';";
print $foo;

print eval '
$foo =;';		# this tests for a call through yyerror()
if ($@ =~ /line 2/) {print "ok 5\n";} else {print "not ok 5\n";}

print eval '$foo = /';	# this tests for a call through fatal()
if ($@ =~ /Search/) {print "ok 6\n";} else {print "not ok 6\n";}

print eval '"ok 7\n";';

# calculate a factorial with recursive evals

$foo = 5;
$fact = 'if ($foo <= 1) {1;} else {push(@x,$foo--); (eval $fact) * pop(@x);}';
$ans = eval $fact;
if ($ans == 120) {print "ok 8\n";} else {print "not ok 8\n";}

$foo = 5;
$fact = 'local($foo)=$foo; $foo <= 1 ? 1 : $foo-- * (eval $fact);';
$ans = eval $fact;
if ($ans == 120) {print "ok 9\n";} else {print "not ok 9 $ans\n";}

open(try,'>Op.eval');
print try 'print "ok 10\n"; unlink "Op.eval";',"\n";
close try;

do './Op.eval'; print $@;

# Test the singlequoted eval optimizer

$i = 11;
for (1..3) {
    eval 'print "ok ", $i++, "\n"';
}

eval {
    print "ok 14\n";
    die "ok 16\n";
    1;
} || print "ok 15\n$@";

# check whether eval EXPR determines value of EXPR correctly

{
  my @a = qw(a b c d);
  my @b = eval @a;
  print "@b" eq '4' ? "ok 17\n" : "not ok 17\n";
  print $@ ? "not ok 18\n" : "ok 18\n";

  my $a = q[defined(wantarray) ? (wantarray ? ($b='A') : ($b='S')) : ($b='V')];
  my $b;
  @a = eval $a;
  print "@a" eq 'A' ? "ok 19\n" : "# $b\nnot ok 19\n";
  print   $b eq 'A' ? "ok 20\n" : "# $b\nnot ok 20\n";
  $_ = eval $a;
  print   $b eq 'S' ? "ok 21\n" : "# $b\nnot ok 21\n";
  eval $a;
  print   $b eq 'V' ? "ok 22\n" : "# $b\nnot ok 22\n";

  $b = 'wrong';
  $x = sub {
     my $b = "right";
     print eval('"$b"') eq $b ? "ok 23\n" : "not ok 23\n";
  };
  &$x();
}

my $b = 'wrong';
my $X = sub {
   my $b = "right";
   print eval('"$b"') eq $b ? "ok 24\n" : "not ok 24\n";
};
&$X();


# check navigation of multiple eval boundaries to find lexicals

my $x = 25;
eval <<'EOT'; die if $@;
  print "# $x\n";	# clone into eval's pad
  sub do_eval1 {
     eval $_[0]; die if $@;
  }
EOT
do_eval1('print "ok $x\n"');
$x++;
do_eval1('eval q[print "ok $x\n"]');
$x++;
do_eval1('sub { print "# $x\n"; eval q[print "ok $x\n"] }->()');
$x++;

# calls from within eval'' should clone outer lexicals

eval <<'EOT'; die if $@;
  sub do_eval2 {
     eval $_[0]; die if $@;
  }
do_eval2('print "ok $x\n"');
$x++;
do_eval2('eval q[print "ok $x\n"]');
$x++;
do_eval2('sub { print "# $x\n"; eval q[print "ok $x\n"] }->()');
$x++;
EOT

# calls outside eval'' should NOT clone lexicals from called context

$main::ok = 'not ok';
my $ok = 'ok';
eval <<'EOT'; die if $@;
  # $x unbound here
  sub do_eval3 {
     eval $_[0]; die if $@;
  }
EOT
{
    my $ok = 'not ok';
    do_eval3('print "$ok ' . $x++ . '\n"');
    do_eval3('eval q[print "$ok ' . $x++ . '\n"]');
    do_eval3('sub { eval q[print "$ok ' . $x++ . '\n"] }->()');
}

# can recursive subroutine-call inside eval'' see its own lexicals?
sub recurse {
  my $l = shift;
  if ($l < $x) {
     ++$l;
     eval 'print "# level $l\n"; recurse($l);';
     die if $@;
  }
  else {
    print "ok $l\n";
  }
}
{
  local $SIG{__WARN__} = sub { die "not ok $x\n" if $_[0] =~ /^Deep recurs/ };
  recurse($x-5);
}
$x++;

# do closures created within eval bind correctly?
eval <<'EOT';
  sub create_closure {
    my $self = shift;
    return sub {
       print $self;
    };
  }
EOT
create_closure("ok $x\n")->();
$x++;

# does lexical search terminate correctly at subroutine boundary?
$main::r = "ok $x\n";
sub terminal { eval 'print $r' }
{
   my $r = "not ok $x\n";
   eval 'terminal($r)';
}
$x++;

# Have we cured panic which occurred with require/eval in die handler ?
$SIG{__DIE__} = sub { eval {1}; die shift }; 
eval { die "ok ".$x++,"\n" }; 
print $@;

# does scalar eval"" pop stack correctly?
{
    my $c = eval "(1,2)x10";
    print $c eq '2222222222' ? "ok $x\n" : "# $c\nnot ok $x\n";
    $x++;
}

# return from eval {} should clear $@ correctly
{
    my $status = eval {
	eval { die };
	print "# eval { return } test\n";
	return; # removing this changes behavior
    };
    print "not " if $@;
    print "ok $x\n";
    $x++;
}

# ditto for eval ""
{
    my $status = eval q{
	eval q{ die };
	print "# eval q{ return } test\n";
	return; # removing this changes behavior
    };
    print "not " if $@;
    print "ok $x\n";
    $x++;
}

# Check that eval catches bad goto calls
#   (BUG ID 20010305.003)
{
    eval {
	eval { goto foo; };
	print ($@ ? "ok 41\n" : "not ok 41\n");
	last;
	foreach my $i (1) {
	    foo: print "not ok 41\n";
	    print "# jumped into foreach\n";
	}
    };
    print "not ok 41\n" if $@;
}

# Make sure that "my $$x" is forbidden
# 20011224 MJD
{
  eval q{my $$x};
  print $@ ? "ok 42\n" : "not ok 42\n";
  eval q{my @$x};
  print $@ ? "ok 43\n" : "not ok 43\n";
  eval q{my %$x};
  print $@ ? "ok 44\n" : "not ok 44\n";
  eval q{my $$$x};
  print $@ ? "ok 45\n" : "not ok 45\n";
}

# [ID 20020623.002] eval "" doesn't clear $@
{
    $@ = 5;
    eval q{};
    print length($@) ? "not ok 46\t# \$\@ = '$@'\n" : "ok 46\n";
}

# DAPM Nov-2002. Perl should now capture the full lexical context during
# evals.

$::zzz = $::zzz = 0;
my $zzz = 1;

eval q{
    sub fred1 {
	eval q{ print eval '$zzz' == 1 ? 'ok' : 'not ok', " $_[0]\n"}
    }
    fred1(47);
    { my $zzz = 2; fred1(48) }
};

eval q{
    sub fred2 {
	print eval('$zzz') == 1 ? 'ok' : 'not ok', " $_[0]\n";
    }
};
fred2(49);
{ my $zzz = 2; fred2(50) }

# sort() starts a new context stack. Make sure we can still find
# the lexically enclosing sub

sub do_sort {
    my $zzz = 2;
    my @a = sort
	    { print eval('$zzz') == 2 ? 'ok' : 'not ok', " 51\n"; $a <=> $b }
	    2, 1;
}
do_sort();

# more recursion and lexical scope leak tests

eval q{
    my $r = -1;
    my $yyy = 9;
    sub fred3 {
	my $l = shift;
	my $r = -2;
	return 1 if $l < 1;
	return 0 if eval '$zzz' != 1;
	return 0 if       $yyy  != 9;
	return 0 if eval '$yyy' != 9;
	return 0 if eval '$l' != $l;
	return $l * fred3($l-1);
    }
    my $r = fred3(5);
    print $r == 120 ? 'ok' : 'not ok', " 52\n";
    $r = eval'fred3(5)';
    print $r == 120 ? 'ok' : 'not ok', " 53\n";
    $r = 0;
    eval '$r = fred3(5)';
    print $r == 120 ? 'ok' : 'not ok', " 54\n";
    $r = 0;
    { my $yyy = 4; my $zzz = 5; my $l = 6; $r = eval 'fred3(5)' };
    print $r == 120 ? 'ok' : 'not ok', " 55\n";
};
my $r = fred3(5);
print $r == 120 ? 'ok' : 'not ok', " 56\n";
$r = eval'fred3(5)';
print $r == 120 ? 'ok' : 'not ok', " 57\n";
$r = 0;
eval'$r = fred3(5)';
print $r == 120 ? 'ok' : 'not ok', " 58\n";
$r = 0;
{ my $yyy = 4; my $zzz = 5; my $l = 6; $r = eval 'fred3(5)' };
print $r == 120 ? 'ok' : 'not ok', " 59\n";

# check that goto &sub within evals doesn't leak lexical scope

my $yyy = 2;

my $test = 60;
sub fred4 { 
    my $zzz = 3;
    print +($zzz == 3  && eval '$zzz' == 3) ? 'ok' : 'not ok', " $test\n";
    $test++;
    print eval '$yyy' == 2 ? 'ok' : 'not ok', " $test\n";
    $test++;
}

eval q{
    fred4();
    sub fred5 {
	my $zzz = 4;
	print +($zzz == 4  && eval '$zzz' == 4) ? 'ok' : 'not ok', " $test\n";
	$test++;
	print eval '$yyy' == 2 ? 'ok' : 'not ok', " $test\n";
	$test++;
	goto &fred4;
    }
    fred5();
};
fred5();
{ my $yyy = 88; my $zzz = 99; fred5(); }
eval q{ my $yyy = 888; my $zzz = 999; fred5(); };

# [perl #9728] used to dump core
{
   $eval = eval 'sub { eval "sub { %S }" }';
   $eval->({});
   print "ok $test\n";
   $test++;
}

# evals that appear in the DB package should see the lexical scope of the
# thing outside DB that called them (usually the debugged code), rather
# than the usual surrounding scope

$test=79;
our $x = 1;
{
    my $x=2;
    sub db1	{ $x; eval '$x' }
    sub DB::db2	{ $x; eval '$x' }
    package DB;
    sub db3	{ eval '$x' }
    sub DB::db4	{ eval '$x' }
    sub db5	{ my $x=4; eval '$x' }
    package main;
    sub db6	{ my $x=4; eval '$x' }
}
{
    my $x = 3;
    print db1()     == 2 ? 'ok' : 'not ok', " $test\n"; $test++;
    print DB::db2() == 2 ? 'ok' : 'not ok', " $test\n"; $test++;
    print DB::db3() == 3 ? 'ok' : 'not ok', " $test\n"; $test++;
    print DB::db4() == 3 ? 'ok' : 'not ok', " $test\n"; $test++;
    print DB::db5() == 3 ? 'ok' : 'not ok', " $test\n"; $test++;
    print db6()     == 4 ? 'ok' : 'not ok', " $test\n"; $test++;
}
require './test.pl';
$NO_ENDING = 1;
# [perl #19022] used to end up with shared hash warnings
# The program should generate no output, so anything we see is on stderr
my $got = runperl (prog => '$h{a}=1; foreach my $k (keys %h) {eval qq{\$k}}',
		   stderr => 1);

if ($got eq '') {
  print "ok $test\n";
} else {
  print "not ok $test\n";
  _diag ("# Got '$got'\n");
}
$test++;

# And a buggy way of fixing #19022 made this fail - $k became undef after the
# eval for a build with copy on write
{
  my %h;
  $h{a}=1;
  foreach my $k (keys %h) {
    if (defined $k and $k eq 'a') {
      print "ok $test\n";
    } else {
      print "not $test # got ", _q ($k), "\n";
    }
    $test++;

    eval "\$k";

    if (defined $k and $k eq 'a') {
      print "ok $test\n";
    } else {
      print "not $test # got ", _q ($k), "\n";
    }
    $test++;
  }
}

sub Foo {} print Foo(eval {});
print "ok ",$test++," - #20798 (used to dump core)\n";

# check for context in string eval
{
  my(@r,$r,$c);
  sub context { defined(wantarray) ? (wantarray ? ($c='A') : ($c='S')) : ($c='V') }

  my $code = q{ context() };
  @r = qw( a b );
  $r = 'ab';
  @r = eval $code;
  print "@r$c" eq 'AA' ? "ok " : "# '@r$c' ne 'AA'\nnot ok ", $test++, "\n";
  $r = eval $code;
  print "$r$c" eq 'SS' ? "ok " : "# '$r$c' ne 'SS'\nnot ok ", $test++, "\n";
  eval $code;
  print   $c   eq 'V'  ? "ok " : "# '$c' ne 'V'\nnot ok ", $test++, "\n";
}
