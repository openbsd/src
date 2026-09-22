# test that default glob(7) path matching is case-sensitive

use strict;
use warnings;

our %args = (
    client => {
	func => \&http_client,
	path => "FOO",
    },
    relayd => {
	protocol => [ "http",
	    'match request path "/foo" tag PATHLOWER',
	    'match request path "/FOO" tag PATHUPPER',
	],
	loggrep => {
	    qr/, PATHLOWER,/ => 0,
	    qr/, PATHUPPER,.*done/ => 1,
	},
    },
    server => {
	func => \&http_server,
    },
    len => 4,
);

1;
