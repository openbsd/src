#
# decode on Perl 5.005, 5.6, 5.8 or later
#
use strict;
use warnings;
use Test::More;

BEGIN { plan tests => 7 };

BEGIN { $ENV{PERL_JSON_BACKEND} = 0; }

use JSON::PP;

no utf8;

my $json = JSON::PP->new->allow_nonref;

is($json->encode("ü"),                   q|"ü"|); # as is

$json->ascii;

is($json->encode("\xfc"),           q|"\u00fc"|); # latin1
is($json->encode("\xc3\xbc"), q|"\u00c3\u00bc"|); # utf8
is($json->encode("ü"),        q|"\u00c3\u00bc"|); # utf8
is($json->encode('あ'), q|"\u00e3\u0081\u0082"|);

if ($] >= 5.006) {
    is($json->encode(chr hex 3042 ),  q|"\u3042"|);
    is($json->encode(chr hex 12345 ), q|"\ud808\udf45"|);
}
else {
    is($json->encode(chr hex 3042 ),  $json->encode(chr 66));
    is($json->encode(chr hex 12345 ), $json->encode(chr 69));
}

