# test that "pattern" enables patterns(7) matching for url lookup

use strict;
use warnings;

my %header_client = (
	Host => "example.com",
);

our %args = (
    client => {
	func => \&http_client,
	path => "FOO",
	header => \%header_client,
    },
    relayd => {
	protocol => [ "http",
	    'match request url "^example%.com/%a+$" tag URLNOMATCH',
	    'match request url pattern "^example%.com/%a+$" tag URLPATTERN',
	],
	loggrep => {
	    qr/, URLNOMATCH,/ => 0,
	    qr/, URLPATTERN,.*done/ => 1,
	},
    },
    server => {
	func => \&http_server,
    },
    len => 4,
);

1;
