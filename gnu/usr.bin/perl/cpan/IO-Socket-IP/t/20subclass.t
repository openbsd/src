#!/usr/bin/perl

use v5.14;
use warnings;

use Test::More;

use IO::Socket::IP;

package MySubclass {
   use base qw( IO::Socket::IP );
}

my $server = IO::Socket::IP->new(
   Listen    => 1,
   LocalHost => "127.0.0.1",
   LocalPort => 0,
) or die "Cannot listen on PF_INET - $IO::Socket::errstr";

my $client = IO::Socket::IP->new(
   PeerHost => $server->sockhost,
   PeerPort => $server->sockport,
) or die "Cannot connect on PF_INET - $IO::Socket::errstr";

my $accepted = $server->accept( 'MySubclass' )
   or die "Cannot accept - $!";

isa_ok( $accepted, 'MySubclass' );

done_testing;
