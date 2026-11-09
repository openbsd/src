# test that "glob" makes cookie name matching a case-sensitive glob(7)

use strict;
use warnings;

our %args = (
    client => {
	func => \&http_client,
	cookie => "mycookie=bar",
    },
    relayd => {
	protocol => [ "http",
	    'match request cookie glob "MY*" tag COOKIEUPPER',
	    'match request cookie glob "my*" tag COOKIELOWER',
	],
	loggrep => {
	    qr/, COOKIEUPPER,/ => 0,
	    qr/, COOKIELOWER,.*done/ => 1,
	},
    },
    server => {
	func => \&http_server,
    },
);

1;
