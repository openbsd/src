# test that "set" with a matching keyword is a config error

use strict;
use warnings;

our %args = (
    client => {
	noclient => 1,
    },
    relayd => {
	protocol => [ "http",
	    'match request header set "X-Test" value glob "y"',
	],
	dryrun => qr/glob not allowed with set/,
    },
    server => {
	noserver => 1,
    },
    nocheck => 1,
);

1;
