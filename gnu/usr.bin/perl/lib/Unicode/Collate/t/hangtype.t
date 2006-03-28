BEGIN {
    unless ("A" eq pack('U', 0x41)) {
	print "1..0 # Unicode::Collate " .
	    "cannot stringify a Unicode code point\n";
	exit 0;
    }
    if ($ENV{PERL_CORE}) {
	chdir('t') if -d 't';
	@INC = $^O eq 'MacOS' ? qw(::lib) : qw(../lib);
    }
}

use Test;
BEGIN { plan tests => 33 };

use strict;
use warnings;
use Unicode::Collate;

ok(1);

#########################

ok(Unicode::Collate::getHST(0x0000), '');
ok(Unicode::Collate::getHST(0x0100), '');
ok(Unicode::Collate::getHST(0x1000), '');
ok(Unicode::Collate::getHST(0x10FF), '');
ok(Unicode::Collate::getHST(0x1100), 'L');
ok(Unicode::Collate::getHST(0x1101), 'L');
ok(Unicode::Collate::getHST(0x1159), 'L');
ok(Unicode::Collate::getHST(0x115A), '');
ok(Unicode::Collate::getHST(0x115E), '');
ok(Unicode::Collate::getHST(0x115F), 'L');
ok(Unicode::Collate::getHST(0x1160), 'V');
ok(Unicode::Collate::getHST(0x1161), 'V');
ok(Unicode::Collate::getHST(0x11A0), 'V');
ok(Unicode::Collate::getHST(0x11A2), 'V');
ok(Unicode::Collate::getHST(0x11A3), '');
ok(Unicode::Collate::getHST(0x11A7), '');
ok(Unicode::Collate::getHST(0x11A8), 'T');
ok(Unicode::Collate::getHST(0x11AF), 'T');
ok(Unicode::Collate::getHST(0x11E0), 'T');
ok(Unicode::Collate::getHST(0x11F9), 'T');
ok(Unicode::Collate::getHST(0x11FA), '');
ok(Unicode::Collate::getHST(0x11FF), '');
ok(Unicode::Collate::getHST(0x3011), '');
ok(Unicode::Collate::getHST(0x11A7), '');
ok(Unicode::Collate::getHST(0xABFF), '');
ok(Unicode::Collate::getHST(0xAC00), 'LV');
ok(Unicode::Collate::getHST(0xAC01), 'LVT');
ok(Unicode::Collate::getHST(0xAC1B), 'LVT');
ok(Unicode::Collate::getHST(0xAC1C), 'LV');
ok(Unicode::Collate::getHST(0xD7A3), 'LVT');
ok(Unicode::Collate::getHST(0xD7A4), '');
ok(Unicode::Collate::getHST(0xFFFF), '');

