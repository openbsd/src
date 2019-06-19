
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
BEGIN { $| = 1; print "1..8\n"; }
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

my $objFrCa = Unicode::Collate::Locale->
    new(locale => 'FR-CA', normalization => undef);

ok($objFrCa->getlocale, 'fr_CA');

$objFrCa->change(level => 2);

ok($objFrCa->lt("a\x{300}a", "aa\x{300}"));
ok($objFrCa->gt("Ca\x{300}ca\x{302}", "ca\x{302}ca\x{300}"));
ok($objFrCa->gt("ca\x{300}ca\x{302}", "Ca\x{302}ca\x{300}"));

$objFrCa->change(backwards => undef);

ok($objFrCa->gt("a\x{300}a", "aa\x{300}"));
ok($objFrCa->lt("Ca\x{300}ca\x{302}", "ca\x{302}ca\x{300}"));
ok($objFrCa->lt("ca\x{300}ca\x{302}", "Ca\x{302}ca\x{300}"));

# 8
