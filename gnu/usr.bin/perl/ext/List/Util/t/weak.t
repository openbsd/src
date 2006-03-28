#!./perl

BEGIN {
    unless (-d 'blib') {
	chdir 't' if -d 't';
	@INC = '../lib';
	require Config; import Config;
	keys %Config; # Silence warning
	if ($Config{extensions} !~ /\bList\/Util\b/) {
	    print "1..0 # Skip: List::Util was not built\n";
	    exit 0;
	}
    }
}

use Scalar::Util ();
use Test::More  (grep { /weaken/ } @Scalar::Util::EXPORT_FAIL)
			? (skip_all => 'weaken requires XS version')
			: (tests => 22);

if (0) {
  require Devel::Peek;
  Devel::Peek->import('Dump');
}
else {
  *Dump = sub {};
}

Scalar::Util->import(qw(weaken isweak));

if(1) {

my ($y,$z);

#
# Case 1: two references, one is weakened, the other is then undef'ed.
#

{
	my $x = "foo";
	$y = \$x;
	$z = \$x;
}
print "# START\n";
Dump($y); Dump($z);

ok( ref($y) and ref($z));

print "# WEAK:\n";
weaken($y);
Dump($y); Dump($z);

ok( ref($y) and ref($z));

print "# UNDZ:\n";
undef($z);
Dump($y); Dump($z);

ok( not (defined($y) and defined($z)) );

print "# UNDY:\n";
undef($y);
Dump($y); Dump($z);

ok( not (defined($y) and defined($z)) );

print "# FIN:\n";
Dump($y); Dump($z);


# 
# Case 2: one reference, which is weakened
#

print "# CASE 2:\n";

{
	my $x = "foo";
	$y = \$x;
}

ok( ref($y) );
print "# BW: \n";
Dump($y);
weaken($y);
print "# AW: \n";
Dump($y);
ok( not defined $y  );

print "# EXITBLOCK\n";
}

# 
# Case 3: a circular structure
#

$flag = 0;
{
	my $y = bless {}, Dest;
	Dump($y);
	print "# 1: $y\n";
	$y->{Self} = $y;
	Dump($y);
	print "# 2: $y\n";
	$y->{Flag} = \$flag;
	print "# 3: $y\n";
	weaken($y->{Self});
	print "# WKED\n";
	ok( ref($y) );
	print "# VALS: HASH ",$y,"   SELF ",\$y->{Self},"  Y ",\$y, 
		"    FLAG: ",\$y->{Flag},"\n";
	print "# VPRINT\n";
}
print "# OUT $flag\n";
ok( $flag == 1 );

print "# AFTER\n";

undef $flag;

print "# FLAGU\n";

#
# Case 4: a more complicated circular structure
#

$flag = 0;
{
	my $y = bless {}, Dest;
	my $x = bless {}, Dest;
	$x->{Ref} = $y;
	$y->{Ref} = $x;
	$x->{Flag} = \$flag;
	$y->{Flag} = \$flag;
	weaken($x->{Ref});
}
ok( $flag == 2 );

#
# Case 5: deleting a weakref before the other one
#

{
	my $x = "foo";
	$y = \$x;
	$z = \$x;
}

print "# CASE5\n";
Dump($y);

weaken($y);
Dump($y);
undef($y);

ok( not defined $y);
ok( ref($z) );


#
# Case 6: test isweakref
#

$a = 5;
ok(!isweak($a));
$b = \$a;
ok(!isweak($b));
weaken($b);
ok(isweak($b));
$b = \$a;
ok(!isweak($b));

$x = {};
weaken($x->{Y} = \$a);
ok(isweak($x->{Y}));
ok(!isweak($x->{Z}));

#
# Case 7: test weaken on a read only ref
#

SKIP: {
    # Doesn't work for older perls, see bug [perl #24506]
    skip("Test does not work with perl < 5.8.3", 5) if $] < 5.008003;

    $a = eval '\"hello"';
    ok(ref($a)) or print "# didn't get a ref from eval\n";
    $b = $a;
    eval{weaken($b)};
    # we didn't die
    ok($@ eq "") or print "# died with $@\n";
    ok(isweak($b));
    ok($$b eq "hello") or print "# b is '$$b'\n";
    $a="";
    ok(not $b) or print "# b didn't go away\n";
}

package Dest;

sub DESTROY {
	print "# INCFLAG\n";
	${$_[0]{Flag}} ++;
}
