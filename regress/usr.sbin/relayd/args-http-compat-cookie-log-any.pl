# test that "cookie log" without a key does not match any cookie, the
# implicit "*" key is compared literally, as before the glob keywords

use strict;
use warnings;

our %args = (
    client => {
	func => \&http_client,
	cookie => "mycookie=bar",
    },
    relayd => {
	protocol => [ "http",
	    'match request cookie log tag COOKIELOG',
	],
	loggrep => { qr/, COOKIELOG,/ => 0 },
    },
    server => {
	func => \&http_server,
    },
);

1;
