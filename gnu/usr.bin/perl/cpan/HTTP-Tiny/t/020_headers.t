#!perl

use strict;
use warnings;

use Test::More qw[no_plan];
use lib 't';
use Util    qw[tmpfile rewind $CRLF $LF];
use HTTP::Tiny;

{
    no warnings 'redefine';
    sub HTTP::Tiny::Handle::can_read  { 1 };
    sub HTTP::Tiny::Handle::can_write { 1 };
}

{
    my $header = join $CRLF, 'Foo: Foo', 'Foo: Baz', 'Bar: Bar', '', '';
    my $fh     = tmpfile($header);
    my $exp    = { foo => ['Foo', 'Baz'], bar => 'Bar' };
    my $handle = HTTP::Tiny::Handle->new(fh => $fh);
    my $got    = $handle->read_header_lines;
    is_deeply($got, $exp, "->read_header_lines() CRLF");
}

{
    my $header = join $LF, 'Foo: Foo', 'Foo: Baz', 'Bar: Bar', '', '';
    my $fh     = tmpfile($header);
    my $exp    = { foo => ['Foo', 'Baz'], bar => 'Bar' };
    my $handle = HTTP::Tiny::Handle->new(fh => $fh);
    my $got    = $handle->read_header_lines;
    is_deeply($got, $exp, "->read_header_lines() LF");
}

{
    my $header = "Foo: $CRLF\x09Bar$CRLF\x09$CRLF\x09Baz$CRLF$CRLF";
    my $fh     = tmpfile($header);
    my $exp    = { foo => 'Bar Baz' };
    my $handle = HTTP::Tiny::Handle->new(fh => $fh);
    my $got    = $handle->read_header_lines;
    is_deeply($got, $exp, "->read_header_lines() insane continuations");
}

{
    my $fh      = tmpfile();
    my $handle  = HTTP::Tiny::Handle->new(fh => $fh);
    my $headers = { foo => ['Foo', 'Baz'], bar => 'Bar' };
    $handle->write_header_lines($headers);
    rewind($fh);
    is_deeply($handle->read_header_lines, $headers, "roundtrip header lines");
}

{
    my $fh      = tmpfile();
    my $handle  = HTTP::Tiny::Handle->new(fh => $fh);
    my $headers = { foo => ['Foo', 'Baz'], bar => 'Bar', baz => '' };
    $handle->write_header_lines($headers);
    rewind($fh);
    is_deeply($handle->read_header_lines, $headers, "roundtrip header lines");
}

{
    my $fh     = tmpfile();
    my $handle = HTTP::Tiny::Handle->new(fh => $fh);
    eval { $handle->write_header_lines({ range => "bytes=13-37${CRLF}X-Injected: foo" }) };
    like($@, qr/Invalid HTTP header field value \(Range\)/,
         "reject CRLF in control field value");
}

{
    my $fh     = tmpfile();
    my $handle = HTTP::Tiny::Handle->new(fh => $fh);
    eval { $handle->write_header_lines({ "X-Foo-Bar" => "foo${CRLF}X-Injected: foo" }) };
    like($@, qr/Invalid HTTP header field value \(X-Foo-Bar\)/,
         "reject CRLF in other header value");
}

{
    my $fh     = tmpfile();
    my $handle = HTTP::Tiny::Handle->new(fh => $fh);
    eval { $handle->write_request_header("GET${CRLF}", "/foo", {}, {}) };
    like($@, qr/Invalid characters in Method/,
         "->write_request_header() reject CRLF in method");
}

{
    my $fh     = tmpfile();
    my $handle = HTTP::Tiny::Handle->new(fh => $fh);
    eval { $handle->write_request_header("GET\x00", "/foo", {}, {}) };
    like($@, qr/Invalid characters in Method/,
         "->write_request_header() reject nullbyte in method");
}

{
    my $fh     = tmpfile();
    my $handle = HTTP::Tiny::Handle->new(fh => $fh);
    eval { $handle->write_request_header("GET ", "/foo", {}, {}) };
    like($@, qr/Invalid characters in Method/,
         "->write_request_header() reject trailing space in method");
}

{
    my $fh     = tmpfile();
    my $handle = HTTP::Tiny::Handle->new(fh => $fh);
    eval { $handle->write_request_header("GET", "/foo${CRLF}Foo: 1", {}, {}) };
    like($@, qr/Invalid characters in Request-URI/,
         "->write_request_header() reject CRLF in request-uri");
}

{
    my $fh     = tmpfile();
    my $handle = HTTP::Tiny::Handle->new(fh => $fh);
    eval { $handle->write_request_header("GET", "/foo bar", {}, {}) };
    like($@, qr/Invalid characters in Request-URI/,
         "->write_request_header() reject space in request-uri");
}
