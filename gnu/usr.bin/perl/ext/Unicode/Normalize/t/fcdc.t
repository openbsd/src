
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
BEGIN { plan tests => 35 };
use Unicode::Normalize qw(:all);
ok(1); # If we made it this far, we're ok.

sub _pack_U   { Unicode::Normalize::pack_U(@_) }
sub _unpack_U { Unicode::Normalize::unpack_U(@_) }
sub answer { defined $_[0] ? $_[0] ? "YES" : "NO" : "MAYBE" }

#########################

ok(answer(checkFCD('')), 'YES');
ok(answer(checkFCD('A')), 'YES');
ok(answer(checkFCD("\x{030A}")), 'YES');  # 030A;COMBINING RING ABOVE
ok(answer(checkFCD("\x{0327}")), 'YES'); # 0327;COMBINING CEDILLA
ok(answer(checkFCD(_pack_U(0x00C5))), 'YES'); # A with ring above
ok(answer(checkFCD(_pack_U(0x41, 0x30A))), 'YES'); # A+ring
ok(answer(checkFCD(_pack_U(0x41, 0x327, 0x30A))), 'YES'); # A+cedilla+ring
ok(answer(checkFCD(_pack_U(0x41, 0x30A, 0x327))), 'NO');  # A+ring+cedilla
ok(answer(checkFCD(_pack_U(0xC5, 0x0327))), 'NO'); # A-ring+cedilla
ok(answer(checkNFC(_pack_U(0xC5, 0x0327))), 'MAYBE'); # NFC: A-ring+cedilla
ok(answer(check("FCD", _pack_U(0xC5, 0x0327))), 'NO');
ok(answer(check("NFC", _pack_U(0xC5, 0x0327))), 'MAYBE');
ok(answer(checkFCD("\x{AC01}\x{1100}\x{1161}")), 'YES'); # hangul
ok(answer(checkFCD("\x{212B}\x{F900}")), 'YES'); # compat

ok(FCD(''), "");
ok(FCC(''), "");

ok(FCD('A'), "A");
ok(FCC('A'), "A");

ok(answer(checkFCD(_pack_U(0x1EA7, 0x05AE, 0x0315, 0x0062))), "NO");
ok(answer(checkFCC(_pack_U(0x1EA7, 0x05AE, 0x0315, 0x0062))), "NO");

ok(FCC(_pack_U(0xC5, 0x327)), _pack_U(0x41, 0x327, 0x30A));
ok(FCC(_pack_U(0x45, 0x304, 0x300)), _pack_U(0x1E14));
ok(FCC("\x{1100}\x{1161}\x{1100}\x{1173}\x{11AF}"), "\x{AC00}\x{AE00}");

ok(answer(checkFCC('')), 'YES');
ok(answer(checkFCC('A')), 'YES');
ok(answer(checkFCC("\x{030A}")), 'MAYBE');  # 030A;COMBINING RING ABOVE
ok(answer(checkFCC("\x{0327}")), 'MAYBE'); # 0327;COMBINING CEDILLA
ok(answer(checkFCC(_pack_U(0x00C5))), 'YES'); # A with ring above
ok(answer(checkFCC(_pack_U(0x41, 0x30A))), 'MAYBE'); # A+ring
ok(answer(checkFCC(_pack_U(0x41, 0x327, 0x30A))), 'MAYBE'); # A+cedilla+ring
ok(answer(checkFCC(_pack_U(0x41, 0x30A, 0x327))), 'NO');  # A+ring+cedilla
ok(answer(checkFCC(_pack_U(0xC5, 0x0327))), 'NO'); # A-ring+cedilla
ok(answer(checkFCC("\x{AC01}\x{1100}\x{1161}")), 'MAYBE'); # hangul
ok(answer(checkFCC("\x{212B}\x{F900}")), 'NO'); # compat

