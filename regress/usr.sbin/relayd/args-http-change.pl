use strict;
use warnings;

my %header = ("X-Test-Header" => "XOriginalValue");
our %args = (
    client => {
	func => \&http_client,
	nocheck => 1,
	loggrep => {
		qr/X-Test-Header: XChangedValue/ => 1,
		qr/Host: foo.bar/ => 1,
	}
    },
    relayd => {
	protocol => [ "http",
	    'request header change "Host" to "foobar.changed"',
	    'response header change "X-Test-Header" to "XChangedValue"',
	],
    },
    server => {
	func => \&http_server,
	header => \%header,
	loggrep => {
		qr/X-Test-Header: XOriginalValue/ => 1,
		qr/Host: foobar.changed/ => 1,
	},
    },
);

1;
