use strict;
use warnings;

use Test::More;

use CGI ':all';


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

subtest 'rewrite_interactions' => sub {
    # Reference: RT#45019

    local $ENV{HTTP_X_FORWARDED_HOST} = undef;
    local $ENV{SERVER_PROTOCOL}       = undef;
    local $ENV{SERVER_PORT}           = undef;
    local $ENV{SERVER_NAME}           = undef;

    # These two are always set
    local $ENV{'SCRIPT_NAME'}     = '/real/cgi-bin/dispatch.cgi';
    local $ENV{'SCRIPT_FILENAME'} = '/home/mark/real/path/cgi-bin/dispatch.cgi';

    # These two are added by mod_rewrite Ref: http://httpd.apache.org/docs/2.2/mod/mod_rewrite.html

    local $ENV{'SCRIPT_URL'}      = '/real/path/info';
    local $ENV{'SCRIPT_URI'}      = 'http://example.com/real/path/info';

    local $ENV{'PATH_INFO'}       = '/path/info';
    local $ENV{'REQUEST_URI'}     = '/real/path/info';
    local $ENV{'HTTP_HOST'}       = 'example.com';

    my $q = CGI->new;

    is(
        $q->url( -absolute => 1, -query => 1, -path_info => 1 ),
        '/real/path/info',
        '$q->url( -absolute => 1, -query => 1, -path_info => 1 ) should return complete path, even when mod_rewrite is detected.'
    );
    is( $q->url(), 'http://example.com/real', '$q->url(), with rewriting detected' );
    is( $q->url(-full=>1), 'http://example.com/real', '$q->url(-full=>1), with rewriting detected' );
    is( $q->url(-path=>1), 'http://example.com/real/path/info', '$q->url(-path=>1), with rewriting detected' );
    is( $q->url(-path=>0), 'http://example.com/real', '$q->url(-path=>0), with rewriting detected' );
    is( $q->url(-full=>1,-path=>1), 'http://example.com/real/path/info', '$q->url(-full=>1,-path=>1), with rewriting detected' );
    is( $q->url(-rewrite=>1,-path=>0), 'http://example.com/real', '$q->url(-rewrite=>1,-path=>0), with rewriting detected' );
    is( $q->url(-rewrite=>1), 'http://example.com/real',
                                                '$q->url(-rewrite=>1), with rewriting detected' );
    is( $q->url(-rewrite=>0), 'http://example.com/real/cgi-bin/dispatch.cgi',
                                                '$q->url(-rewrite=>0), with rewriting detected' );
    is( $q->url(-rewrite=>0,-path=>1), 'http://example.com/real/cgi-bin/dispatch.cgi/path/info',
                                                '$q->url(-rewrite=>0,-path=>1), with rewriting detected' );
    is( $q->url(-rewrite=>1,-path=>1), 'http://example.com/real/path/info',
                                                '$q->url(-rewrite=>1,-path=>1), with rewriting detected' );
    is( $q->url(-rewrite=>0,-path=>0), 'http://example.com/real/cgi-bin/dispatch.cgi',
                                                '$q->url(-rewrite=>0,-path=>1), with rewriting detected' );
};

subtest 'RT#58377: + in PATH_INFO' => sub {
    local $ENV{PATH_INFO}             = '/hello+world';
    local $ENV{HTTP_X_FORWARDED_HOST} = undef;
    local $ENV{'HTTP_HOST'}           = 'example.com';
    local $ENV{'SCRIPT_NAME'}         = '/script/plus+name.cgi';
    local $ENV{'SCRIPT_FILENAME'}     = '/script/plus+filename.cgi';

    my $q = CGI->new;
    is($q->url(), 'http://example.com/script/plus+name.cgi', 'a plus sign in a script name is preserved when calling url()');
    is($q->path_info(), '/hello+world', 'a plus sign in a script name is preserved when calling path_info()');
};


done_testing();


