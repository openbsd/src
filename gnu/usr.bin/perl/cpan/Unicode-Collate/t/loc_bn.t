
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
BEGIN { $| = 1; print "1..6\n"; }
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

ok($objBn->lt("\x{994}", "\x{982}"));
ok($objBn->lt("\x{982}", "\x{983}"));
ok($objBn->lt("\x{983}", "\x{981}"));
ok($objBn->lt("\x{981}", "\x{995}"));

