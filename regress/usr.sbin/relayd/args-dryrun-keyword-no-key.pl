# test that a matching keyword without a key or a file is a config error

use strict;
use warnings;

our %args = (
    client => {
	noclient => 1,
    },
    relayd => {
	protocol => [ "http",
	    'match request header glob ignorecase tag NOKEY',
	],
	dryrun => qr/matching keyword needs a key or a file/,
    },
    server => {
	noserver => 1,
    },
    nocheck => 1,
);

1;
