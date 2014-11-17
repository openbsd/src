#!perl

use strict;
use warnings;

use Test::More tests => 4;
use t::SimpleCookieJar;
use t::BrokenCookieJar;
use HTTP::Tiny;

### a couple tests to ensure that:
###  * by default there is no cookie jar defined
###  * the correct cookie jar is returned when specified
###  * error when cookie jar does not support the add and cookie_header methods


my $default = undef;
my $jar = t::SimpleCookieJar->new();
my $mug = t::BrokenCookieJar->new();
my $dog = t::BrokenCookieJar2->new();

{
    my $ua = HTTP::Tiny->new();
    is $ua->cookie_jar, $default, 'default cookie jar is as expected';
}

{
    my $ua = HTTP::Tiny->new(cookie_jar => $jar);
    is $ua->cookie_jar, $jar, 'cookie_jar is as expected';
}

{
    my $ua = eval { HTTP::Tiny->new(cookie_jar => $mug) };
    my $err = $@;
    like( $err, qr/must provide .* 'add' method/
	  => 'invalid jar does not support add method' );
    
    $ua = eval { HTTP::Tiny->new(cookie_jar => $dog) };
    $err = $@;
    like( $err, qr/must provide .* 'cookie_header' method/
	  => 'invalid jar does not support cookie_header method' );
}
