
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
BEGIN { $| = 1; print "1..91\n"; }
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

my $objUr = Unicode::Collate::Locale->
    new(locale => 'UR', normalization => undef);

ok($objUr->getlocale, 'ur');

$objUr->change(level => 1);

ok($objUr->lt("\x{627}",        "\x{622}"));
ok($objUr->lt("\x{622}",        "\x{628}"));
ok($objUr->lt("\x{628}",        "\x{628}\x{6BE}"));
ok($objUr->lt("\x{628}\x{6BE}", "\x{67E}"));
ok($objUr->lt("\x{67E}",        "\x{67E}\x{6BE}"));
ok($objUr->lt("\x{67E}\x{6BE}", "\x{62A}"));
ok($objUr->lt("\x{62A}",        "\x{62A}\x{6BE}"));
ok($objUr->lt("\x{62A}\x{6BE}", "\x{679}"));
ok($objUr->lt("\x{679}",        "\x{679}\x{6BE}"));
ok($objUr->lt("\x{679}\x{6BE}", "\x{62B}"));
ok($objUr->lt("\x{62B}",        "\x{62C}"));
ok($objUr->lt("\x{62C}",        "\x{62C}\x{6BE}"));
ok($objUr->lt("\x{62C}\x{6BE}", "\x{686}"));
ok($objUr->lt("\x{686}",        "\x{686}\x{6BE}"));
ok($objUr->lt("\x{686}\x{6BE}", "\x{62D}"));
ok($objUr->lt("\x{62D}",        "\x{62E}"));
ok($objUr->lt("\x{62E}",        "\x{62F}"));
ok($objUr->lt("\x{62F}",        "\x{62F}\x{6BE}"));
ok($objUr->lt("\x{62F}\x{6BE}", "\x{688}"));
ok($objUr->lt("\x{688}",        "\x{688}\x{6BE}"));
ok($objUr->lt("\x{688}\x{6BE}", "\x{630}"));
ok($objUr->lt("\x{630}",        "\x{631}"));
ok($objUr->lt("\x{631}",        "\x{631}\x{6BE}"));
ok($objUr->lt("\x{631}\x{6BE}", "\x{691}"));
ok($objUr->lt("\x{691}",        "\x{691}\x{6BE}"));
ok($objUr->lt("\x{691}\x{6BE}", "\x{632}"));
ok($objUr->lt("\x{632}",        "\x{698}"));
ok($objUr->lt("\x{698}",        "\x{633}"));
ok($objUr->lt("\x{633}",        "\x{634}"));
ok($objUr->lt("\x{634}",        "\x{635}"));
ok($objUr->lt("\x{635}",        "\x{636}"));
ok($objUr->lt("\x{636}",        "\x{637}"));
ok($objUr->lt("\x{637}",        "\x{638}"));
ok($objUr->lt("\x{638}",        "\x{639}"));
ok($objUr->lt("\x{639}",        "\x{63A}"));
ok($objUr->lt("\x{63A}",        "\x{641}"));
ok($objUr->lt("\x{641}",        "\x{642}"));
ok($objUr->lt("\x{642}",        "\x{6A9}"));
ok($objUr->lt("\x{6A9}",        "\x{6A9}\x{6BE}"));
ok($objUr->lt("\x{6A9}\x{6BE}", "\x{6AF}"));
ok($objUr->lt("\x{6AF}",        "\x{6AF}\x{6BE}"));
ok($objUr->lt("\x{6AF}\x{6BE}", "\x{644}"));
ok($objUr->lt("\x{644}",        "\x{644}\x{6BE}"));
ok($objUr->lt("\x{644}\x{6BE}", "\x{645}"));
ok($objUr->lt("\x{645}",        "\x{645}\x{6BE}"));
ok($objUr->lt("\x{645}\x{6BE}", "\x{646}"));
ok($objUr->lt("\x{646}",        "\x{646}\x{6BE}"));
ok($objUr->lt("\x{646}\x{6BE}", "\x{6BA}"));
ok($objUr->lt("\x{6BA}",        "\x{6BA}\x{6BE}"));
ok($objUr->lt("\x{6BA}\x{6BE}", "\x{648}"));
ok($objUr->lt("\x{648}",        "\x{648}\x{6BE}"));
ok($objUr->lt("\x{648}\x{6BE}", "\x{6C1}"));
ok($objUr->lt("\x{6C1}",        "\x{6BE}"));
ok($objUr->lt("\x{6BE}",        "\x{6C3}"));
ok($objUr->lt("\x{6C3}",        "\x{621}"));
ok($objUr->lt("\x{621}",        "\x{6CC}"));
ok($objUr->lt("\x{6CC}",        "\x{6CC}\x{6BE}"));
ok($objUr->lt("\x{6CC}\x{6BE}", "\x{6D2}"));
ok($objUr->lt("\x{6D2}",        "\x{67B}"));

# 61

ok($objUr->eq("\x{627}", "\x{623}"));
ok($objUr->eq("\x{648}", "\x{624}"));
ok($objUr->eq("\x{6C1}", "\x{6C2}"));
ok($objUr->eq("\x{6CC}", "\x{626}"));
ok($objUr->eq("\x{6D2}", "\x{6D3}"));

# 66

$objUr->change(level => 2);

ok($objUr->lt("\x{627}", "\x{623}"));
ok($objUr->lt("\x{648}", "\x{624}"));
ok($objUr->lt("\x{6C1}", "\x{6C2}"));
ok($objUr->lt("\x{6CC}", "\x{626}"));
ok($objUr->lt("\x{6D2}", "\x{6D3}"));

# 71

ok($objUr->lt("\x{652}", "\x{64E}"));
ok($objUr->lt("\x{64E}", "\x{650}"));
ok($objUr->lt("\x{650}", "\x{64F}"));
ok($objUr->lt("\x{64F}", "\x{670}"));
ok($objUr->lt("\x{670}", "\x{656}"));
ok($objUr->lt("\x{656}", "\x{657}"));
ok($objUr->lt("\x{657}", "\x{64B}"));
ok($objUr->lt("\x{64B}", "\x{64D}"));
ok($objUr->lt("\x{64D}", "\x{64C}"));
ok($objUr->lt("\x{64C}", "\x{654}"));
ok($objUr->lt("\x{654}", "\x{651}"));
ok($objUr->lt("\x{651}", "\x{658}"));
ok($objUr->lt("\x{658}", "\x{653}"));
ok($objUr->lt("\x{653}", "\x{655}"));

# 85

ok($objUr->eq("\x{623}", "\x{627}\x{654}"));
ok($objUr->eq("\x{622}", "\x{627}\x{653}"));
ok($objUr->eq("\x{624}", "\x{648}\x{654}"));
ok($objUr->eq("\x{6C2}", "\x{6C1}\x{654}"));
ok($objUr->eq("\x{626}", "\x{64A}\x{654}"));
ok($objUr->eq("\x{6D3}", "\x{6D2}\x{654}"));

# 91
