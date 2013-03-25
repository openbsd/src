
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

my $objHy = Unicode::Collate::Locale->
    new(locale => 'HY', normalization => undef);

ok($objHy->getlocale, 'hy');

$objHy->change(level => 1);

ok($objHy->lt("\x{584}", "\x{587}"));
ok($objHy->gt("\x{585}", "\x{587}"));

ok($objHy->lt("\x{584}\x{4E00}",  "\x{587}"));
ok($objHy->lt("\x{584}\x{20000}", "\x{587}"));
ok($objHy->lt("\x{584}\x{10FFFD}","\x{587}"));

# 7

$objHy->change(level => 2);

ok($objHy->eq("\x{587}", "\x{535}\x{582}"));

$objHy->change(level => 3);

ok($objHy->lt("\x{587}", "\x{535}\x{582}"));

$objHy->change(upper_before_lower => 1);

ok($objHy->gt("\x{587}", "\x{535}\x{582}"));

# 10

$objHy->change(UCA_Version => 8);

ok($objHy->lt("\x{584}\x{4E00}",  "\x{587}"));
ok($objHy->lt("\x{584}\x{20000}", "\x{587}"));
ok($objHy->lt("\x{584}\x{10FFFD}","\x{587}"));

# 13
