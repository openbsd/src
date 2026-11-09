# test that "path set" accepts glob and glob ignorecase, the path key
# is still matched against the request path

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
	    'match request path set glob "/OLD/*" value "/wrong"',
	    'match request path set glob ignorecase "/OLD/*" value "/new"',
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
