# test that an unterminated chunk-size line is not buffered without limit

use strict;
use warnings;

our %args = (
    client => {
	func => sub {
	    my $self = shift;
	    errignore();
	    print "POST /1 HTTP/1.1\r\nHost: foo\r\n",
	    "Transfer-Encoding: chunked\r\n\r\n";
	    print "1;", "x" x 65536;	# no CRLF
	    IO::Handle::flush(\*STDOUT);
	    read_char($self);
	},
	nocheck => 1,
    },
    relayd => {
	protocol => [ "http", 'pass' ],
	loggrep => { qr/chunk size too long/ => 1 },
    },
    server => {
	func => sub { errignore(); read_char(shift) },
	nocheck => 1,
    },
);

1;
