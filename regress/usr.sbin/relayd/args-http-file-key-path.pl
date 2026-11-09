# test that a matching keyword before "file" applies to the paths
# read from it

use strict;
use warnings;

our %args = (
    client => {
	func => \&http_client,
	path => "FOO",
    },
    relayd => {
	protocol => [ "http",
	    'match request path glob file ' .
		'"$curdir/args-http-compat-path-file-lower.in" ' .
		'tag PATHFILEGLOB',
	    'match request path glob ignorecase file ' .
		'"$curdir/args-http-compat-path-file-lower.in" ' .
		'tag PATHFILEICASE',
	],
	loggrep => {
	    qr/, PATHFILEGLOB,/ => 0,
	    qr/, PATHFILEICASE,.*done/ => 1,
	},
    },
    server => {
	func => \&http_server,
    },
    len => 4,
);

1;
