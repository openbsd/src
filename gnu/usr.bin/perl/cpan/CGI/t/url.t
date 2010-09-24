use strict;
use warnings;

use Test::More tests => 4;    # last test to print

use CGI qw/ :all /;

$ENV{HTTP_X_FORWARDED_HOST} = 'proxy:8484';
$ENV{SERVER_PROTOCOL}       = 'HTTP/1.0';
$ENV{SERVER_PORT}           = 8080;
$ENV{SERVER_NAME}           = 'the.good.ship.lollypop.com';

is virtual_port() => 8484, 'virtual_port()';
is server_port()  => 8080, 'server_port()';

is url() => 'http://proxy:8484', 'url()';

# let's see if we do the defaults right

$ENV{HTTP_X_FORWARDED_HOST} = 'proxy:80';

is url() => 'http://proxy', 'url() with default port';

