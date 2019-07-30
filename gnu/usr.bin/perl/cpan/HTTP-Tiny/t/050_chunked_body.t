#!perl

use strict;
use warnings;

use Test::More qw[no_plan];
use t::Util    qw[tmpfile rewind $CRLF];
use HTTP::Tiny;

{
    no warnings 'redefine';
    sub HTTP::Tiny::Handle::can_read  { 1 };
    sub HTTP::Tiny::Handle::can_write { 1 };
}

{
    my $body    = join($CRLF, map { sprintf('%x', length $_) . $CRLF . $_ } 'A'..'Z', '') . $CRLF;
    my $fh      = tmpfile($body);
    my $handle  = HTTP::Tiny::Handle->new(fh => $fh);
    my $exp     = ['A'..'Z'];
    my $got     = [];
    my $cb      = sub { push @$got, $_[0] };
    my $response = { headers => {} };
    $handle->read_chunked_body($cb, $response);
    is_deeply($response->{headers}, {}, 'chunked trailers');
    is_deeply($got, $exp, "chunked chunks");
}

{
    my $fh      = tmpfile();
    my $handle  = HTTP::Tiny::Handle->new(fh => $fh);

    my $exp      = ['A'..'Z'];
    my $trailers = { foo => 'Bar', bar => 'Baz' };
    my $got      = [];

    {
        my @chunks = @$exp;
        my $request = {
          cb => sub { shift @chunks },
          trailer_cb => sub { $trailers },
        };
        $handle->write_chunked_body($request);
    }

    rewind($fh);

    {
        my $cb = sub { push @$got, $_[0] };
        my $response = { headers => {} };
        $handle->read_chunked_body($cb, $response);
        is_deeply($response->{headers}, $trailers, 'roundtrip chunked trailers');
    }

    is_deeply($got, $exp, "roundtrip chunked chunks");
}


