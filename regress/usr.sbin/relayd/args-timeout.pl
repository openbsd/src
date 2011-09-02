# test that 3 seconds timeout occurs within 4 seconds idle

use strict;
use warnings;

our %args = (
    client => {
	func => sub {
	    errignore();
	    write_char(@_, 5);
	    sleep 4;
	    write_char(@_, 4);
	},
	sleep => 1,
	down => "Broken pipe",
	nocheck => 1,
    },
    relayd => {
	relay => [ "session timeout 3" ],
    },
    len => 5,
);

1;
