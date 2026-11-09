# test that default url lookup is literal (no glob(7)) and case-insensitive

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
	    'match request url "example.com/f*" tag URLLITERAL',
	    'match request url "example.com/foo" tag URLNAME',
	],
	loggrep => {
	    qr/, URLLITERAL,/ => 0,
	    qr/, URLNAME,.*done/ => 1,
	},
    },
    server => {
	func => \&http_server,
    },
    len => 4,
);

1;
