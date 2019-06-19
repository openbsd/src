
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

my $objGu = Unicode::Collate::Locale->
    new(locale => 'GU', normalization => undef);

ok($objGu->getlocale, 'gu');

$objGu->change(level => 2);

ok($objGu->lt("\x{A82}", "\x{A81}"));

$objGu->change(level => 1);

ok($objGu->eq("\x{A82}", "\x{A81}"));

# 4

for my $h (0, 1) {
    no warnings 'utf8';
    my $t = $h ? pack('U', 0xFFFF) : 'z';

    ok($objGu->lt("\x{AD0}$t", "\x{A82}"));
    ok($objGu->lt("\x{A82}$t", "\x{A83}"));
    ok($objGu->lt("\x{A83}$t", "\x{A85}"));
}

# 10
