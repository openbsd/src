#!/usr/bin/perl

use warnings;
use strict;

use File::Temp qw(tempdir);
use File::Spec::Functions;
use IO::Socket;
use IO::Socket::UNIX;
use Socket;
use Config;
use Test::More;

plan skip_all => "UNIX domain sockets not implemented on $^O"
  if ($^O =~ m/^(?:qnx|nto|vos|MSWin32|VMS)$/);

plan tests => 15;

my $socketpath = catfile(tempdir( CLEANUP => 1 ), 'testsock');

# start testing stream sockets:
my $listener = IO::Socket::UNIX->new(Type => SOCK_STREAM,
				     Listen => 1,
				     Local => $socketpath);
ok(defined($listener), 'stream socket created');

my $p = $listener->protocol();
ok(defined($p), 'protocol defined');
my $d = $listener->sockdomain();
ok(defined($d), 'domain defined');
my $s = $listener->socktype();
ok(defined($s), 'type defined');

SKIP: {
    skip "fork not available", 4
	unless $Config{d_fork} || $Config{d_pseudofork};

    my $cpid = fork();
    if (0 == $cpid) {
	# the child:
	sleep(1);
	my $connector = IO::Socket::UNIX->new(Peer => $socketpath);
	exit(0);
    } else {
	ok(defined($cpid), 'spawned a child');
    }

    my $new = $listener->accept();

    is($new->sockdomain(), $d, 'domain match');
  SKIP: {
      skip "no Socket::SO_PROTOCOL", 1 if !defined(eval { Socket::SO_PROTOCOL });
      skip "SO_PROTOCOL defined but not implemented", 1
         if !defined $new->sockopt(Socket::SO_PROTOCOL);
      is($new->protocol(), $p, 'protocol match');
    }
  SKIP: {
      skip "no Socket::SO_TYPE", 1 if !defined(eval { Socket::SO_TYPE });
      skip "SO_TYPE defined but not implemented", 1
         if !defined $new->sockopt(Socket::SO_TYPE);
      is($new->socktype(), $s, 'type match');
    }

    unlink($socketpath);
    wait();
}

undef $TODO;
SKIP: {
    skip "datagram unix sockets not supported on $^O", 7
      if $^O eq "haiku";
    # now test datagram sockets:
    $listener = IO::Socket::UNIX->new(Type => SOCK_DGRAM,
				      Local => $socketpath);
    ok(defined($listener), 'datagram socket created');

    $p = $listener->protocol();
    ok(defined($p), 'protocol defined');
    $d = $listener->sockdomain();
    ok(defined($d), 'domain defined');
    $s = $listener->socktype();
    ok(defined($s), 'type defined');

    my $new = IO::Socket::UNIX->new_from_fd($listener->fileno(), 'r+');

    is($new->sockdomain(), $d, 'domain match');
    SKIP: {
      skip "no Socket::SO_PROTOCOL", 1 if !defined(eval { Socket::SO_PROTOCOL });
      skip "SO_PROTOCOL defined but not implemented", 1
         if !defined $new->sockopt(Socket::SO_PROTOCOL);
      is($new->protocol(), $p, 'protocol match');
    }
    SKIP: {
      skip "AIX: getsockopt(SO_TYPE) is badly broken on UDP/UNIX sockets", 1
         if $^O eq "aix";
      skip "no Socket::SO_TYPE", 1 if !defined(eval { Socket::SO_TYPE });
      skip "SO_TYPE defined but not implemented", 1
         if !defined $new->sockopt(Socket::SO_TYPE);
      is($new->socktype(), $s, 'type match');
    }
}
unlink($socketpath);
