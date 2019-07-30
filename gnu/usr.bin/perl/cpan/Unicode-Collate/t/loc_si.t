
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
BEGIN { $| = 1; print "1..16\n"; }
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

my $objSi = Unicode::Collate::Locale->
    new(locale => 'SI', normalization => undef);

ok($objSi->getlocale, 'si');

$objSi->change(level => 1);

for my $h (0, 1) {
    no warnings 'utf8';
    my $t = $h ? pack('U', 0xFFFF) : "";
    $objSi->change(highestFFFF => 1) if $h;

    ok($objSi->lt("\x{D95}$t", "\x{D96}"));
    ok($objSi->lt("\x{D96}$t", "\x{D82}"));
    ok($objSi->lt("\x{D82}$t", "\x{D83}"));
    ok($objSi->lt("\x{D83}$t", "\x{D9A}"));

    ok($objSi->lt("\x{DA3}$t", "\x{DA5}"));
    ok($objSi->lt("\x{DA5}$t", "\x{DA4}"));
    ok($objSi->lt("\x{DA4}$t", "\x{DA6}"));
}

