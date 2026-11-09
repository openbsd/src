# test that "url log" keys default to a case-insensitive glob(7) on the
# path, and "glob" makes them case-sensitive

use strict;
use warnings;

our %args = (
    client => {
	func => \&http_client,
	path => "FOO",
    },
    relayd => {
	protocol => [ "http",
	    'match request url log glob "/f*" tag URLLOGGLOB',
	    'match request url log "/f*" tag URLLOGDEFAULT',
	],
	loggrep => {
	    qr/, URLLOGGLOB,/ => 0,
	    qr/, URLLOGDEFAULT,.*done/ => 1,
	},
    },
    server => {
	func => \&http_server,
    },
    len => 4,
);

1;
