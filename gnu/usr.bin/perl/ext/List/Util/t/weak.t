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

use vars qw($skip);

BEGIN {
  $|=1;
  require Scalar::Util;
  if (grep { /weaken/ } @Scalar::Util::EXPORT_FAIL) {
    print("1..0\n");
    $skip=1;
  }

  $DEBUG = 0;

  if ($DEBUG && eval { require Devel::Peek } ) {
    Devel::Peek->import('Dump');
  }
  else {
    *Dump = sub {};
  }
}

eval <<'EOT' unless $skip;
use Scalar::Util qw(weaken isweak);
print "1..22\n";

######################### End of black magic.

$cnt = 0;

sub ok {
	++$cnt;
	if($_[0]) { print "ok $cnt\n"; } else {print "not ok $cnt\n"; }
	return $_[0];
}

$| = 1;

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
print "# START:\n";
Dump($y); Dump($z);

ok( $y ne "" and $z ne "" );
weaken($y);

print "# WEAK:\n";
Dump($y); Dump($z);

ok( $y ne "" and $z ne "" );
undef($z);

print "# UNDZ:\n";
Dump($y); Dump($z);

ok( not (defined($y) and defined($z)) );
undef($y);

print "# UNDY:\n";
Dump($y); Dump($z);

ok( not (defined($y) and defined($z)) );

print "# FIN:\n";
Dump($y); Dump($z);

# exit(0);

# }
# {

# 
# Case 2: one reference, which is weakened
#

# kill 5,$$;

print "# CASE 2:\n";

{
	my $x = "foo";
	$y = \$x;
}

ok( $y ne "" );
print "# BW: \n";
Dump($y);
weaken($y);
print "# AW: \n";
Dump($y);
ok( not defined $y  );

print "# EXITBLOCK\n";
}

# exit(0);

# 
# Case 3: a circular structure
#

# kill 5, $$;

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
	ok( $y ne "" );
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
ok($z ne "");


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

if ($] < 5.008003) {
    # Doesn't work for older perls, see bug [perl #24506]
    print "# Skip next 5 tests on perl $]\n";
    for (1..5) {
	ok(1);
    }
}
else {
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
EOT
