
BEGIN {
    unless ("A" eq pack('U', 0x41)) {
	print "1..0 # Unicode::Normalize " .
	    "cannot stringify a Unicode code point\n";
	exit 0;
    }
}

BEGIN {
    if ($ENV{PERL_CORE}) {
        chdir('t') if -d 't';
        @INC = $^O eq 'MacOS' ? qw(::lib) : qw(../lib);
    }
}

#########################

use Test;
use strict;
use warnings;
BEGIN { plan tests => 20 };
use Unicode::Normalize;
ok(1); # If we made it this far, we're ok.

sub _pack_U   { Unicode::Normalize::pack_U(@_) }
sub _unpack_U { Unicode::Normalize::unpack_U(@_) }

#########################

ok(NFC(""), "");
ok(NFD(""), "");

sub hexNFC {
  join " ", map sprintf("%04X", $_),
  _unpack_U NFC _pack_U map hex, split ' ', shift;
}
sub hexNFD {
  join " ", map sprintf("%04X", $_),
  _unpack_U NFD _pack_U map hex, split ' ', shift;
}

ok(hexNFC("0061 0315 0300 05AE 05C4 0062"), "00E0 05AE 05C4 0315 0062");
ok(hexNFC("00E0 05AE 05C4 0315 0062"),      "00E0 05AE 05C4 0315 0062");
ok(hexNFC("0061 05AE 0300 05C4 0315 0062"), "00E0 05AE 05C4 0315 0062");
ok(hexNFC("0045 0304 0300 AC00 11A8"), "1E14 AC01");
ok(hexNFC("1100 1161 1100 1173 11AF"), "AC00 AE00");
ok(hexNFC("1100 0300 1161 1173 11AF"), "1100 0300 1161 1173 11AF");

ok(hexNFD("0061 0315 0300 05AE 05C4 0062"), "0061 05AE 0300 05C4 0315 0062");
ok(hexNFD("00E0 05AE 05C4 0315 0062"),      "0061 05AE 0300 05C4 0315 0062");
ok(hexNFD("0061 05AE 0300 05C4 0315 0062"), "0061 05AE 0300 05C4 0315 0062");
ok(hexNFC("0061 05C4 0315 0300 05AE 0062"), "0061 05AE 05C4 0300 0315 0062");
ok(hexNFC("0061 05AE 05C4 0300 0315 0062"), "0061 05AE 05C4 0300 0315 0062");
ok(hexNFD("0061 05C4 0315 0300 05AE 0062"), "0061 05AE 05C4 0300 0315 0062");
ok(hexNFD("0061 05AE 05C4 0300 0315 0062"), "0061 05AE 05C4 0300 0315 0062");
ok(hexNFC("0000 0041 0000 0000"), "0000 0041 0000 0000");
ok(hexNFD("0000 0041 0000 0000"), "0000 0041 0000 0000");

# should be unary.
my $str11 = _pack_U(0x41, 0x0302, 0x0301, 0x62);
my $str12 = _pack_U(0x1EA4, 0x62);
ok(NFC $str11 eq $str12);

my $str21 = _pack_U(0xE0, 0xAC00);
my $str22 = _pack_U(0x61, 0x0300, 0x1100, 0x1161);
ok(NFD $str21 eq $str22);

