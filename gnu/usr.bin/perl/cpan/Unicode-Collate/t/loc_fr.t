
BEGIN {
    unless ('A' eq pack('U', 0x41)) {
	print "1..0 # Unicode::Collate cannot pack a Unicode code point\n";
	exit 0;
    }
    unless (0x41 == unpack('U', 'A')) {
	print "1..0 # Unicode::Collate cannot get a Unicode code point\n";
	exit 0;
    }
    if ($ENV{PERL_CORE}) {
	chdir('t') if -d 't';
	@INC = $^O eq 'MacOS' ? qw(::lib) : qw(../lib);
    }
}

use strict;
use warnings;
BEGIN { $| = 1; print "1..27\n"; }
my $count = 0;
sub ok ($;$) {
    my $p = my $r = shift;
    if (@_) {
	my $x = shift;
	$p = !defined $x ? !defined $r : !defined $r ? 0 : $r eq $x;
    }
    print $p ? "ok" : "not ok", ' ', ++$count, "\n";
}

use Unicode::Collate::Locale;

ok(1);

#########################

my $ae = pack 'U', 0xE6;
my $AE = pack 'U', 0xC6;

my $objFr = Unicode::Collate::Locale->
    new(locale => 'FR', normalization => undef);

ok($objFr->getlocale, 'fr');
ok($objFr->locale_version, 0.87);

$objFr->change(level => 1);

ok($objFr->eq($ae, "ae"));
ok($objFr->eq($AE, "AE"));
ok($objFr->eq("\x{1FD}", $ae));
ok($objFr->eq("\x{1FC}", $AE));
ok($objFr->eq("\x{1E3}", $ae));
ok($objFr->eq("\x{1E2}", $AE));

# 9

$objFr->change(level => 2);

ok($objFr->gt($ae, "ae"));
ok($objFr->gt($AE, "AE"));
ok($objFr->gt("\x{1FD}", $ae));
ok($objFr->gt("\x{1FC}", $AE));
ok($objFr->gt("\x{1E3}", $ae));
ok($objFr->gt("\x{1E2}", $AE));

ok($objFr->eq($ae, $AE));
ok($objFr->eq($AE, "\x{1D2D}"));
ok($objFr->eq("$ae\x{304}", "$AE\x{304}"));
ok($objFr->eq("$ae\x{301}", "$AE\x{301}"));

# 19

$objFr->change(level => 3);

ok($objFr->lt($ae, $AE));
ok($objFr->lt($AE, "\x{1D2D}"));
ok($objFr->lt("$ae\x{304}", "$AE\x{304}"));
ok($objFr->lt("$ae\x{301}", "$AE\x{301}"));

ok($objFr->eq("\x{1FD}", "$ae\x{301}"));
ok($objFr->eq("\x{1FC}", "$AE\x{301}"));
ok($objFr->eq("\x{1E3}", "$ae\x{304}"));
ok($objFr->eq("\x{1E2}", "$AE\x{304}"));

# 27
