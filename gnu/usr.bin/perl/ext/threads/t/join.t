
BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require Config; import Config;
    unless ($Config{'useithreads'}) {
        print "1..0 # Skip: no useithreads\n";
        exit 0;
    }
}

use ExtUtils::testlib;
use strict;
BEGIN { print "1..10\n" };
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

ok(1,"");


{
    my $retval = threads->create(sub { return ("hi") })->join();
    ok($retval eq 'hi', "Check basic returnvalue");
}
{
    my ($thread) = threads->create(sub { return (1,2,3) });
    my @retval = $thread->join();
    ok($retval[0] == 1 && $retval[1] == 2 && $retval[2] == 3);
}
{
    my $retval = threads->create(sub { return [1] })->join();
    ok($retval->[0] == 1,"Check that a array ref works");
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
    ok($$retval eq 'hi');
}
{
    my $test = "hi";
    share($test);
    my $retval = threads->create(sub { return $_[0]}, \$test)->join();
    ok($$retval eq 'hi');
    $test = "foo";
    ok($$retval eq 'foo');
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
