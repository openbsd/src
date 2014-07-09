use strict;
use warnings;

my @lengths = (1, 2, 4, 0, 3, 5);
our %args = (
    client => {
	func => \&http_client,
	lengths => \@lengths,
	loggrep => {
		qr/403 Forbidden/ => 2,
		qr/Server: OpenBSD relayd/ => 2,
		qr/Connection: close/ => 2,
		qr/Content-Length\: 3/ => 0,
		qr/Content-Length\: 4/ => 1,
	},
	mreqs => 1,
	httpnok => 1,
    },
    relayd => {
	protocol => [ "http",
	    'return error',
	    'pass',
	    'block request url log file "$curdir/args-http-filter-url-file.in" value "*" label "test_reject_label"',
	],
	loggrep => {
		qr/Forbidden/ => 2,
		qr/\[test_reject_label\, foo\.bar\/0\]/ => 1,
		qr/\[test_reject_label\, foo\.bar\/3\]/ => 1,
	},
    },
    server => {
	func => \&http_server,
	lengths => (1, 2, 4),
	mreqs => 4,
	nocheck => 1,
    },
);

1;
