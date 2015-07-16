use strict;
use warnings;

our %args = (
    client => {
	func => sub {
	    my $self = shift;
	    print "GET /\r\n\r\n";
	},
        nocheck => 1
    },
    httpd => {
	loggrep => {
	    qr/"GET \/" 500 0/ => 1,
	},
    },
);

1;

