# test that header names read from a file are compared literally, even
# with glob(7) characters, as before the glob keywords

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
	    'match request header file ' .
		'"$curdir/args-http-compat-header-file-glob.in" ' .
		'tag HDRFILEGLOB',
	    'match request header file ' .
		'"$curdir/args-http-compat-header-file.in" tag HDRFILE',
	],
	loggrep => {
	    qr/, HDRFILEGLOB,/ => 0,
	    qr/, HDRFILE,.*done/ => 1,
	},
    },
    server => {
	func => \&http_server,
    },
);

1;
