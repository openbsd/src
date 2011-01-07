# test mix write and relay delaying before server read

use strict;
use warnings;

our %args = (
    client => {
	func => sub { sleep 4; write_char(@_); },
	len => 65521,
	nocheck => 1,
    },
    relay => {
	func => sub {
	    write_char(@_, 32749);
	    IO::Handle::flush(\*STDOUT);
	    relay(@_);
	    write_char(@_, 2039);
	},
	nocheck => 1,
    },
    server => {
	func => sub { sleep 3; read_char(@_); },
    },
    len => 100309,
    md5 => "0efc7833e5c0652823ca63eaccb9918f",
);

1;
