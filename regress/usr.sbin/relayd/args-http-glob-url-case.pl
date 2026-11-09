# test that "glob" makes url lookup a case-sensitive glob(7),
# the host part is always lowercased by relayd

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
	    'match request url glob "example.com/f*" tag URLLOWER',
	    'match request url glob "example.com/F*" tag URLUPPER',
	],
	loggrep => {
	    qr/, URLLOWER,/ => 0,
	    qr/, URLUPPER,.*done/ => 1,
	},
    },
    server => {
	func => \&http_server,
    },
    len => 4,
);

1;
