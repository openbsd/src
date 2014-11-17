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
    ok(!defined $c->http_proxy);
}

# trailing / is optional
for my $proxy ("http://localhost:8080/", "http://localhost:8080"){
    local $ENV{http_proxy} = $proxy;
    my $c = HTTP::Tiny->new();
    is($c->http_proxy, $proxy);
}

# http_proxy must be http://<host>:<port> format
{
    local $ENV{http_proxy} = "localhost:8080";
    eval {
        my $c = HTTP::Tiny->new();
    };
    like($@, qr{http_proxy URL must be in format http\[s\]://\[auth\@\]<host>:<port>/});
}


done_testing();
