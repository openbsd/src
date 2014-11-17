#
# $Id: decode.t,v 1.1 2013/08/29 16:47:39 dankogai Exp $
#
use strict;
use Encode qw(decode_utf8 FB_CROAK);
use Test::More tests => 3;

sub croak_ok(&) {
    my $code = shift;
    eval { $code->() };
    like $@, qr/does not map/;
}

my $bytes = "L\x{e9}on";
my $pad = "\x{30C9}";

my $orig = $bytes;
croak_ok { Encode::decode_utf8($orig, FB_CROAK) };

my $orig2 = $bytes;
croak_ok { Encode::decode('utf-8', $orig2, FB_CROAK) };

chop(my $new = $bytes . $pad);
croak_ok { Encode::decode_utf8($new, FB_CROAK) };

