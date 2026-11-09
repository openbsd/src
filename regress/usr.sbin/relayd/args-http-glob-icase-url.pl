# test that "glob ignorecase" makes url lookup a case-insensitive glob(7)

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
	    'match request url glob "example.com/f*" tag URLNOMATCH',
	    'match request url glob ignorecase "example.com/f*" tag URLICASE',
	],
	loggrep => {
	    qr/, URLNOMATCH,/ => 0,
	    qr/, URLICASE,.*done/ => 1,
	},
    },
    server => {
	func => \&http_server,
    },
    len => 4,
);

1;
