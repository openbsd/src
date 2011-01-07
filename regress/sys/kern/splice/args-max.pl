# test maximum data length

use strict;
use warnings;

our %args = (
    client => {
	func => sub { errignore(@_); write_char(@_); },
	len => 2**17,
	nocheck => 1,
    },
    relay => {
	max => 32117,
    },
    len => 32117,
    md5 => "ee338e9693fb2a2ec101bb28935ed123",
);

1;
