
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

use strict;
use warnings;
BEGIN { $| = 1; print "1..13\n"; }
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

$objKok->change(level => 1);

ok($objKok->lt("\x{950}", "\x{902}"));
ok($objKok->lt("\x{902}", "\x{903}"));
ok($objKok->lt("\x{903}", "\x{972}"));

ok($objKok->eq("\x{902}", "\x{901}"));

ok($objKok->lt("\x{939}", "\x{933}"));
ok($objKok->lt("\x{933}", "\x{915}\x{94D}\x{937}"));
ok($objKok->lt("\x{915}\x{94D}\x{937}", "\x{93D}"));

ok($objKok->eq("\x{933}", "\x{934}"));

# 10

$objKok->change(level => 2);

ok($objKok->lt("\x{902}", "\x{901}"));
ok($objKok->lt("\x{933}", "\x{934}"));

$objKok->change(level => 3);

ok($objKok->eq("\x{933}\x{93C}", "\x{934}"));

# 13
