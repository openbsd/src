use strict;
use warnings;

our %args = (
    client => {
	noclient => 1,
	nocheck => 1,
    },
    relayd => {
	protocol => [ "http",
	    'request path mark "*" with 99 marked 55',
	],
	loggrep => { "either mark or marked" => 1 },
	dummyrun => 1,
    },
    server => {
	noserver => 1,
	nocheck => 1,
    },
);

1;
