
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
BEGIN { $| = 1; print "1..10\n"; }
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

my $objHi = Unicode::Collate::Locale->
    new(locale => 'HI', normalization => undef);

ok($objHi->getlocale, 'hi');

$objHi->change(level => 2);

ok($objHi->lt("\x{902}", "\x{901}"));

$objHi->change(level => 1);

ok($objHi->eq("\x{902}", "\x{901}"));

# 4

for my $h (0, 1) {
    no warnings 'utf8';
    my $t = $h ? pack('U', 0xFFFF) : "";
    $objHi->change(highestFFFF => 1) if $h;

    ok($objHi->lt("\x{950}$t", "\x{902}"));
    ok($objHi->lt("\x{902}$t", "\x{903}"));
    ok($objHi->lt("\x{903}$t", "\x{972}"));
}

# 10
