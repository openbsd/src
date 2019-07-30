
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
BEGIN { $| = 1; print "1..40\n"; }
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

my $objWo = Unicode::Collate::Locale->
    new(locale => 'WO', normalization => undef);

ok($objWo->getlocale, 'wo');

$objWo->change(level => 1);

ok($objWo->lt("a", "a\x{300}"));
ok($objWo->gt("b", "a\x{300}"));
ok($objWo->lt("e", "e\x{301}"));
ok($objWo->lt("e\x{301}", "e\x{308}"));
ok($objWo->gt("f", "e\x{308}"));
ok($objWo->lt("n", "n\x{303}"));
ok($objWo->lt("n\x{303}", "\x{14B}"));
ok($objWo->gt("o", "\x{14B}"));
ok($objWo->lt("o", "o\x{301}"));
ok($objWo->gt("p", "o\x{301}"));

# 12

$objWo->change(level => 2);

ok($objWo->eq("a\x{300}", "A\x{300}"));
ok($objWo->eq("e\x{301}", "E\x{301}"));
ok($objWo->eq("e\x{308}", "E\x{308}"));
ok($objWo->eq("n\x{303}", "N\x{303}"));
ok($objWo->eq( "\x{14B}",  "\x{14A}"));
ok($objWo->eq("o\x{301}", "O\x{301}"));

# 18

$objWo->change(level => 3);

ok($objWo->lt("a\x{300}", "A\x{300}"));
ok($objWo->lt("e\x{301}", "E\x{301}"));
ok($objWo->lt("e\x{308}", "E\x{308}"));
ok($objWo->lt("n\x{303}", "N\x{303}"));
ok($objWo->lt( "\x{14B}",  "\x{14A}"));
ok($objWo->lt("o\x{301}", "O\x{301}"));

# 24

ok($objWo->eq("a\x{300}", pack('U', 0xE0)));
ok($objWo->eq("a\x{340}", pack('U', 0xE0)));
ok($objWo->eq("A\x{300}", pack('U', 0xC0)));
ok($objWo->eq("A\x{340}", pack('U', 0xC0)));
ok($objWo->eq("e\x{301}", pack('U', 0xE9)));
ok($objWo->eq("e\x{341}", pack('U', 0xE9)));
ok($objWo->eq("E\x{301}", pack('U', 0xC9)));
ok($objWo->eq("E\x{341}", pack('U', 0xC9)));
ok($objWo->eq("e\x{308}", pack('U', 0xEB)));
ok($objWo->eq("E\x{308}", pack('U', 0xCB)));
ok($objWo->eq("n\x{303}", pack('U', 0xF1)));
ok($objWo->eq("N\x{303}", pack('U', 0xD1)));
ok($objWo->eq("o\x{301}", pack('U', 0xF3)));
ok($objWo->eq("o\x{341}", pack('U', 0xF3)));
ok($objWo->eq("O\x{301}", pack('U', 0xD3)));
ok($objWo->eq("O\x{341}", pack('U', 0xD3)));

# 40
