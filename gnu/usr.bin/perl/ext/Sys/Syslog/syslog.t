#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require Config; import Config;
    if ($Config{'extensions'} !~ /\bSyslog\b/) {
	print "1..0 # Skip: Sys::Syslog was not built\n";
	exit 0;
    }
    if ($Config{'extensions'} !~ /\bSocket\b/) {
	print "1..0 # Skip: Socket was not built\n";
	exit 0;
    }

    require Socket;

    # This code inspired by Sys::Syslog::connect():
    require Sys::Hostname;
    my ($host_uniq) = Sys::Hostname::hostname();
    my ($host)      = $host_uniq =~ /([A-Za-z0-9_.-]+)/;

    if (! defined Socket::inet_aton($host)) {
        print "1..0 # Skip: Can't lookup $host\n";
        exit 0;
    }
}

BEGIN {
  eval {require Sys::Syslog} or do {
    if ($@ =~ /Your vendor has not/) {
      print "1..0 # Skip: missing macros\n";
      exit 0;
    }
  }
}

use Sys::Syslog qw(:DEFAULT setlogsock);

# Test this to 1 if your syslog accepts udp connections.
# Most don't (or at least shouldn't)
my $Test_Syslog_INET = 0;

my $test_string = "uid $< is testing perl $] syslog capabilities";

print "1..6\n";

if (Sys::Syslog::_PATH_LOG()) {
    if (-e Sys::Syslog::_PATH_LOG()) {
        print defined(eval { setlogsock('unix') }) ? "ok 1\n" : "not ok 1 # $!\n";
        print defined(eval { openlog('perl', 'ndelay', 'local0') }) ? "ok 2\n" : "not ok 2 # $!\n";
        print defined(eval { syslog('info', $test_string ) }) ? "ok 3\n" : "not ok 3 # $!\n";
    }
    else {
        for (1..3) {
            print
                "ok $_ # Skip: file ",
                Sys::Syslog::_PATH_LOG(),
                " does not exist\n";
        }
    }
}
else {
    for (1..3) { print "ok $_ # Skip: _PATH_LOG unavailable\n" }
}

if( $Test_Syslog_INET ) {
    print defined(eval { setlogsock('inet') }) ? "ok 4\n" 
                                               : "not ok 4\n";
    print defined(eval { openlog('perl', 'ndelay', 'local0') }) ? "ok 5\n" 
                                                                : "not ok 5 # $!\n";
    print defined(eval { syslog('info', $test_string ) }) ? "ok 6\n" 
                                                   : "not ok 6 # $!\n";
}
else {
    print "ok $_ # Skip: assuming syslog doesn't accept inet connections\n" 
      foreach (4..6);
}
