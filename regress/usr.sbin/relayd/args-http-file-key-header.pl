# test that a matching keyword before "file" applies to the header
# names read from it

use strict;
use warnings;

my %header_client = (
	"X-Test" => "ok",
);

our %args = (
    client => {
	func => \&http_client,
	header => \%header_client,
    },
    relayd => {
	protocol => [ "http",
	    'match request header pattern file ' .
		'"$curdir/args-http-file-key-header-pattern.in" ' .
		'tag HDRFILEPATTERN',
	    'match request header glob ignorecase file ' .
		'"$curdir/args-http-compat-header-file-glob.in" ' .
		'tag HDRFILEICASE',
	],
	loggrep => {
	    qr/, HDRFILEPATTERN,/ => 0,
	    qr/, HDRFILEICASE,.*done/ => 1,
	},
    },
    server => {
	func => \&http_server,
    },
);

1;
