# test that default glob(7) url lookup is case-insensitive

use strict;
use warnings;

my %header_client = (
	Host => "EXAMPLE.COM",
);

our %args = (
    client => {
	func => \&http_client,
	path => "FOO",
	header => \%header_client,
    },
    relayd => {
	protocol => [ "http",
	    'match request url "example.com/foo" tag URLGLOB',
	],
	loggrep => { qr/, URLGLOB,.*done/ => 1 },
    },
    server => {
	func => \&http_server,
    },
    len => 4,
);

1;
