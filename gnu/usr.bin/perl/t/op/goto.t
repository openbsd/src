#!./perl

# "This IS structured code.  It's just randomly structured."

BEGIN {
    chdir 't' if -d 't';
    @INC = qw(. ../lib);
}

print "1..33\n";

require "test.pl";

while ($?) {
    $foo = 1;
  label1:
    $foo = 2;
    goto label2;
} continue {
    $foo = 0;
    goto label4;
  label3:
    $foo = 4;
    goto label4;
}
goto label1;

$foo = 3;

label2:
print "#1\t:$foo: == 2\n";
if ($foo == 2) {print "ok 1\n";} else {print "not ok 1\n";}
goto label3;

label4:
print "#2\t:$foo: == 4\n";
if ($foo == 4) {print "ok 2\n";} else {print "not ok 2\n";}

$PERL = ($^O eq 'MSWin32') ? '.\perl' : ($^O eq 'MacOS') ? $^X : ($^O eq 'NetWare') ? 'perl' : './perl';
$CMD = qq[$PERL -e "goto foo;" 2>&1 ];
$x = `$CMD`;

if ($x =~ /label/) {print "ok 3\n";} else {print "not ok 3\n";}

sub foo {
    goto bar;
    print "not ok 4\n";
    return;
bar:
    print "ok 4\n";
}

&foo;

sub bar {
    $x = 'bypass';
    eval "goto $x";
}

&bar;
exit;

FINALE:
print "ok 13\n";

# does goto LABEL handle block contexts correctly?

my $cond = 1;
for (1) {
    if ($cond == 1) {
	$cond = 0;
	goto OTHER;
    }
    elsif ($cond == 0) {
      OTHER:
	$cond = 2;
	print "ok 14\n";
	goto THIRD;
    }
    else {
      THIRD:
	print "ok 15\n";
    }
}
print "ok 16\n";

# Does goto work correctly within a for(;;) loop?
#  (BUG ID 20010309.004)

for(my $i=0;!$i++;) {
  my $x=1;
  goto label;
  label: print (defined $x?"ok ": "not ok ", "17\n")
}

# Does goto work correctly going *to* a for(;;) loop?
#  (make sure it doesn't skip the initializer)

my ($z, $y) = (0);
FORL1: for($y="ok 18\n"; $z;) {print $y; goto TEST19}
($y,$z) = ("not ok 18\n", 1);
goto FORL1;

# Even from within the loop?

TEST19: $z = 0;
FORL2: for($y="ok 19\n"; 1;) {
  if ($z) {
    print $y;
    last;
  }
  ($y, $z) = ("not ok 19\n", 1);
  goto FORL2;
}

# Does goto work correctly within a try block?
#  (BUG ID 20000313.004)

my $ok = 0;
eval {
  my $variable = 1;
  goto LABEL20;
  LABEL20: $ok = 1 if $variable;
};
print ($ok&&!$@ ? "ok 20\n" : "not ok 20\n");

# And within an eval-string?


$ok = 0;
eval q{
  my $variable = 1;
  goto LABEL21;
  LABEL21: $ok = 1 if $variable;
};
print ($ok&&!$@ ? "ok 21\n" : "not ok 21\n");


# Test that goto works in nested eval-string
$ok = 0;
{eval q{
  eval q{
    goto LABEL22;
  };
  $ok = 0;
  last;

  LABEL22: $ok = 1;
};
$ok = 0 if $@;
}
print ($ok ? "ok 22\n" : "not ok 22\n");

{
    my $false = 0;

    $ok = 0;
    { goto A; A: $ok = 1 } continue { }
    print "not " unless $ok;
    print "ok 23 - #20357 goto inside /{ } continue { }/ loop\n";

    $ok = 0;
    { do { goto A; A: $ok = 1 } while $false }
    print "not " unless $ok;
    print "ok 24 - #20154 goto inside /do { } while ()/ loop\n";

    $ok = 0;
    foreach(1) { goto A; A: $ok = 1 } continue { };
    print "not " unless $ok;
    print "ok 25 - goto inside /foreach () { } continue { }/ loop\n";

    $ok = 0;
    sub a {
	A: { if ($false) { redo A; B: $ok = 1; redo A; } }
	goto B unless $r++
    }
    a();
    print "not " unless $ok;
    print "ok 26 - #19061 loop label wiped away by goto\n";

    $ok = 0;
    for ($p=1;$p && goto A;$p=0) { A: $ok = 1 }
    print "not " unless $ok;
    print "ok 27 - weird case of goto and for(;;) loop\n";
}

# bug #9990 - don't prematurely free the CV we're &going to.

sub f1 {
    my $x;
    goto sub { $x; print "ok 28 - don't prematurely free CV\n" }
}
f1();

# bug #22181 - this used to coredump or make $x undefined, due to
# erroneous popping of the inner BLOCK context

for ($i=0; $i<2; $i++) {
    my $x = 1;
    goto LABEL29;
    LABEL29:
    print "not " if !defined $x || $x != 1;
}
print "ok 29 - goto in for(;;) with continuation\n";

# bug #22299 - goto in require doesn't find label

open my $f, ">goto01.pm" or die;
print $f <<'EOT';
package goto01;
goto YYY;
die;
YYY: print "OK\n";
1;
EOT
close $f;

curr_test(30);
my $r = runperl(prog => 'use goto01; print qq[DONE\n]');
is($r, "OK\nDONE\n", "goto within use-d file"); 
unlink "goto01.pm";

# test for [perl #24108]
sub i_return_a_label {
    print "ok 31 - i_return_a_label called\n";
    return "returned_label";
}
eval { goto +i_return_a_label; };
print "not ";
returned_label : print "ok 32 - done to returned_label\n";

# [perl #29708] - goto &foo could leave foo() at depth two with
# @_ == PL_sv_undef, causing a coredump


my $r = runperl(
    prog =>
	'sub f { return if $d; $d=1; my $a=sub {goto &f}; &$a; f() } f(); print qq(ok\n)',
    stderr => 1
    );
print "not " if $r ne "ok\n";
print "ok 33 - avoid pad without an \@_\n";


exit;

bypass:
print "ok 5\n";

# Test autoloading mechanism.

sub two {
    ($pack, $file, $line) = caller;	# Should indicate original call stats.
    print "@_ $pack $file $line" eq "1 2 3 main $FILE $LINE"
	? "ok 7\n"
	: "not ok 7\n";
}

sub one {
    eval <<'END';
    sub one { print "ok 6\n"; goto &two; print "not ok 6\n"; }
END
    goto &one;
}

$FILE = __FILE__;
$LINE = __LINE__ + 1;
&one(1,2,3);

$wherever = NOWHERE;
eval { goto $wherever };
print $@ =~ /Can't find label NOWHERE/ ? "ok 8\n" : "not ok 8\n";

# see if a modified @_ propagates
{
  package Foo;
  sub DESTROY	{ my $s = shift; print "ok $s->[0]\n"; }
  sub show	{ print "# @_\nnot ok $_[0][0]\n" if @_ != 5; }
  sub start	{ push @_, 1, "foo", {}; goto &show; }
  for (9..11)	{ start(bless([$_]), 'bar'); }
}

sub auto {
    goto &loadit;
}

sub AUTOLOAD { print @_ }

auto("ok 12\n");

$wherever = FINALE;
goto $wherever;
