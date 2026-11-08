# test that "path glob" without a key or a file is a config error,
# although glob is also the default of path keys

use strict;
use warnings;

our %args = (
    client => {
	noclient => 1,
    },
    relayd => {
	protocol => [ "http",
	    'block request quick path glob',
	],
	dryrun => qr/matching keyword needs a key or a file/,
    },
    server => {
	noserver => 1,
    },
    nocheck => 1,
);

1;
