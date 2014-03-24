#!./perl -w

BEGIN {
    require './test.pl';
    skip_all_without_dynamic_extension('Encode');
    skip_all("EBCDIC") if $::IS_EBCDIC;
    skip_all_without_perlio();
}

use strict;
plan (tests => 8);
no warnings 'deprecated';
use encoding 'johab';

ok(chr(0x7f) eq "\x7f");
ok(chr(0x80) eq "\x80");
ok(chr(0xff) eq "\xff");

for my $i (127, 128, 255) {
    ok(chr($i) eq pack('C', $i));
}

# [perl #83048]
{
    my $w;
    local $SIG{__WARN__} = sub { $w .= $_[0] };
    my $chr = chr(-1);
    is($chr, "\x{fffd}", "invalid values become REPLACEMENT CHARACTER");
    like($w, qr/^Invalid negative number \(-1\) in chr at /, "with a warning");
}

__END__
