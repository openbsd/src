#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require Config; import Config;
    if ($Config{'extensions'} !~ /\bSocket\b/ && 
        !(($^O eq 'VMS') && $Config{d_socket})) {
	print "1..0\n";
	exit 0;
    }
    $has_alarm = $Config{d_alarm};
}
	
use Socket;

print "1..17\n";

$has_echo = $^O ne 'MSWin32';
$alarmed = 0;
sub arm      { $alarmed = 0; alarm(shift) if $has_alarm }
sub alarmed  { $alarmed = 1 }
$SIG{ALRM} = 'alarmed'                    if $has_alarm;

if (socket(T,PF_INET,SOCK_STREAM,6)) {
  print "ok 1\n";
  
  arm(5);
  my $host = $^O eq 'MacOS' || ($^O eq 'irix' && $Config{osvers} == 5) ?
                 '127.0.0.1' : 'localhost';
  my $localhost = inet_aton($host);

  if ($has_echo && defined $localhost && connect(T,pack_sockaddr_in(7,$localhost))){
	arm(0);

	print "ok 2\n";

	print "# Connected to " .
		inet_ntoa((unpack_sockaddr_in(getpeername(T)))[1])."\n";

	arm(5);
	syswrite(T,"hello",5);
	arm(0);

	arm(5);
	$read = sysread(T,$buff,10);	# Connection may be granted, then closed!
	arm(0);

	while ($read > 0 && length($buff) < 5) {
	    # adjust for fact that TCP doesn't guarantee size of reads/writes
	    arm(5);
	    $read = sysread(T,$buff,10,length($buff));
	    arm(0);
	}
	print(($read == 0 || $buff eq "hello") ? "ok 3\n" : "not ok 3\n");
  }
  else {
	print "# You're allowed to fail tests 2 and 3 if\n";
	print "# the echo service has been disabled or if your\n";
        print "# gethostbyname() cannot resolve your localhost.\n";
	print "# 'Connection refused' indicates disabled echo service.\n";
	print "# 'Interrupted system call' indicates a hanging echo service.\n";
	print "# Error: $!\n";
	print "ok 2 - skipped\n";
	print "ok 3 - skipped\n";
  }
}
else {
	print "# Error: $!\n";
	print "not ok 1\n";
}

if( socket(S,PF_INET,SOCK_STREAM,6) ){
  print "ok 4\n";

  arm(5);
  if ($has_echo && connect(S,pack_sockaddr_in(7,INADDR_LOOPBACK))){
        arm(0);

	print "ok 5\n";

	print "# Connected to " .
		inet_ntoa((unpack_sockaddr_in(getpeername(S)))[1])."\n";

	arm(5);
	syswrite(S,"olleh",5);
	arm(0);

	arm(5);
	$read = sysread(S,$buff,10);	# Connection may be granted, then closed!
	arm(0);

	while ($read > 0 && length($buff) < 5) {
	    # adjust for fact that TCP doesn't guarantee size of reads/writes
	    arm(5);
	    $read = sysread(S,$buff,10,length($buff));
	    arm(0);
	}
	print(($read == 0 || $buff eq "olleh") ? "ok 6\n" : "not ok 6\n");
  }
  else {
	print "# You're allowed to fail tests 5 and 6 if\n";
	print "# the echo service has been disabled.\n";
	print "# 'Interrupted system call' indicates a hanging echo service.\n";
	print "# Error: $!\n";
	print "ok 5 - skipped\n";
	print "ok 6 - skipped\n";
  }
}
else {
	print "# Error: $!\n";
	print "not ok 4\n";
}

# warnings
$SIG{__WARN__} = sub {
    ++ $w if $_[0] =~ /^6-ARG sockaddr_in call is deprecated/ ;
} ;
$w = 0 ;
sockaddr_in(1,2,3,4,5,6) ;
print ($w == 1 ? "not ok 7\n" : "ok 7\n") ;
use warnings 'Socket' ;
sockaddr_in(1,2,3,4,5,6) ;
print ($w == 1 ? "ok 8\n" : "not ok 8\n") ;

# Thest that whatever we give into pack/unpack_sockaddr retains
# the value thru the entire chain.
if((inet_ntoa((unpack_sockaddr_in(pack_sockaddr_in(100,inet_aton("10.250.230.10"))))[1])) eq '10.250.230.10') {
    print "ok 9\n"; 
} else {
    print "not ok 9\n"; 
}
print ((inet_ntoa(inet_aton("10.20.30.40")) eq "10.20.30.40") ? "ok 10\n" : "not ok 10\n");
print ((inet_ntoa(v10.20.30.40) eq "10.20.30.40") ? "ok 11\n" : "not ok 11\n");
{
    my ($port,$addr) = unpack_sockaddr_in(pack_sockaddr_in(100,v10.10.10.10));
    print (($port == 100) ? "ok 12\n" : "not ok 12\n");
    print ((inet_ntoa($addr) eq "10.10.10.10") ? "ok 13\n" : "not ok 13\n");
}
				     
eval { inet_ntoa(v10.20.30.400) };
print (($@ =~ /^Wide character in Socket::inet_ntoa at/) ? "ok 14\n" : "not ok 14\n");

if (sockaddr_family(pack_sockaddr_in(100,inet_aton("10.250.230.10"))) == AF_INET) {
    print "ok 15\n";
} else {
    print "not ok 15\n";
}

eval { sockaddr_family("") };
print (($@ =~ /^Bad arg length for Socket::sockaddr_family, length is 0, should be at least \d+/) ? "ok 16\n" : "not ok 16\n");

if ($^O eq 'linux') {
    # see if we can handle abstract sockets
    my $test_abstract_socket = chr(0) . '/tmp/test-perl-socket';
    my $addr = sockaddr_un ($test_abstract_socket);
    my ($path) = sockaddr_un ($addr);
    if ($test_abstract_socket eq $path) {
        print "ok 17\n";
    }
    else {
	$path =~ s/\0/\\0/g;
	print "# got <$path>\n";
        print "not ok 17\n";
    }
} else {
    # doesn't have abstract socket support
    print "ok 17 - skipped on this platform\n";
}
