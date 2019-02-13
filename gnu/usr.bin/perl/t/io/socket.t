#!perl

# sanity tests for socket functions

BEGIN {
    chdir 't' if -d 't';

    require "./test.pl";
    set_up_inc( '../lib' ) if -d '../lib' && -d '../ext';
    require Config; import Config;

    skip_all_if_miniperl();
    for my $needed (qw(d_socket d_getpbyname)) {
	if ($Config{$needed} ne 'define') {
	    skip_all("-- \$Config{$needed} undefined");
	}
    }
    unless ($Config{extensions} =~ /\bSocket\b/) {
	skip_all('-- Socket not available');
    }
}

use strict;
use Socket;

our $TODO;

$| = 1; # ensure test output is synchronous so processes don't conflict

my $tcp = getprotobyname('tcp')
    or skip_all("no tcp protocol available ($!)");
my $udp = getprotobyname('udp')
    or note "getprotobyname('udp') failed: $!";

my $local = gethostbyname('localhost')
    or note "gethostbyname('localhost') failed: $!";

my $fork = $Config{d_fork} || $Config{d_pseudofork};

{
    # basic socket creation
    socket(my $sock, PF_INET, SOCK_STREAM, $tcp)
	or skip_all('socket() for tcp failed ($!), nothing else will work');
    ok(close($sock), "close the socket");
}

SKIP: {
    # test it all in TCP
    $local or skip("No localhost", 3);

    ok(socket(my $serv, PF_INET, SOCK_STREAM, $tcp), "make a tcp socket");
    my $bind_at = pack_sockaddr_in(0, $local);
    ok(bind($serv, $bind_at), "bind works")
	or skip("Couldn't bind to localhost", 4);
    my $bind_name = getsockname($serv);
    ok($bind_name, "getsockname() on bound socket");
    my ($bind_port) = unpack_sockaddr_in($bind_name);

    print "# port $bind_port\n";

  SKIP:
    {
	ok(listen($serv, 5), "listen() works")
	  or diag "listen error: $!";

	$fork or skip("No fork", 2);
	my $pid = fork;
	my $send_data = "test" x 50_000;
	if ($pid) {
	    # parent
	    ok(socket(my $accept, PF_INET, SOCK_STREAM, $tcp),
	       "make accept tcp socket");
	    ok(my $addr = accept($accept, $serv), "accept() works")
		or diag "accept error: $!";
            binmode $accept;
	    SKIP: {
		skip "no fcntl", 1 unless $Config{d_fcntl};
		my $acceptfd = fileno($accept);
		fresh_perl_is(qq(
		    print open(F, "+<&=$acceptfd") ? 1 : 0, "\\n";
		), "0\n", {}, "accepted socket not inherited across exec");
	    }
	    my $sent_total = 0;
	    while ($sent_total < length $send_data) {
		my $sent = send($accept, substr($send_data, $sent_total), 0);
		defined $sent or last;
		$sent_total += $sent;
	    }
	    my $shutdown = shutdown($accept, 1);

	    # wait for the remote to close so data isn't lost in
	    # transit on a certain broken implementation
	    <$accept>;
	    # child tests are printed once we hit eof
	    curr_test(curr_test()+5);
	    waitpid($pid, 0);

	    ok($shutdown, "shutdown() works");
	}
	elsif (defined $pid) {
	    curr_test(curr_test()+3);
	    #sleep 1;
	    # child
	    ok_child(close($serv), "close server socket in child");
	    ok_child(socket(my $child, PF_INET, SOCK_STREAM, $tcp),
	       "make child tcp socket");

	    ok_child(connect($child, $bind_name), "connect() works")
		or diag "connect error: $!";
            binmode $child;
	    my $buf;
	    my $recv_peer = recv($child, $buf, 1000, 0);
	    {
        local $TODO = "[perl #122657] Hurd doesn't populate sin_len correctly"
		    if $^O eq "gnu";
		# [perl #118843]
		ok_child($recv_peer eq '' || $recv_peer eq getpeername $child,
			 "peer from recv() should be empty or the remote name");
	    }
	    while(defined recv($child, my $tmp, 1000, 0)) {
		last if length $tmp == 0;
		$buf .= $tmp;
	    }
	    is_child($buf, $send_data, "check we received the data");
	    close($child);
	    end_child();

	    exit(0);
	}
	else {
	    # failed to fork
	    diag "fork() failed $!";
	    skip("fork() failed", 2);
	}
    }
}

SKIP:
{
    eval { require Errno; defined &Errno::EMFILE }
      or skip "Can't load Errno or EMFILE not defined", 1;
    # stdio might return strange values in errno if it runs
    # out of FILE entries, and does on darwin
    $^O eq "darwin" && exists $ENV{PERLIO} && $ENV{PERLIO} =~ /stdio/
      and skip "errno values from stdio are unspecified", 1;
    my @socks;
    my $sock_limit = 1000; # don't consume every file in the system
    # Default limits on various systems I have:
    #  65536 - Linux
    #    256 - Solaris
    #    128 - NetBSD
    #    256 - Cygwin
    #    256 - darwin
    while (@socks < $sock_limit) {
        socket my $work, PF_INET, SOCK_STREAM, $tcp
          or last;
        push @socks, $work;
    }
    @socks == $sock_limit
      and skip "Didn't run out of open handles", 1;
    is(0+$!, Errno::EMFILE(), "check correct errno for too many files");
}

{
    my $sock;
    my $proto = getprotobyname('tcp');
    socket($sock, PF_INET, SOCK_STREAM, $proto);
    accept($sock, $sock);
    ok('RT #7614: still alive after accept($sock, $sock)');
}

SKIP: {
    skip "no fcntl", 1 unless $Config{d_fcntl};
    my $sock;
    socket($sock, PF_INET, SOCK_STREAM, $tcp) or die "socket: $!";
    my $sockfd = fileno($sock);
    fresh_perl_is(qq(
	print open(F, "+<&=$sockfd") ? 1 : 0, "\\n";
    ), "0\n", {}, "fresh socket not inherited across exec");
}

done_testing();

my @child_tests;
sub ok_child {
    my ($ok, $note) = @_;
    push @child_tests, ( $ok ? "ok " : "not ok ") . curr_test() . " - $note "
	. ( $TODO ? "# TODO $TODO" : "" ) . "\n";
    curr_test(curr_test()+1);
}

sub is_child {
    my ($got, $want, $note) = @_;
    ok_child($got eq $want, $note);
}

sub end_child {
    print @child_tests;
}

