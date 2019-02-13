#!perl

use strict;
use warnings;

use Test::More qw[no_plan];
use lib 't';
use Util    qw[tmpfile rewind $CRLF $LF];
use HTTP::Tiny;

sub _header {
  return [ @{$_[0]}{qw/status reason headers protocol/} ]
}

{
    no warnings 'redefine';
    sub HTTP::Tiny::Handle::can_read  { 1 };
    sub HTTP::Tiny::Handle::can_write { 1 };
}

{
    my $response = join $CRLF, 'HTTP/1.1 200 OK', 'Foo: Foo', 'Bar: Bar', '', '';
    my $fh       = tmpfile($response);
    my $handle   = HTTP::Tiny::Handle->new(fh => $fh);
    my $exp      = [ 200, 'OK', { foo => 'Foo', bar => 'Bar' }, 'HTTP/1.1' ];
    is_deeply(_header($handle->read_response_header), $exp, "->read_response_header CRLF");
}

{
    my $response = join $LF, 'HTTP/1.1 200 OK', 'Foo: Foo', 'Bar: Bar', '', '';
    my $fh       = tmpfile($response);
    my $handle   = HTTP::Tiny::Handle->new(fh => $fh);
    my $exp      = [ 200, 'OK', { foo => 'Foo', bar => 'Bar' }, 'HTTP/1.1' ];
    is_deeply(_header($handle->read_response_header), $exp, "->read_response_header LF");
}

