#!perl

use strict;
use warnings;

use File::Basename;
use Test::More 0.88;

use HTTP::Tiny;

# Require a true value
for my $proxy (undef, "", 0){
    local $ENV{http_proxy} = $proxy;
    my $c = HTTP::Tiny->new();
    ok(!defined $c->proxy);
}

# trailing / is optional
for my $proxy ("http://localhost:8080/", "http://localhost:8080"){
    local $ENV{http_proxy} = $proxy;
    my $c = HTTP::Tiny->new();
    is($c->proxy, $proxy);
}

# http_proxy must be http://<host>:<port> format
{
    local $ENV{http_proxy} = "localhost:8080";
    eval {
        my $c = HTTP::Tiny->new();
    };
    like($@, qr{Environment 'http_proxy' must be in format http://<host>:<port>/});
}


done_testing();