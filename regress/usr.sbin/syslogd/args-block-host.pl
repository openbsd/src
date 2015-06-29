# The client writes a message to Sys::Syslog native method.
# The syslogd writes into multiple files depending on hostname.
# The syslogd writes it into a file and through a pipe.
# The syslogd passes it via UDP to the loghost.
# The server receives the message on its UDP socket.
# Find the message in client, file, pipe, syslogd, server log.
# Check that the message appears in the correct log files.

use strict;
use warnings;
use Cwd;
use Sys::Hostname;

my $objdir = getcwd();
(my $hostname = hostname()) =~ s/\..*//;

our %args = (
    syslogd => {
	conf => <<"EOF",
+nonexist
*.*	$objdir/file-0.log
+$hostname
*.*	$objdir/file-1.log
*.*	$objdir/file-2.log
+*
*.*	$objdir/file-3.log
EOF
    },
    multifile => [
	{ loggrep => { get_testlog() => 0 } },
	{ loggrep => { get_testlog() => 1 } },
	{ loggrep => { get_testlog() => 1 } },
	{ loggrep => { get_testlog() => 1 } },
    ],
);

1;
