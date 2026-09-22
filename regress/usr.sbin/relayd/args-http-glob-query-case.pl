# test that default glob(7) query name/value matching is case-sensitive

use strict;
use warnings;

our %args = (
    client => {
	func => \&http_client,
	path => "search?KEY=VALUE",
    },
    relayd => {
	protocol => [ "http",
	    'match request query "key" value "value" tag QUERYLOWER',
	    'match request query "KEY" value "VALUE" tag QUERYUPPER',
	],
	loggrep => {
	    qr/, QUERYLOWER,/ => 0,
	    qr/, QUERYUPPER,.*done/ => 1,
	},
    },
    server => {
	func => \&http_server,
    },
    len => 17,
);

1;
