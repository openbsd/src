# test that "set" with a matching keyword on keys from a "file" is a
# config error

use strict;
use warnings;

our %args = (
    client => {
	noclient => 1,
    },
    relayd => {
	protocol => [ "http",
	    'match request header set glob file ' .
		'"$curdir/args-http-file-value-keyword.in" value "y"',
	],
	dryrun => qr/glob not allowed with set/,
    },
    server => {
	noserver => 1,
    },
    nocheck => 1,
);

1;
