# test that "file" without a preceding lookup type is a config error

use strict;
use warnings;

our %args = (
    client => {
	noclient => 1,
    },
    relayd => {
	protocol => [ "http",
	    'match request file ' .
		'"$curdir/args-http-compat-header-file.in" tag NOTYPE',
	],
	dryrun => qr/file needs a preceding cookie, header, path, query or url/,
    },
    server => {
	noserver => 1,
    },
    nocheck => 1,
);

1;
