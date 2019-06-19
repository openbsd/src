
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
BEGIN { $| = 1; print "1..38\n"; }
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

my $auml = pack 'U', 0xE4;
my $Auml = pack 'U', 0xC4;
my $ouml = pack 'U', 0xF6;
my $Ouml = pack 'U', 0xD6;
my $uuml = pack 'U', 0xFC;
my $Uuml = pack 'U', 0xDC;

my $objVo = Unicode::Collate::Locale->
    new(locale => 'VO', normalization => undef);

ok($objVo->getlocale, 'vo');

$objVo->change(level => 1);

ok($objVo->gt($auml, "az"));
ok($objVo->lt($auml, "b"));
ok($objVo->gt($ouml, "oz"));
ok($objVo->lt($ouml, "p"));
ok($objVo->gt($uuml, "uz"));
ok($objVo->lt($uuml, "v"));

# 8

$objVo->change(level => 2);

ok($objVo->eq("a\x{308}", "A\x{308}"));
ok($objVo->eq("o\x{308}", "O\x{308}"));
ok($objVo->eq("u\x{308}", "U\x{308}"));

ok($objVo->eq($auml, $Auml));
ok($objVo->eq($ouml, $Ouml));
ok($objVo->eq($uuml, $Uuml));

# 14

$objVo->change(level => 3);

ok($objVo->lt("a\x{308}", "A\x{308}"));
ok($objVo->lt("o\x{308}", "O\x{308}"));
ok($objVo->lt("u\x{308}", "U\x{308}"));

ok($objVo->lt($auml, $Auml));
ok($objVo->lt($ouml, $Ouml));
ok($objVo->lt($uuml, $Uuml));

# 20

ok($objVo->eq("a\x{308}", $auml));
ok($objVo->eq("A\x{308}", $Auml));
ok($objVo->eq("o\x{308}", $ouml));
ok($objVo->eq("O\x{308}", $Ouml));
ok($objVo->eq("u\x{308}", $uuml));
ok($objVo->eq("U\x{308}", $Uuml));

# 26

ok($objVo->eq("a\x{308}\x{304}", "\x{1DF}"));
ok($objVo->eq("A\x{308}\x{304}", "\x{1DE}"));
ok($objVo->eq("o\x{308}\x{304}", "\x{22B}"));
ok($objVo->eq("O\x{308}\x{304}", "\x{22A}"));
ok($objVo->eq("u\x{308}\x{300}", "\x{1DC}"));
ok($objVo->eq("U\x{308}\x{300}", "\x{1DB}"));
ok($objVo->eq("u\x{308}\x{301}", "\x{1D8}"));
ok($objVo->eq("U\x{308}\x{301}", "\x{1D7}"));
ok($objVo->eq("u\x{308}\x{304}", "\x{1D6}"));
ok($objVo->eq("U\x{308}\x{304}", "\x{1D5}"));
ok($objVo->eq("u\x{308}\x{30C}", "\x{1DA}"));
ok($objVo->eq("U\x{308}\x{30C}", "\x{1D9}"));

# 38
