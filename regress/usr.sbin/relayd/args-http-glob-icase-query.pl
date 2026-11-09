# test that "glob ignorecase" makes query name/value matching case-insensitive

use strict;
use warnings;

our %args = (
    client => {
	func => \&http_client,
	path => "search?KEY=VALUE",
    },
    relayd => {
	protocol => [ "http",
	    'match request query "key" value "value" tag QUERYNOMATCH',
	    'match request query glob ignorecase "key" ' .
		'value glob ignorecase "value" tag QUERYICASE',
	],
	loggrep => {
	    qr/, QUERYNOMATCH,/ => 0,
	    qr/, QUERYICASE,.*done/ => 1,
	},
    },
    server => {
	func => \&http_server,
    },
    len => 17,
);

1;
