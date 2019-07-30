
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
BEGIN { $| = 1; print "1..21\n"; }
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

my $objKok = Unicode::Collate::Locale->
    new(locale => 'KOK', normalization => undef);

ok($objKok->getlocale, 'kok');

$objKok->change(level => 2);

ok($objKok->lt("\x{902}", "\x{901}"));
ok($objKok->lt("\x{933}", "\x{934}"));

$objKok->change(level => 3);

ok($objKok->eq("\x{933}\x{93C}", "\x{934}"));

$objKok->change(level => 1);

ok($objKok->eq("\x{902}", "\x{901}"));
ok($objKok->eq("\x{933}", "\x{934}"));

# 7

for my $h (0, 1) {
    no warnings 'utf8';
    my $t = $h ? pack('U', 0xFFFF) : "";
    $objKok->change(highestFFFF => 1) if $h;

    ok($objKok->lt("\x{950}$t", "\x{902}"));
    ok($objKok->lt("\x{902}$t", "\x{903}"));
    ok($objKok->lt("\x{903}$t", "\x{972}"));

    ok($objKok->lt("\x{938}$t", "\x{939}"));
    ok($objKok->lt("\x{939}$t", "\x{933}"));
    ok($objKok->lt("\x{933}$t", "\x{915}\x{94D}\x{937}"));
    ok($objKok->lt("\x{915}\x{94D}\x{937}$t", "\x{93D}"));
}

# 21
