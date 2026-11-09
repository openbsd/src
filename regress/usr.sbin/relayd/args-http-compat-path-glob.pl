# test that a path with glob(7) characters and no matching keyword
# is a case-sensitive glob(7), as before the glob keywords

use strict;
use warnings;

our %args = (
    client => {
	func => \&http_client,
	path => "FOO/bar",
    },
    relayd => {
	protocol => [ "http",
	    'match request path "/f*" tag PATHLOWER',
	    'match request path "/F?O/*" tag PATHUPPER',
	],
	loggrep => {
	    qr/, PATHLOWER,/ => 0,
	    qr/, PATHUPPER,.*done/ => 1,
	},
    },
    server => {
	func => \&http_server,
    },
    len => 8,
);

1;
