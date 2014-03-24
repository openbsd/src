
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
BEGIN { $| = 1; print "1..12\n"; }
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

my $objBn = Unicode::Collate::Locale->
    new(locale => 'BN', normalization => undef);

ok($objBn->getlocale, 'bn');

$objBn->change(level => 1);

for my $h (0, 1) {
    no warnings 'utf8';
    my $t = $h ? pack('U', 0xFFFF) : "";
    $objBn->change(highestFFFF => 1) if $h;

    ok($objBn->lt("\x{993}$t", "\x{994}"));
    ok($objBn->lt("\x{994}$t", "\x{982}"));
    ok($objBn->lt("\x{982}$t", "\x{983}"));
    ok($objBn->lt("\x{983}$t", "\x{981}"));
    ok($objBn->lt("\x{981}$t", "\x{995}"));
}
