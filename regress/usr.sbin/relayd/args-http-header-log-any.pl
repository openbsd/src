# test that "header log" without a key matches any header, the implicit
# "*" key is a case-insensitive glob(7)

use strict;
use warnings;

our %args = (
    client => {
	func => \&http_client,
    },
    relayd => {
	protocol => [ "http",
	    'match request header log tag HDRLOGANY',
	],
	loggrep => { qr/, HDRLOGANY,.*done/ => 1 },
    },
    server => {
	func => \&http_server,
    },
);

1;
