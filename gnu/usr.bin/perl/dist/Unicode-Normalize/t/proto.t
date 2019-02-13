
BEGIN {
    unless ('A' eq pack('U', 0x41)) {
	print "1..0 # Unicode::Normalize cannot pack a Unicode code point\n";
	exit 0;
    }
    unless (0x41 == unpack('U', 'A')) {
	print "1..0 # Unicode::Normalize cannot get a Unicode code point\n";
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

use strict;
use warnings;
BEGIN { $| = 1; print "1..48\n"; }
my $count = 0;
sub ok ($;$) {
    my $p = my $r = shift;
    if (@_) {
	my $x = shift;
	$p = !defined $x ? !defined $r : !defined $r ? 0 : $r eq $x;
    }
    print $p ? "ok" : "not ok", ' ', ++$count, "\n";
}

use Unicode::Normalize qw(:all);

ok(1);

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

# 9

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

# 27

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
ok(prototype \&splitOnLastStarter, undef);
ok(prototype \&normalize_partial, '$$');
ok(prototype \&NFD_partial,  '$');
ok(prototype \&NFC_partial,  '$');
ok(prototype \&NFKD_partial, '$');
ok(prototype \&NFKC_partial, '$');

# 48

