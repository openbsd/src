
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
BEGIN { $| = 1; print "1..32\n"; }
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

my $objDe = Unicode::Collate::Locale->
    new(locale => 'DE', normalization => undef);

ok($objDe->getlocale, 'default');

$objDe->change(level => 1);

ok($objDe->lt("a\x{308}", "ae"));
ok($objDe->lt("A\x{308}", "AE"));
ok($objDe->lt("o\x{308}", "oe"));
ok($objDe->lt("O\x{308}", "OE"));
ok($objDe->lt("u\x{308}", "ue"));
ok($objDe->lt("U\x{308}", "UE"));

# 8

ok($objDe->eq("a\x{308}", "a"));
ok($objDe->eq("A\x{308}", "A"));
ok($objDe->eq("o\x{308}", "o"));
ok($objDe->eq("O\x{308}", "O"));
ok($objDe->eq("u\x{308}", "u"));
ok($objDe->eq("U\x{308}", "U"));

# 14

$objDe->change(level => 2);

ok($objDe->gt("a\x{308}", "a"));
ok($objDe->gt("A\x{308}", "A"));
ok($objDe->gt("o\x{308}", "o"));
ok($objDe->gt("O\x{308}", "O"));
ok($objDe->gt("u\x{308}", "u"));
ok($objDe->gt("U\x{308}", "U"));

ok($objDe->eq("a\x{308}", "A\x{308}"));
ok($objDe->eq("o\x{308}", "O\x{308}"));
ok($objDe->eq("u\x{308}", "U\x{308}"));

# 23

$objDe->change(level => 3);

ok($objDe->lt("a\x{308}", "A\x{308}"));
ok($objDe->lt("o\x{308}", "O\x{308}"));
ok($objDe->lt("u\x{308}", "U\x{308}"));

ok($objDe->eq("a\x{308}", $auml));
ok($objDe->eq("A\x{308}", $Auml));
ok($objDe->eq("o\x{308}", $ouml));
ok($objDe->eq("O\x{308}", $Ouml));
ok($objDe->eq("u\x{308}", $uuml));
ok($objDe->eq("U\x{308}", $Uuml));

# 32
