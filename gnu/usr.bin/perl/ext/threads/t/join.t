BEGIN {
    chdir 't' if -d 't';
    push @INC, '../lib';
    require Config; import Config;
    unless ($Config{'useithreads'}) {
        print "1..0 # Skip: no useithreads\n";
        exit 0;
    }
}

use ExtUtils::testlib;
use strict;
BEGIN { print "1..12\n" };
use threads;
use threads::shared;

my $test_id = 1;
share($test_id);
use Devel::Peek qw(Dump);

sub ok {
    my ($ok, $name) = @_;

    # You have to do it this way or VMS will get confused.
    print $ok ? "ok $test_id - $name\n" : "not ok $test_id - $name\n";

    printf "# Failed test at line %d\n", (caller)[2] unless $ok;
    $test_id++;
    return $ok;
}

sub skip {
    ok(1, "# Skipped: @_");
}

ok(1,"");


{
    my $retval = threads->create(sub { return ("hi") })->join();
    ok($retval eq 'hi', "Check basic returnvalue");
}
{
    my ($thread) = threads->create(sub { return (1,2,3) });
    my @retval = $thread->join();
    ok($retval[0] == 1 && $retval[1] == 2 && $retval[2] == 3,'');
}
{
    my $retval = threads->create(sub { return [1] })->join();
    ok($retval->[0] == 1,"Check that a array ref works",);
}
{
    my $retval = threads->create(sub { return { foo => "bar" }})->join();
    ok($retval->{foo} eq 'bar',"Check that hash refs work");
}
{
    my $retval = threads->create( sub {
	open(my $fh, "+>threadtest") || die $!;
	print $fh "test\n";
	return $fh;
    })->join();
    ok(ref($retval) eq 'GLOB', "Check that we can return FH $retval");
    print $retval "test2\n";
#    seek($retval,0,0);
#    ok(<$retval> eq "test\n");
    close($retval);
    unlink("threadtest");
}
{
    my $test = "hi";
    my $retval = threads->create(sub { return $_[0]}, \$test)->join();
    ok($$retval eq 'hi','');
}
{
    my $test = "hi";
    share($test);
    my $retval = threads->create(sub { return $_[0]}, \$test)->join();
    ok($$retval eq 'hi','');
    $test = "foo";
    ok($$retval eq 'foo','');
}
{
    my %foo;
    share(%foo);
    threads->create(sub { 
	my $foo;
	share($foo);
	$foo = "thread1";
	return $foo{bar} = \$foo;
    })->join();
    ok(1,"");
}

# We parse ps output so this is OS-dependent.
if ($^O eq 'linux') {
  # First modify $0 in a subthread.
  print "# mainthread: \$0 = $0\n";
  threads->new( sub {
		  print "# subthread: \$0 = $0\n";
		  $0 = "foobar";
		  print "# subthread: \$0 = $0\n" } )->join;
  print "# mainthread: \$0 = $0\n";
  print "# pid = $$\n";
  if (open PS, "ps -f |") { # Note: must work in (all) systems.
    my ($sawpid, $sawexe);
    while (<PS>) {
      chomp;
      print "# [$_]\n";
      if (/^\S+\s+$$\s/) {
	$sawpid++;
	if (/\sfoobar\s*$/) { # Linux 2.2 leaves extra trailing spaces.
	  $sawexe++;
        }
	last;
      }
    }
    close PS or die;
    if ($sawpid) {
      ok($sawpid && $sawexe, 'altering $0 is effective');
    } else {
      skip("\$0 check: did not see pid $$ in 'ps -f |'");
    }
  } else {
    skip("\$0 check: opening 'ps -f |' failed: $!");
  }
} else {
  skip("\$0 check: only on Linux");
}

{
    my $t = threads->new(sub {});
    $t->join;
    my $x = threads->new(sub {});
    $x->join;
    eval {
      $t->join;
    };
    my $ok = 0;
    $ok++ if($@ =~/Thread already joined/);
    ok($ok, "Double join works");
}


