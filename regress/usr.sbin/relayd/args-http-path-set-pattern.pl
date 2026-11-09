# test that "path set pattern" only rewrites a path matching the
# case-sensitive patterns(7) key

use strict;
use warnings;

our %args = (
    client => {
	func => \&http_client,
	path => "old/foo",
	loggrep => {
		qr/GET \/old\/foo HTTP\/1\.0/ => 1,
	},
    },
    relayd => {
	protocol => [ "http",
	    'match request path set pattern "^/OLD/" value "/wrong"',
	    'match request path set pattern "^/old/" value "/new"',
	],
    },
    server => {
	func => \&http_server,
	loggrep => {
		qr/GET \/wrong HTTP\/1\.0/ => 0,
		qr/GET \/new HTTP\/1\.0/ => 1,
	},
    },
    len => 4,
);

1;
