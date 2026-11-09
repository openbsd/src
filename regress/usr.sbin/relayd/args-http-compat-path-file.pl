# test that paths read from a file are a case-sensitive glob(7),
# as before the glob keywords

use strict;
use warnings;

our %args = (
    client => {
	func => \&http_client,
	path => "FOO",
    },
    relayd => {
	protocol => [ "http",
	    'match request path file ' .
		'"$curdir/args-http-compat-path-file-lower.in" ' .
		'tag PATHFILELOWER',
	    'match request path file ' .
		'"$curdir/args-http-compat-path-file.in" tag PATHFILE',
	],
	loggrep => {
	    qr/, PATHFILELOWER,/ => 0,
	    qr/, PATHFILE,.*done/ => 1,
	},
    },
    server => {
	func => \&http_server,
    },
    len => 4,
);

1;
