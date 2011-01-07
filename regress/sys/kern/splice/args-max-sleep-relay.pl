# test maximum data length with delay before relay copy

use strict;
use warnings;

our %args = (
    client => {
	func => sub { errignore(@_); write_char(@_); },
	len => 2**17,
	down => "Client print failed: Broken pipe",
	nocheck => 1,
    },
    relay => {
	func => sub { sleep 3; relay(@_); shutin(@_); sleep 1; },
	max => 32117,
    },
    len => 32117,
    md5 => "ee338e9693fb2a2ec101bb28935ed123",
);

1;
