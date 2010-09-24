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
BEGIN { plan tests => 40 };

use strict;
use warnings;
use Unicode::Collate;

our $kjeEntry = <<'ENTRIES';
0301  ; [.0000.0032.0002.0301] # COMBINING ACUTE ACCENT
0334  ; [.0000.008B.0002.0334] # COMBINING TILDE OVERLAY
043A  ; [.0D31.0020.0002.043A] # CYRILLIC SMALL LETTER KA
041A  ; [.0D31.0020.0008.041A] # CYRILLIC CAPITAL LETTER KA
045C  ; [.0DA1.0020.0002.045C] # CYRILLIC SMALL LETTER KJE
043A 0301 ; [.0DA1.0020.0002.045C] # CYRILLIC SMALL LETTER KJE
040C  ; [.0DA1.0020.0008.040C] # CYRILLIC CAPITAL LETTER KJE
041A 0301 ; [.0DA1.0020.0008.040C] # CYRILLIC CAPITAL LETTER KJE
ENTRIES

our $aaEntry = <<'ENTRIES';
0304  ; [.0000.005A.0002.0304] # COMBINING MACRON (cc = 230)
030A  ; [.0000.0043.0002.030A] # COMBINING RING ABOVE (cc = 230)
0327  ; [.0000.0055.0002.0327] # COMBINING CEDILLA (cc = 202)
031A  ; [.0000.006B.0002.031A] # COMBINING LEFT ANGLE ABOVE (cc = 232)
0061  ; [.0A15.0020.0002.0061] # LATIN SMALL LETTER A
0041  ; [.0A15.0020.0008.0041] # LATIN CAPITAL LETTER A
007A  ; [.0C13.0020.0002.007A] # LATIN SMALL LETTER Z
005A  ; [.0C13.0020.0008.005A] # LATIN CAPITAL LETTER Z
00E5  ; [.0C25.0020.0002.00E5] # LATIN SMALL LETTER A WITH RING ABOVE; QQCM
00C5  ; [.0C25.0020.0008.00C5] # LATIN CAPITAL LETTER A WITH RING ABOVE; QQCM
0061 030A ; [.0C25.0020.0002.0061] # LATIN SMALL LETTER A WITH RING ABOVE
0041 030A ; [.0C25.0020.0008.0041] # LATIN CAPITAL LETTER A WITH RING ABOVE
ENTRIES

#########################

ok(1);

my $kjeNoN = Unicode::Collate->new(
    level => 1,
    table => undef,
    normalization => undef,
    entry => $kjeEntry,
);

ok($kjeNoN->lt("\x{043A}", "\x{043A}\x{0301}"));
ok($kjeNoN->gt("\x{045C}", "\x{043A}\x{0334}\x{0301}"));
ok($kjeNoN->eq("\x{043A}", "\x{043A}\x{0334}\x{0301}"));
ok($kjeNoN->eq("\x{045C}", "\x{043A}\x{0301}\x{0334}"));

our %sortkeys;

$sortkeys{'KAac'} = $kjeNoN->viewSortKey("\x{043A}\x{0301}");
$sortkeys{'KAta'} = $kjeNoN->viewSortKey("\x{043A}\x{0334}\x{0301}");
$sortkeys{'KAat'} = $kjeNoN->viewSortKey("\x{043A}\x{0301}\x{0334}");

eval { require Unicode::Normalize };
if (!$@) {
    my $kjeNFD = Unicode::Collate->new(
	level => 1,
	table => undef,
	entry => $kjeEntry,
    );
ok($kjeNFD->lt("\x{043A}", "\x{043A}\x{0301}"));
ok($kjeNFD->eq("\x{045C}", "\x{043A}\x{0334}\x{0301}"));
ok($kjeNFD->lt("\x{043A}", "\x{043A}\x{0334}\x{0301}"));
ok($kjeNFD->eq("\x{045C}", "\x{043A}\x{0301}\x{0334}"));

    my $aaNFD = Unicode::Collate->new(
	level => 1,
	table => undef,
	entry => $aaEntry,
    );

ok($aaNFD->lt("Z", "A\x{30A}\x{304}"));
ok($aaNFD->eq("A", "A\x{304}\x{30A}"));
ok($aaNFD->eq(pack('U', 0xE5), "A\x{30A}\x{304}"));
ok($aaNFD->eq("A\x{304}", "A\x{304}\x{30A}"));
ok($aaNFD->lt("Z", "A\x{327}\x{30A}"));
ok($aaNFD->lt("Z", "A\x{30A}\x{327}"));
ok($aaNFD->lt("Z", "A\x{31A}\x{30A}"));
ok($aaNFD->lt("Z", "A\x{30A}\x{31A}"));

    my $aaPre = Unicode::Collate->new(
	level => 1,
	normalization => "prenormalized",
	table => undef,
	entry => $aaEntry,
    );

ok($aaPre->lt("Z", "A\x{30A}\x{304}"));
ok($aaPre->eq("A", "A\x{304}\x{30A}"));
ok($aaPre->eq(pack('U', 0xE5), "A\x{30A}\x{304}"));
ok($aaPre->eq("A\x{304}", "A\x{304}\x{30A}"));
ok($aaPre->lt("Z", "A\x{327}\x{30A}"));
ok($aaPre->lt("Z", "A\x{30A}\x{327}"));
ok($aaPre->lt("Z", "A\x{31A}\x{30A}"));
ok($aaPre->lt("Z", "A\x{30A}\x{31A}"));
}
else {
  ok(1) for 1..20;
}

# again: loading Unicode::Normalize should not affect $kjeNoN.
ok($kjeNoN->lt("\x{043A}", "\x{043A}\x{0301}"));
ok($kjeNoN->gt("\x{045C}", "\x{043A}\x{0334}\x{0301}"));
ok($kjeNoN->eq("\x{043A}", "\x{043A}\x{0334}\x{0301}"));
ok($kjeNoN->eq("\x{045C}", "\x{043A}\x{0301}\x{0334}"));

ok($sortkeys{'KAac'}, $kjeNoN->viewSortKey("\x{043A}\x{0301}"));
ok($sortkeys{'KAta'}, $kjeNoN->viewSortKey("\x{043A}\x{0334}\x{0301}"));
ok($sortkeys{'KAat'}, $kjeNoN->viewSortKey("\x{043A}\x{0301}\x{0334}"));

my $aaNoN = Unicode::Collate->new(
    level => 1,
    table => undef,
    entry => $aaEntry,
    normalization => undef,
);

ok($aaNoN->lt("Z", "A\x{30A}\x{304}"));
ok($aaNoN->eq("A", "A\x{304}\x{30A}"));
ok($aaNoN->eq(pack('U', 0xE5), "A\x{30A}\x{304}"));
ok($aaNoN->eq("A\x{304}", "A\x{304}\x{30A}"));
ok($aaNoN->eq("A", "A\x{327}\x{30A}"));
ok($aaNoN->lt("Z", "A\x{30A}\x{327}"));
ok($aaNoN->eq("A", "A\x{31A}\x{30A}"));
ok($aaNoN->lt("Z", "A\x{30A}\x{31A}"));

