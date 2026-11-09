# test that a matching keyword on the value of a "file" rule is used,
# without one the per-type default applies

use strict;
use warnings;

my %header_client = (
	"X-Test" => "HELLO",
);

our %args = (
    client => {
	func => \&http_client,
	header => \%header_client,
    },
    relayd => {
	protocol => [ "http",
	    'match request header file ' .
		'"$curdir/args-http-file-value-keyword.in" ' .
		'value "hello" tag FILEDEFAULT',
	    'match request header file ' .
		'"$curdir/args-http-file-value-keyword.in" ' .
		'value pattern "^hel" tag FILEPATTERN',
	    'match request header file ' .
		'"$curdir/args-http-file-value-keyword.in" ' .
		'value glob ignorecase "hel*" tag FILEICASE',
	],
	loggrep => {
	    qr/, FILEDEFAULT,/ => 0,
	    qr/, FILEPATTERN,/ => 0,
	    qr/, FILEICASE,.*done/ => 1,
	},
    },
    server => {
	func => \&http_server,
    },
);

1;
