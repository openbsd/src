
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
BEGIN { plan tests => 42 };
use Unicode::Normalize qw(:all);
ok(1); # If we made it this far, we're ok.

#########################

# unary op. RING-CEDILLA
ok(        "\x{30A}\x{327}" ne "\x{327}\x{30A}");
ok(NFD     "\x{30A}\x{327}" eq "\x{327}\x{30A}");
ok(NFC     "\x{30A}\x{327}" eq "\x{327}\x{30A}");
ok(NFKD    "\x{30A}\x{327}" eq "\x{327}\x{30A}");
ok(NFKC    "\x{30A}\x{327}" eq "\x{327}\x{30A}");
ok(FCD     "\x{30A}\x{327}" eq "\x{327}\x{30A}");
ok(FCC     "\x{30A}\x{327}" eq "\x{327}\x{30A}");
ok(reorder "\x{30A}\x{327}" eq "\x{327}\x{30A}");

ok(prototype \&normalize,'$$');
ok(prototype \&NFD,  '$');
ok(prototype \&NFC,  '$');
ok(prototype \&NFKD, '$');
ok(prototype \&NFKC, '$');
ok(prototype \&FCD,  '$');
ok(prototype \&FCC,  '$');

ok(prototype \&check,    '$$');
ok(prototype \&checkNFD, '$');
ok(prototype \&checkNFC, '$');
ok(prototype \&checkNFKD,'$');
ok(prototype \&checkNFKC,'$');
ok(prototype \&checkFCD, '$');
ok(prototype \&checkFCC, '$');

ok(prototype \&decompose, '$;$');
ok(prototype \&reorder,   '$');
ok(prototype \&compose,   '$');
ok(prototype \&composeContiguous, '$');

ok(prototype \&getCanon,      '$');
ok(prototype \&getCompat,     '$');
ok(prototype \&getComposite,  '$$');
ok(prototype \&getCombinClass,'$');
ok(prototype \&isExclusion,   '$');
ok(prototype \&isSingleton,   '$');
ok(prototype \&isNonStDecomp, '$');
ok(prototype \&isComp2nd,     '$');
ok(prototype \&isComp_Ex,     '$');

ok(prototype \&isNFD_NO,      '$');
ok(prototype \&isNFC_NO,      '$');
ok(prototype \&isNFC_MAYBE,   '$');
ok(prototype \&isNFKD_NO,     '$');
ok(prototype \&isNFKC_NO,     '$');
ok(prototype \&isNFKC_MAYBE,  '$');

