use strict;
use warnings;

our %args = (
    client => {
	noclient => 1,
	nocheck => 1,
    },
    relayd => {
	protocol => [ "http",
	    'request path change "path" to "foobarchangedpath" marked 55',
	],
	loggrep => { 
		qr/relayd.conf\:.*action only supported for headers/ => 1
	},
	dummyrun => 1,
    },
    server => {
	noserver => 1,
	nocheck => 1,
    },
);

1;
