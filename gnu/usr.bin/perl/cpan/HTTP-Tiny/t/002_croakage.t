#!perl

use strict;
use warnings;

use Test::More;
use lib 't';
use Util qw[tmpfile monkey_patch set_socket_source];

use HTTP::Tiny;

BEGIN { monkey_patch() }

my %usage = (
  'get' => q/Usage: $http->get(URL, [HASHREF])/,
  'mirror' => q/Usage: $http->mirror(URL, FILE, [HASHREF])/,
  'request' => q/Usage: $http->request(METHOD, URL, [HASHREF])/,
);

my @cases = (
  ['get'],
  ['get','http://www.example.com/','extra'],
  ['get','http://www.example.com/','extra', 'extra'],
  ['mirror'],
  ['mirror','http://www.example.com/',],
  ['mirror','http://www.example.com/','extra', 'extra'],
  ['mirror','http://www.example.com/','extra', 'extra', 'extra'],
  ['request'],
  ['request','GET'],
  ['request','GET','http://www.example.com/','extra'],
  ['request','GET','http://www.example.com/','extra', 'extra'],
);

my $res_fh = tmpfile();
my $req_fh = tmpfile();

my $http = HTTP::Tiny->new;
set_socket_source($req_fh, $res_fh);

for my $c ( @cases ) {
  my ($method, @args) = @$c;
  eval {$http->$method(@args)};
  my $err = $@;
  like ($err, qr/\Q$usage{$method}\E/, join("|",@$c) );
}

my $res = eval{ $http->get("http://www.example.com/", { headers => { host => "www.example2.com" } } ) };
is( $res->{status}, 599, "Providing a Host header errors with 599" );
like( $res->{content}, qr/'Host' header/, "Providing a Host header gives right error message" );

done_testing;

