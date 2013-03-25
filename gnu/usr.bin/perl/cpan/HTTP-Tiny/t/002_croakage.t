#!perl

use strict;
use warnings;

use Test::More;
use HTTP::Tiny;

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

my $http = HTTP::Tiny->new;

for my $c ( @cases ) {
  my ($method, @args) = @$c;
  eval {$http->$method(@args)};
  my $err = $@;
  like ($err, qr/\Q$usage{$method}\E/, join("|",@$c) );
}

done_testing;

