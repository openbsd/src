# test that "url log pattern" matches the path with case-sensitive
# patterns(7)

use strict;
use warnings;

our %args = (
    client => {
	func => \&http_client,
	path => "FOO",
    },
    relayd => {
	protocol => [ "http",
	    'match request url log pattern "^/f" tag URLLOGNOMATCH',
	    'match request url log pattern "^/F" tag URLLOGPATTERN',
	],
	loggrep => {
	    qr/, URLLOGNOMATCH,/ => 0,
	    qr/, URLLOGPATTERN,.*done/ => 1,
	},
    },
    server => {
	func => \&http_server,
    },
    len => 4,
);

1;
