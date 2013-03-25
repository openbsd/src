
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

my $objOr = Unicode::Collate::Locale->
    new(locale => 'OR', normalization => undef);

ok($objOr->getlocale, 'or');

$objOr->change(level => 1);

ok($objOr->lt("\x{B14}", "\x{B01}"));
ok($objOr->lt("\x{B01}", "\x{B02}"));
ok($objOr->lt("\x{B02}", "\x{B03}"));
ok($objOr->lt("\x{B03}", "\x{B15}"));

ok($objOr->lt("\x{B39}", "\x{B15}\x{B4D}\x{B37}"));
ok($objOr->gt("\x{B3D}", "\x{B15}\x{B4D}\x{B37}"));

ok($objOr->eq("\x{B2F}", "\x{B5F}"));

$objOr->change(level => 2);

ok($objOr->lt("\x{B2F}", "\x{B5F}"));

