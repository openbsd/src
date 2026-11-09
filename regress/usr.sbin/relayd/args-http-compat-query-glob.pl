# test that query name and value with glob(7) characters and no matching
# keyword are a case-sensitive glob(7), as before the glob keywords

use strict;
use warnings;

our %args = (
    client => {
	func => \&http_client,
	path => "search?KEY=VALUE",
    },
    relayd => {
	protocol => [ "http",
	    'match request query "k*" value "v*" tag QUERYLOWER',
	    'match request query "K?Y" value "V*E" tag QUERYUPPER',
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
