# test that "glob ignorecase" makes cookie name matching a
# case-insensitive glob(7)

use strict;
use warnings;

our %args = (
    client => {
	func => \&http_client,
	cookie => "mycookie=bar",
    },
    relayd => {
	protocol => [ "http",
	    'match request cookie pattern "^MY" tag COOKIENOMATCH',
	    'match request cookie glob ignorecase "MY*" tag COOKIEICASE',
	],
	loggrep => {
	    qr/, COOKIENOMATCH,/ => 0,
	    qr/, COOKIEICASE,.*done/ => 1,
	},
    },
    server => {
	func => \&http_server,
    },
);

1;
