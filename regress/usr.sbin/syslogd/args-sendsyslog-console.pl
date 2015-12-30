# The client kills syslogd.
# The client writes a message with sendsyslog2 LOG_CONS flag.
# Find the message in console log.
# Create a ktrace dump of the client and check for sendsyslog2.

use strict;
use warnings;
use Errno ':POSIX';
use Sys::Syslog 'LOG_CONS';

my $errno = ENOTCONN;

our %args = (
    client => {
	func => sub {
	    my $self = shift;
	    ${$self->{syslogd}}->kill_syslogd('TERM');
	    ${$self->{syslogd}}->down();
	    sendsyslog2(get_testlog(), LOG_CONS)
		and die ref($self), " sendsyslog2 succeeded";
	},
	ktrace => {
	    qr/CALL  sendsyslog2\(/ => 1,
	    qr/RET   sendsyslog2 -1 errno $errno / => 1,
	},
	loggrep => {},
    },
    syslogd => { loggrep => {} },
    server => { noserver => 1 },
    file => { nocheck => 1 },
    pipe => { nocheck => 1 },
    user => { nocheck => 1 },
);

1;
