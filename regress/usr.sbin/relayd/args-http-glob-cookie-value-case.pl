# test that "value glob" makes cookie value matching case-sensitive

use strict;
use warnings;

our %args = (
    client => {
	func => \&http_client,
	cookie => "mycookie=BAR",
    },
    relayd => {
	protocol => [ "http",
	    'match request cookie "mycookie" value glob "bar" tag COOKIELOWER',
	    'match request cookie "mycookie" value glob "B?R" tag COOKIEUPPER',
	],
	loggrep => {
	    qr/, COOKIELOWER,/ => 0,
	    qr/, COOKIEUPPER,.*done/ => 1,
	},
    },
    server => {
	func => \&http_server,
    },
);

1;
