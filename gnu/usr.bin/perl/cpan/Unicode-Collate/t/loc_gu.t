
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
BEGIN { $| = 1; print "1..7\n"; }
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

$objGu->change(level => 1);

ok($objGu->lt("\x{AD0}", "\x{A82}"));
ok($objGu->lt("\x{A82}", "\x{A83}"));
ok($objGu->lt("\x{A83}", "\x{A85}"));

ok($objGu->eq("\x{A82}", "\x{A81}"));

$objGu->change(level => 2);

ok($objGu->lt("\x{A82}", "\x{A81}"));

