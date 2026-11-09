# test that a matching keyword before "file" applies to the cookie
# names read from it, without one they stay literal

use strict;
use warnings;

our %args = (
    client => {
	func => \&http_client,
	cookie => "mycookie=bar",
    },
    relayd => {
	protocol => [ "http",
	    'match request cookie file ' .
		'"$curdir/args-http-file-key-cookie.in" ' .
		'tag COOKIEFILE',
	    'match request cookie glob file ' .
		'"$curdir/args-http-file-key-cookie.in" ' .
		'tag COOKIEFILEGLOB',
	],
	loggrep => {
	    qr/, COOKIEFILE,/ => 0,
	    qr/, COOKIEFILEGLOB,.*done/ => 1,
	},
    },
    server => {
	func => \&http_server,
    },
);

1;
