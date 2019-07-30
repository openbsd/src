# copied over from JSON::PP::XS and modified to use JSON::PP

use strict;
use Test::More;
BEGIN { plan tests => 9 };

BEGIN { $ENV{PERL_JSON_BACKEND} = 0; }

BEGIN {
    use lib qw(t);
    use _unicode_handling;
}


use utf8;
use JSON::PP;


ok (JSON::PP->new->allow_nonref (1)->utf8 (1)->encode ("ü") eq "\"\xc3\xbc\"");
ok (JSON::PP->new->allow_nonref (1)->encode ("ü") eq "\"ü\"");

SKIP: {
    skip "UNICODE handling is disabale.", 7 unless $JSON::PP::can_handle_UTF16_and_utf8;

ok (JSON::PP->new->allow_nonref (1)->ascii (1)->utf8 (1)->encode (chr 0x8000) eq '"\u8000"');
ok (JSON::PP->new->allow_nonref (1)->ascii (1)->utf8 (1)->pretty (1)->encode (chr 0x10402) eq "\"\\ud801\\udc02\"\n");

eval { JSON::PP->new->allow_nonref (1)->utf8 (1)->decode ('"ü"') };
ok $@ =~ /malformed UTF-8/;

ok (JSON::PP->new->allow_nonref (1)->decode ('"ü"') eq "ü");
ok (JSON::PP->new->allow_nonref (1)->decode ('"\u00fc"') eq "ü");
ok (JSON::PP->new->allow_nonref (1)->decode ('"\ud801\udc02' . "\x{10204}\"") eq "\x{10402}\x{10204}");
ok (JSON::PP->new->allow_nonref (1)->decode ('"\"\n\\\\\r\t\f\b"') eq "\"\012\\\015\011\014\010");

}
