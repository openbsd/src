
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
BEGIN { $| = 1; print "1..27\n"; }
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

my $objEsTrad = Unicode::Collate::Locale->
    new(locale => 'ES-trad', normalization => undef);

ok($objEsTrad->getlocale, 'es__traditional');
ok($objEsTrad->locale_version, 1.01);

$objEsTrad->change(level => 1);

ok($objEsTrad->lt("c", "ch"));
ok($objEsTrad->lt("cz","ch"));
ok($objEsTrad->gt("d", "ch"));
ok($objEsTrad->lt("l", "ll"));
ok($objEsTrad->lt("lz","ll"));
ok($objEsTrad->gt("m", "ll"));
ok($objEsTrad->lt("n", "n\x{303}"));
ok($objEsTrad->gt("o", "n\x{303}"));

# 11

ok($objEsTrad->eq("a\x{300}a", "aa\x{300}"));

$objEsTrad->change(level => 2);

ok($objEsTrad->gt("a\x{300}a", "aa\x{300}"));
ok($objEsTrad->lt("Ca\x{300}ca\x{302}", "ca\x{302}ca\x{300}"));
ok($objEsTrad->lt("ca\x{300}ca\x{302}", "Ca\x{302}ca\x{300}"));

# 15

ok($objEsTrad->eq("ch", "Ch"));
ok($objEsTrad->eq("Ch", "CH"));
ok($objEsTrad->eq("ll", "Ll"));
ok($objEsTrad->eq("Ll", "LL"));
ok($objEsTrad->eq("n\x{303}", "N\x{303}"));

# 20

$objEsTrad->change(level => 3);

ok($objEsTrad->lt("ch", "Ch"));
ok($objEsTrad->lt("Ch", "CH"));
ok($objEsTrad->lt("ll", "Ll"));
ok($objEsTrad->lt("Ll", "LL"));
ok($objEsTrad->lt("n\x{303}", "N\x{303}"));
ok($objEsTrad->eq("n\x{303}", pack('U', 0xF1)));
ok($objEsTrad->eq("N\x{303}", pack('U', 0xD1)));

# 27
