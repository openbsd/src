
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

my $objSi = Unicode::Collate::Locale->
    new(locale => 'SI', normalization => undef);

ok($objSi->getlocale, 'si');

$objSi->change(level => 1);

ok($objSi->lt("\x{D96}", "\x{D82}"));
ok($objSi->lt("\x{D82}", "\x{D83}"));
ok($objSi->lt("\x{D83}", "\x{D9A}"));

ok($objSi->lt("\x{DA3}", "\x{DA5}"));
ok($objSi->lt("\x{DA5}", "\x{DA4}"));
ok($objSi->lt("\x{DA4}", "\x{DA6}"));

