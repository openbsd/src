
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

my $objAs = Unicode::Collate::Locale->
    new(locale => 'AS', normalization => undef);

ok($objAs->getlocale, 'as');

$objAs->change(level => 1);

ok($objAs->lt("\x{994}", "\x{982}"));
ok($objAs->lt("\x{982}", "\x{981}"));
ok($objAs->lt("\x{981}", "\x{983}"));
ok($objAs->lt("\x{983}", "\x{995}"));

ok($objAs->lt("\x{9A3}","\x{9A4}\x{9CD}\x{200D}"));
ok($objAs->lt("\x{9A4}\x{9CD}\x{200D}","\x{9A4}"));

ok($objAs->lt("\x{9B9}", "\x{995}\x{9CD}\x{9B7}"));
ok($objAs->lt("\x{995}\x{9CD}\x{9B7}", "\x{9BD}"));
