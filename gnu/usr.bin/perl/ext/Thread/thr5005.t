#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require Config; import Config;
    if (! $Config{'use5005threads'}) {
	print "1..0 # Skip: no use5005threads\n";
	exit 0;
    }

    # XXX known trouble with global destruction
    $ENV{PERL_DESTRUCT_LEVEL} = 0 unless $ENV{PERL_DESTRUCT_LEVEL} > 3;
}
$| = 1;
print "1..74\n";
use Thread 'yield';
print "ok 1\n";

sub content
{
 print shift;
 return shift;
}

# create a thread passing args and immedaietly wait for it.
my $t = new Thread \&content,("ok 2\n","ok 3\n", 1..1000);
print $t->join;

# check that lock works ...
{lock $foo;
 $t = new Thread sub { lock $foo; print "ok 5\n" };
 print "ok 4\n";
}
$t->join;

sub dorecurse
{
 my $val = shift;
 my $ret;
 print $val;
 if (@_)
  {
   $ret = Thread->new(\&dorecurse, @_);
   $ret->join;
  }
}

$t = new Thread \&dorecurse, map { "ok $_\n" } 6..10;
$t->join;

# test that sleep lets other thread run
$t = new Thread \&dorecurse,"ok 11\n";
sleep 6;
print "ok 12\n";
$t->join;

sub islocked : locked {
 my $val = shift;
 my $ret;
 print $val;
 if (@_)
  {
   $ret = Thread->new(\&islocked, shift);
  }
 $ret;
}

$t = Thread->new(\&islocked, "ok 13\n", "ok 14\n");
$t->join->join;

{
    package Loch::Ness;
    sub new { bless [], shift }
    sub monster : locked : method {
	my($s, $m) = @_;
	print "ok $m\n";
    }
    sub gollum { &monster }
}
Loch::Ness->monster(15);
Loch::Ness->new->monster(16);
Loch::Ness->gollum(17);
Loch::Ness->new->gollum(18);

my $short = "This is a long string that goes on and on.";
my $shorte = " a long string that goes on and on.";
my $long  = "This is short.";
my $longe  = " short.";
my $thr1 = new Thread \&threaded, $short, $shorte, "19";
my $thr2 = new Thread \&threaded, $long, $longe, "20";
my $thr3 = new Thread \&testsprintf, "21";

sub testsprintf {
  my $testno = shift;
  # this may coredump if thread vars are not properly initialised
  my $same = sprintf "%.0f", $testno;
  if ($testno eq $same) {
    print "ok $testno\n";
  } else {
    print "not ok $testno\t# '$testno' ne '$same'\n";
  }
}

sub threaded {
  my ($string, $string_end, $testno) = @_;

  # Do the match, saving the output in appropriate variables
  $string =~ /(.*)(is)(.*)/;
  # Yield control, allowing the other thread to fill in the match variables
  yield();
  # Examine the match variable contents; on broken perls this fails
  if ($3 eq $string_end) {
    print "ok $testno\n";
  }
  else {
    warn <<EOT;

#
# This is a KNOWN FAILURE, and one of the reasons why threading
# is still an experimental feature.  It is here to stop people
# from deploying threads in production. ;-)
#
EOT
    print "not ok $testno # other thread filled in match variables\n";
  }
}
$thr1->join;
$thr2->join;
$thr3->join;
print "ok 22\n";

{
    my $THRf_STATE_MASK = 7;
    my $THRf_R_JOINABLE = 0;
    my $THRf_R_JOINED = 1;
    my $THRf_R_DETACHED = 2;
    my $THRf_ZOMBIE = 3;
    my $THRf_DEAD = 4;
    my $THRf_DID_DIE = 8;
    sub _test {
	my($test, $t, $state, $die) = @_;
	my $flags = $t->flags;
	if (($flags & $THRf_STATE_MASK) == $state
		&& !($flags & $THRf_DID_DIE) == !$die) {
	    print "ok $test\n";
	} else {
	    print <<BAD;
not ok $test\t# got flags $flags not @{[ $state + ($die ? $THRf_DID_DIE : 0) ]}
BAD
	}
    }

    my @t;
    push @t, (
	Thread->new(sub { sleep 4; die "thread die\n" }),
	Thread->new(sub { die "thread die\n" }),
	Thread->new(sub { sleep 4; 1 }),
	Thread->new(sub { 1 }),
    ) for 1, 2;
    $_->detach for @t[grep $_ & 4, 0..$#t];

    sleep 1;
    my $test = 23;
    for (0..7) {
	my $t = $t[$_];
	my $flags = ($_ & 1)
	    ? ($_ & 4) ? $THRf_DEAD : $THRf_ZOMBIE
	    : ($_ & 4) ? $THRf_R_DETACHED : $THRf_R_JOINABLE;
	_test($test++, $t, $flags, (($_ & 3) != 1) ? 0 : $THRf_DID_DIE);
	printf "%sok %s\n", !$t->done == !($_ & 1) ? "" : "not ", $test++;
    }
#   $test = 39;
    for (grep $_ & 1, 0..$#t) {
	next if $_ & 4;		# can't join detached threads
	$t[$_]->eval;
	my $die = ($_ & 2) ? "" : "thread die\n";
	printf "%sok %s\n", $@ eq $die ? "" : "not ", $test++;
    }
#   $test = 41;
    for (0..7) {
	my $t = $t[$_];
	my $flags = ($_ & 1)
	    ? ($_ & 4) ? $THRf_DEAD : $THRf_DEAD
	    : ($_ & 4) ? $THRf_R_DETACHED : $THRf_R_JOINABLE;
	_test($test++, $t, $flags, (($_ & 3) != 1) ? 0 : $THRf_DID_DIE);
	printf "%sok %s\n", !$t->done == !($_ & 1) ? "" : "not ", $test++;
    }
#   $test = 57;
    for (grep !($_ & 1), 0..$#t) {
	next if $_ & 4;		# can't join detached threads
	$t[$_]->eval;
	my $die = ($_ & 2) ? "" : "thread die\n";
	printf "%sok %s\n", $@ eq $die ? "" : "not ", $test++;
    }
    sleep 1;	# make sure even the detached threads are done sleeping
#   $test = 59;
    for (0..7) {
	my $t = $t[$_];
	my $flags = ($_ & 1)
	    ? ($_ & 4) ? $THRf_DEAD : $THRf_DEAD
	    : ($_ & 4) ? $THRf_DEAD : $THRf_DEAD;
	_test($test++, $t, $flags, ($_ & 2) ? 0 : $THRf_DID_DIE);
	printf "%sok %s\n", $t->done ? "" : "not ", $test++;
    }
#   $test = 75;
}
