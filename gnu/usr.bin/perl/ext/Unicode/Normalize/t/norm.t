
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
BEGIN { plan tests => 18 };
use Unicode::Normalize qw(normalize);
ok(1); # If we made it this far, we're ok.

sub _pack_U   { Unicode::Normalize::pack_U(@_) }
sub _unpack_U { Unicode::Normalize::unpack_U(@_) }

#########################

ok(normalize('C', ""), "");
ok(normalize('D', ""), "");

sub hexNFC {
  join " ", map sprintf("%04X", $_),
  _unpack_U normalize 'C', _pack_U map hex, split ' ', shift;
}
sub hexNFD {
  join " ", map sprintf("%04X", $_),
  _unpack_U normalize 'D', _pack_U map hex, split ' ', shift;
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

