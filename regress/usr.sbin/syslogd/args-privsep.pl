# The client writes a message to Sys::Syslog native method.
# The syslogd writes it into a file and through a pipe.
# The syslogd passes it via UDP to the loghost.
# The server receives the message on its UDP socket.
# Find the message in client, file, pipe, syslogd, server log.
# Check fstat for the parent and child process.
# Check ktrace for setting the correct uid and gid.

use strict;
use warnings;

our %args = (
    syslogd => {
	loggrep => {
	    qr/ -F / => 0,
	    qr/ -d / => 1,
	},
	fstat => {
	    qr/^root .* wd / => 1,
	    qr/^root .* root / => 0,
	    qr/^root .* kqueue / => 0,
	    qr/^root .* internet/ => 0,
	    qr/^_syslogd .* wd / => 1,
	    qr/^_syslogd .* root / => 1,
	    qr/^_syslogd .* kqueue / => 1,
	    qr/^_syslogd .* internet/ => 2,
	},
	ktrace => {
	    qr/CALL  setresuid(.*"_syslogd".*){3}/ => 2,
	    qr/CALL  setresgid(.*"_syslogd".*){3}/ => 2,
	    qr/CALL  setsid/ => 0,
	},
    },
);

1;
