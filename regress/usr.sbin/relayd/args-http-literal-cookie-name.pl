# test that default cookie name matching is literal (no glob(7)) and
# case-insensitive

use strict;
use warnings;

our %args = (
    client => {
	func => \&http_client,
	cookie => "mycookie=bar",
    },
    relayd => {
	protocol => [ "http",
	    'match request cookie "my*" tag COOKIELITERAL',
	    'match request cookie "MYCOOKIE" tag COOKIENAME',
	],
	loggrep => {
	    qr/, COOKIELITERAL,/ => 0,
	    qr/, COOKIENAME,.*done/ => 1,
	},
    },
    server => {
	func => \&http_server,
    },
);

1;
