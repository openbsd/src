#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}
use warnings;
print "1..75\n";

# these shouldn't hang
{
    no warnings;
    sort { for ($_ = 0;; $_++) {} } @a;
    sort { while(1) {}            } @a;
    sort { while(1) { last; }     } @a;
    sort { while(0) { last; }     } @a;
}

sub Backwards { $a lt $b ? 1 : $a gt $b ? -1 : 0 }
sub Backwards_stacked($$) { my($a,$b) = @_; $a lt $b ? 1 : $a gt $b ? -1 : 0 }

my $upperfirst = 'A' lt 'a';

# Beware: in future this may become hairier because of possible
# collation complications: qw(A a B c) can be sorted at least as
# any of the following
#
#	A a B b
#	A B a b
#	a b A B
#	a A b B
#
# All the above orders make sense.
#
# That said, EBCDIC sorts all small letters first, as opposed
# to ASCII which sorts all big letters first.

@harry = ('dog','cat','x','Cain','Abel');
@george = ('gone','chased','yz','punished','Axed');

$x = join('', sort @harry);
$expected = $upperfirst ? 'AbelCaincatdogx' : 'catdogxAbelCain';
print "# 1: x = '$x', expected = '$expected'\n";
print ($x eq $expected ? "ok 1\n" : "not ok 1\n");

$x = join('', sort( Backwards @harry));
$expected = $upperfirst ? 'xdogcatCainAbel' : 'CainAbelxdogcat';
print "# 2: x = '$x', expected = '$expected'\n";
print ($x eq $expected ? "ok 2\n" : "not ok 2\n");

$x = join('', sort( Backwards_stacked @harry));
$expected = $upperfirst ? 'xdogcatCainAbel' : 'CainAbelxdogcat';
print "# 3: x = '$x', expected = '$expected'\n";
print ($x eq $expected ? "ok 3\n" : "not ok 3\n");

$x = join('', sort @george, 'to', @harry);
$expected = $upperfirst ?
    'AbelAxedCaincatchaseddoggonepunishedtoxyz' :
    'catchaseddoggonepunishedtoxyzAbelAxedCain' ;
print "# 4: x = '$x', expected = '$expected'\n";
print ($x eq $expected ?"ok 4\n":"not ok 4\n");

@a = ();
@b = reverse @a;
print ("@b" eq "" ? "ok 5\n" : "not ok 5 (@b)\n");

@a = (1);
@b = reverse @a;
print ("@b" eq "1" ? "ok 6\n" : "not ok 6 (@b)\n");

@a = (1,2);
@b = reverse @a;
print ("@b" eq "2 1" ? "ok 7\n" : "not ok 7 (@b)\n");

@a = (1,2,3);
@b = reverse @a;
print ("@b" eq "3 2 1" ? "ok 8\n" : "not ok 8 (@b)\n");

@a = (1,2,3,4);
@b = reverse @a;
print ("@b" eq "4 3 2 1" ? "ok 9\n" : "not ok 9 (@b)\n");

@a = (10,2,3,4);
@b = sort {$a <=> $b;} @a;
print ("@b" eq "2 3 4 10" ? "ok 10\n" : "not ok 10 (@b)\n");

$sub = 'Backwards';
$x = join('', sort $sub @harry);
$expected = $upperfirst ? 'xdogcatCainAbel' : 'CainAbelxdogcat';
print "# 11: x = $x, expected = '$expected'\n";
print ($x eq $expected ? "ok 11\n" : "not ok 11\n");

$sub = 'Backwards_stacked';
$x = join('', sort $sub @harry);
$expected = $upperfirst ? 'xdogcatCainAbel' : 'CainAbelxdogcat';
print "# 12: x = $x, expected = '$expected'\n";
print ($x eq $expected ? "ok 12\n" : "not ok 12\n");

# literals, combinations

@b = sort (4,1,3,2);
print ("@b" eq '1 2 3 4' ? "ok 13\n" : "not ok 13\n");
print "# x = '@b'\n";

@b = sort grep { $_ } (4,1,3,2);
print ("@b" eq '1 2 3 4' ? "ok 14\n" : "not ok 14\n");
print "# x = '@b'\n";

@b = sort map { $_ } (4,1,3,2);
print ("@b" eq '1 2 3 4' ? "ok 15\n" : "not ok 15\n");
print "# x = '@b'\n";

@b = sort reverse (4,1,3,2);
print ("@b" eq '1 2 3 4' ? "ok 16\n" : "not ok 16\n");
print "# x = '@b'\n";

# redefining sort sub inside the sort sub should fail
sub twoface { *twoface = sub { $a <=> $b }; &twoface }
eval { @b = sort twoface 4,1,3,2 };
print ($@ =~ /redefine active sort/ ? "ok 17\n" : "not ok 17\n");

# redefining sort subs outside the sort should not fail
eval { no warnings 'redefine'; *twoface = sub { &Backwards } };
print $@ ? "not ok 18\n" : "ok 18\n";

eval { @b = sort twoface 4,1,3,2 };
print ("@b" eq '4 3 2 1' ? "ok 19\n" : "not ok 19 |@b|\n");

{
  no warnings 'redefine';
  *twoface = sub { *twoface = *Backwards; $a <=> $b };
}
eval { @b = sort twoface 4,1 };
print ($@ =~ /redefine active sort/ ? "ok 20\n" : "not ok 20\n");

{
  no warnings 'redefine';
  *twoface = sub {
                 eval 'sub twoface { $a <=> $b }';
		 die($@ =~ /redefine active sort/ ? "ok 21\n" : "not ok 21\n");
		 $a <=> $b;
	       };
}
eval { @b = sort twoface 4,1 };
print $@ ? "$@" : "not ok 21\n";

eval <<'CODE';
    my @result = sort main'Backwards 'one', 'two';
CODE
print $@ ? "not ok 22\n# $@" : "ok 22\n";

eval <<'CODE';
    # "sort 'one', 'two'" should not try to parse "'one" as a sort sub
    my @result = sort 'one', 'two';
CODE
print $@ ? "not ok 23\n# $@" : "ok 23\n";

{
  my $sortsub = \&Backwards;
  my $sortglob = *Backwards;
  my $sortglobr = \*Backwards;
  my $sortname = 'Backwards';
  @b = sort $sortsub 4,1,3,2;
  print ("@b" eq '4 3 2 1' ? "ok 24\n" : "not ok 24 |@b|\n");
  @b = sort $sortglob 4,1,3,2;
  print ("@b" eq '4 3 2 1' ? "ok 25\n" : "not ok 25 |@b|\n");
  @b = sort $sortname 4,1,3,2;
  print ("@b" eq '4 3 2 1' ? "ok 26\n" : "not ok 26 |@b|\n");
  @b = sort $sortglobr 4,1,3,2;
  print ("@b" eq '4 3 2 1' ? "ok 27\n" : "not ok 27 |@b|\n");
}

{
  my $sortsub = \&Backwards_stacked;
  my $sortglob = *Backwards_stacked;
  my $sortglobr = \*Backwards_stacked;
  my $sortname = 'Backwards_stacked';
  @b = sort $sortsub 4,1,3,2;
  print ("@b" eq '4 3 2 1' ? "ok 28\n" : "not ok 28 |@b|\n");
  @b = sort $sortglob 4,1,3,2;
  print ("@b" eq '4 3 2 1' ? "ok 29\n" : "not ok 29 |@b|\n");
  @b = sort $sortname 4,1,3,2;
  print ("@b" eq '4 3 2 1' ? "ok 30\n" : "not ok 30 |@b|\n");
  @b = sort $sortglobr 4,1,3,2;
  print ("@b" eq '4 3 2 1' ? "ok 31\n" : "not ok 31 |@b|\n");
}

{
  local $sortsub = \&Backwards;
  local $sortglob = *Backwards;
  local $sortglobr = \*Backwards;
  local $sortname = 'Backwards';
  @b = sort $sortsub 4,1,3,2;
  print ("@b" eq '4 3 2 1' ? "ok 32\n" : "not ok 32 |@b|\n");
  @b = sort $sortglob 4,1,3,2;
  print ("@b" eq '4 3 2 1' ? "ok 33\n" : "not ok 33 |@b|\n");
  @b = sort $sortname 4,1,3,2;
  print ("@b" eq '4 3 2 1' ? "ok 34\n" : "not ok 34 |@b|\n");
  @b = sort $sortglobr 4,1,3,2;
  print ("@b" eq '4 3 2 1' ? "ok 35\n" : "not ok 35 |@b|\n");
}

{
  local $sortsub = \&Backwards_stacked;
  local $sortglob = *Backwards_stacked;
  local $sortglobr = \*Backwards_stacked;
  local $sortname = 'Backwards_stacked';
  @b = sort $sortsub 4,1,3,2;
  print ("@b" eq '4 3 2 1' ? "ok 36\n" : "not ok 36 |@b|\n");
  @b = sort $sortglob 4,1,3,2;
  print ("@b" eq '4 3 2 1' ? "ok 37\n" : "not ok 37 |@b|\n");
  @b = sort $sortname 4,1,3,2;
  print ("@b" eq '4 3 2 1' ? "ok 38\n" : "not ok 38 |@b|\n");
  @b = sort $sortglobr 4,1,3,2;
  print ("@b" eq '4 3 2 1' ? "ok 39\n" : "not ok 39 |@b|\n");
}

## exercise sort builtins... ($a <=> $b already tested)
@a = ( 5, 19, 1996, 255, 90 );
@b = sort {
    my $dummy;		# force blockness
    return $b <=> $a
} @a;
print ("@b" eq '1996 255 90 19 5' ? "ok 40\n" : "not ok 40\n");
print "# x = '@b'\n";
$x = join('', sort { $a cmp $b } @harry);
$expected = $upperfirst ? 'AbelCaincatdogx' : 'catdogxAbelCain';
print ($x eq $expected ? "ok 41\n" : "not ok 41\n");
print "# x = '$x'; expected = '$expected'\n";
$x = join('', sort { $b cmp $a } @harry);
$expected = $upperfirst ? 'xdogcatCainAbel' : 'CainAbelxdogcat';
print ($x eq $expected ? "ok 42\n" : "not ok 42\n");
print "# x = '$x'; expected = '$expected'\n";
{
    use integer;
    @b = sort { $a <=> $b } @a;
    print ("@b" eq '5 19 90 255 1996' ? "ok 43\n" : "not ok 43\n");
    print "# x = '@b'\n";
    @b = sort { $b <=> $a } @a;
    print ("@b" eq '1996 255 90 19 5' ? "ok 44\n" : "not ok 44\n");
    print "# x = '@b'\n";
    $x = join('', sort { $a cmp $b } @harry);
    $expected = $upperfirst ? 'AbelCaincatdogx' : 'catdogxAbelCain';
    print ($x eq $expected ? "ok 45\n" : "not ok 45\n");
    print "# x = '$x'; expected = '$expected'\n";
    $x = join('', sort { $b cmp $a } @harry);
    $expected = $upperfirst ? 'xdogcatCainAbel' : 'CainAbelxdogcat';
    print ($x eq $expected ? "ok 46\n" : "not ok 46\n");
    print "# x = '$x'; expected = '$expected'\n";
}

# test that an optimized-away comparison block doesn't take any other
# arguments away with it
$x = join('', sort { $a <=> $b } 3, 1, 2);
print $x eq "123" ? "ok 47\n" : "not ok 47\n";

# test sorting in non-main package
package Foo;
@a = ( 5, 19, 1996, 255, 90 );
@b = sort { $b <=> $a } @a;
print ("@b" eq '1996 255 90 19 5' ? "ok 48\n" : "not ok 48\n");
print "# x = '@b'\n";

@b = sort main::Backwards_stacked @a;
print ("@b" eq '90 5 255 1996 19' ? "ok 49\n" : "not ok 49\n");
print "# x = '@b'\n";

# check if context for sort arguments is handled right

$test = 49;
sub test_if_list {
    my $gimme = wantarray;
    print "not " unless $gimme;
    ++$test;
    print "ok $test\n";
}
my $m = sub { $a <=> $b };

sub cxt_one { sort $m test_if_list() }
cxt_one();
sub cxt_two { sort { $a <=> $b } test_if_list() }
cxt_two();
sub cxt_three { sort &test_if_list() }
cxt_three();

sub test_if_scalar {
    my $gimme = wantarray;
    print "not " if $gimme or !defined($gimme);
    ++$test;
    print "ok $test\n";
}

$m = \&test_if_scalar;
sub cxt_four { sort $m 1,2 }
@x = cxt_four();
sub cxt_five { sort { test_if_scalar($a,$b); } 1,2 }
@x = cxt_five();
sub cxt_six { sort test_if_scalar 1,2 }
@x = cxt_six();

# test against a reentrancy bug
{
    package Bar;
    sub compare { $a cmp $b }
    sub reenter { my @force = sort compare qw/a b/ }
}
{
    my($def, $init) = (0, 0);
    @b = sort {
	$def = 1 if defined $Bar::a;
	Bar::reenter() unless $init++;
	$a <=> $b
    } qw/4 3 1 2/;
    print ("@b" eq '1 2 3 4' ? "ok 56\n" : "not ok 56\n");
    print "# x = '@b'\n";
    print !$def ? "ok 57\n" : "not ok 57\n";
}

# Bug 19991001.003
{
    sub routine { "one", "two" };
    @a = sort(routine(1));
    print "@a" eq "one two" ? "ok 58\n" : "not ok 58\n";
}


my $test = 59;
sub ok {
    print "not " unless $_[0] eq $_[1];
    print "ok $test - $_[2]\n";
    print "#[$_[0]] ne [$_[1]]\n" unless $_[0] eq $_[1];
    $test++;
}

# check for in-place optimisation of @a = sort @a
{
    my ($r1,$r2,@a);
    our @g;
    @g = (3,2,1); $r1 = \$g[2]; @g = sort @g; $r2 = \$g[0];
    ok "$r1-@g", "$r2-1 2 3", "inplace sort of global";

    @a = qw(b a c); $r1 = \$a[1]; @a = sort @a; $r2 = \$a[0];
    ok "$r1-@a", "$r2-a b c", "inplace sort of lexical";

    @g = (2,3,1); $r1 = \$g[1]; @g = sort { $b <=> $a } @g; $r2 = \$g[0];
    ok "$r1-@g", "$r2-3 2 1", "inplace reversed sort of global";

    @g = (2,3,1);
    $r1 = \$g[1]; @g = sort { $a<$b?1:$a>$b?-1:0 } @g; $r2 = \$g[0];
    ok "$r1-@g", "$r2-3 2 1", "inplace custom sort of global";

    sub mysort { $b cmp $a };
    @a = qw(b c a); $r1 = \$a[1]; @a = sort mysort @a; $r2 = \$a[0];
    ok "$r1-@a", "$r2-c b a", "inplace sort with function of lexical";

    use Tie::Array;
    my @t;
    tie @t, 'Tie::StdArray';

    @t = qw(b c a); @t = sort @t;
    ok "@t", "a b c", "inplace sort of tied array";

    @t = qw(b c a); @t = sort mysort @t;
    ok "@t", "c b a", "inplace sort of tied array with function";

    #  [perl #29790] don't optimise @a = ('a', sort @a) !

    @g = (3,2,1); @g = ('0', sort @g);
    ok "@g", "0 1 2 3", "un-inplace sort of global";
    @g = (3,2,1); @g = (sort(@g),'4');
    ok "@g", "1 2 3 4", "un-inplace sort of global 2";

    @a = qw(b a c); @a = ('x', sort @a);
    ok "@a", "x a b c", "un-inplace sort of lexical";
    @a = qw(b a c); @a = ((sort @a), 'x');
    ok "@a", "a b c x", "un-inplace sort of lexical 2";

    @g = (2,3,1); @g = ('0', sort { $b <=> $a } @g);
    ok "@g", "0 3 2 1", "un-inplace reversed sort of global";
    @g = (2,3,1); @g = ((sort { $b <=> $a } @g),'4');
    ok "@g", "3 2 1 4", "un-inplace reversed sort of global 2";

    @g = (2,3,1); @g = ('0', sort { $a<$b?1:$a>$b?-1:0 } @g);
    ok "@g", "0 3 2 1", "un-inplace custom sort of global";
    @g = (2,3,1); @g = ((sort { $a<$b?1:$a>$b?-1:0 } @g),'4');
    ok "@g", "3 2 1 4", "un-inplace custom sort of global 2";

    @a = qw(b c a); @a = ('x', sort mysort @a);
    ok "@a", "x c b a", "un-inplace sort with function of lexical";
    @a = qw(b c a); @a = ((sort mysort @a),'x');
    ok "@a", "c b a x", "un-inplace sort with function of lexical 2";
}




