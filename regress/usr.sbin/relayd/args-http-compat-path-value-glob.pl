# test that a path rule value (query string) with glob(7) characters and
# no matching keyword is a case-insensitive glob(7), as before the
# glob keywords

use strict;
use warnings;

our %args = (
    client => {
	func => \&http_client,
	path => "foo?A=B",
    },
    relayd => {
	protocol => [ "http",
	    'match request path "/foo" value "x=*" tag PATHVALNOMATCH',
	    'match request path "/foo" value "a=*" tag PATHVALGLOB',
	],
	loggrep => {
	    qr/, PATHVALNOMATCH,/ => 0,
	    qr/, PATHVALGLOB,.*done/ => 1,
	},
    },
    server => {
	func => \&http_server,
    },
    len => 8,
);

1;
