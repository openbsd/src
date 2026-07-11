#!perl

use strict;
use warnings;
use Test::More 0.88;
use lib 't';

use HTTP::Tiny;

plan 'skip_all' => "Only run if HTTP::Tiny->can_ssl()"
  unless $ENV{RELEASE_TESTING} || HTTP::Tiny->can_ssl(); # also requires IO::Socket:SSL


delete $ENV{SSL_CERT_FILE};
delete $ENV{SSL_CERT_DIR};


$ENV{SSL_CERT_FILE} = "corpus/snake-oil.crt";


my $handle = HTTP::Tiny::Handle->new();

# RELEASE_TESTING may skip this call, so ensure it is done again. _find_CA
# relies on IO::Socket::SSL being loaded, which would always be done if we
# weren't bypassing the public API.
HTTP::Tiny->can_ssl;

my %ret = $handle->_find_CA();

is($ret{SSL_ca_file}, "corpus/snake-oil.crt",
   "HTTP::Tiny::Handle::_find_CA returns expected SSL_CERT_FILE in SSL_ca_file");

is($handle->_find_CA_file(), "corpus/snake-oil.crt",
   "HTTP::Tiny::Handle::_find_CA_file() backwards compat shim returns expected SSL_CERT_FILE");


done_testing;

