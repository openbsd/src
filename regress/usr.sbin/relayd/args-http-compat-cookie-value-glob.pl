# test that a cookie value with glob(7) characters and no matching keyword
# is a case-insensitive glob(7), as before the glob keywords

use strict;
use warnings;

our %args = (
    client => {
	func => \&http_client,
	cookie => "mycookie=BAR",
    },
    relayd => {
	protocol => [ "http",
	    'match request cookie "mycookie" value "x*" tag COOKIENOMATCH',
	    'match request cookie "mycookie" value "b?r*" tag COOKIEGLOB',
	],
	loggrep => {
	    qr/, COOKIENOMATCH,/ => 0,
	    qr/, COOKIEGLOB,.*done/ => 1,
	},
    },
    server => {
	func => \&http_server,
    },
);

1;
