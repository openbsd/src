#!perl -T

use strict;
use warnings;

use utf8;

use Test::More tests => 7;
use Encode;

use_ok( 'CGI' );

ok( my $q = CGI->new, 'create a new CGI object' );

{
    no warnings qw/ once /;
    $CGI::PARAM_UTF8 = 1;
}

my $data = 'áéíóúµ';
ok Encode::is_utf8($data), "created UTF-8 encoded data string";

# now set the param.
$q->param(data => $data);

# if param() runs the data  through Encode::decode(), this will fail.
is $q->param('data'), $data;

# make sure setting bytes decodes properly
my $bytes = Encode::encode(utf8 => $data);
ok !Encode::is_utf8($bytes), "converted UTF-8 to bytes";
$q->param(data => $bytes);
is $q->param('data'), $data;
ok Encode::is_utf8($q->param('data')), 'param() decoded UTF-8';
