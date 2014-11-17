
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
BEGIN { $| = 1; print "1..130\n"; }
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

my $objNoSuppress = Unicode::Collate::Locale->
    new(locale => 'NoSuppress', normalization => undef);

ok($objNoSuppress->getlocale, 'default');

$objNoSuppress->change(level => 1);

ok($objNoSuppress->gt("\x{4D1}", "\x{430}")); # not suppressed
ok($objNoSuppress->gt("\x{4D0}", "\x{410}")); # not suppressed
ok($objNoSuppress->gt("\x{4D3}", "\x{430}")); # not suppressed
ok($objNoSuppress->gt("\x{4D2}", "\x{410}")); # not suppressed
ok($objNoSuppress->gt("\x{4DB}", "\x{4D9}")); # not suppressed
ok($objNoSuppress->gt("\x{4DA}", "\x{4D8}")); # not suppressed
ok($objNoSuppress->gt("\x{453}", "\x{433}")); # not suppressed
ok($objNoSuppress->gt("\x{403}", "\x{413}")); # not suppressed
ok($objNoSuppress->gt("\x{4D7}", "\x{435}")); # not suppressed
ok($objNoSuppress->gt("\x{4D6}", "\x{415}")); # not suppressed
ok($objNoSuppress->gt("\x{4DD}", "\x{436}")); # not suppressed
ok($objNoSuppress->gt("\x{4DC}", "\x{416}")); # not suppressed
ok($objNoSuppress->gt("\x{4DF}", "\x{437}")); # not suppressed
ok($objNoSuppress->gt("\x{4DE}", "\x{417}")); # not suppressed
ok($objNoSuppress->gt("\x{4E5}", "\x{438}")); # not suppressed
ok($objNoSuppress->gt("\x{4E4}", "\x{418}")); # not suppressed
ok($objNoSuppress->gt("\x{457}", "\x{456}")); # not suppressed
ok($objNoSuppress->gt("\x{407}", "\x{406}")); # not suppressed
ok($objNoSuppress->gt("\x{439}", "\x{438}")); # not suppressed
ok($objNoSuppress->gt("\x{419}", "\x{418}")); # not suppressed
ok($objNoSuppress->gt("\x{4E7}", "\x{43E}")); # not suppressed
ok($objNoSuppress->gt("\x{4E6}", "\x{41E}")); # not suppressed
ok($objNoSuppress->gt("\x{4EB}", "\x{4E9}")); # not suppressed
ok($objNoSuppress->gt("\x{4EA}", "\x{4E8}")); # not suppressed
ok($objNoSuppress->gt("\x{45C}", "\x{43A}")); # not suppressed
ok($objNoSuppress->gt("\x{40C}", "\x{41A}")); # not suppressed
ok($objNoSuppress->gt("\x{45E}", "\x{443}")); # not suppressed
ok($objNoSuppress->gt("\x{40E}", "\x{423}")); # not suppressed
ok($objNoSuppress->gt("\x{4F1}", "\x{443}")); # not suppressed
ok($objNoSuppress->gt("\x{4F0}", "\x{423}")); # not suppressed
ok($objNoSuppress->gt("\x{4F3}", "\x{443}")); # not suppressed
ok($objNoSuppress->gt("\x{4F2}", "\x{423}")); # not suppressed
ok($objNoSuppress->gt("\x{4F5}", "\x{447}")); # not suppressed
ok($objNoSuppress->gt("\x{4F4}", "\x{427}")); # not suppressed
ok($objNoSuppress->gt("\x{4F9}", "\x{44B}")); # not suppressed
ok($objNoSuppress->gt("\x{4F8}", "\x{42B}")); # not suppressed
ok($objNoSuppress->gt("\x{4ED}", "\x{44D}")); # not suppressed
ok($objNoSuppress->gt("\x{4EC}", "\x{42D}")); # not suppressed
ok($objNoSuppress->gt("\x{477}", "\x{475}")); # not suppressed
ok($objNoSuppress->gt("\x{476}", "\x{474}")); # not suppressed

# 42

ok($objNoSuppress->eq("\x{450}", "\x{435}")); # not contraction
ok($objNoSuppress->eq("\x{400}", "\x{415}")); # not contraction
ok($objNoSuppress->eq("\x{451}", "\x{435}")); # not contraction
ok($objNoSuppress->eq("\x{401}", "\x{415}")); # not contraction
ok($objNoSuppress->eq("\x{4C2}", "\x{436}")); # not contraction
ok($objNoSuppress->eq("\x{4C1}", "\x{416}")); # not contraction
ok($objNoSuppress->eq("\x{45D}", "\x{438}")); # not contraction
ok($objNoSuppress->eq("\x{40D}", "\x{418}")); # not contraction
ok($objNoSuppress->eq("\x{4E3}", "\x{438}")); # not contraction
ok($objNoSuppress->eq("\x{4E2}", "\x{418}")); # not contraction
ok($objNoSuppress->eq("\x{4EF}", "\x{443}")); # not contraction
ok($objNoSuppress->eq("\x{4EE}", "\x{423}")); # not contraction

# 54

$objNoSuppress->change(level => 2);

ok($objNoSuppress->gt("\x{450}", "\x{435}")); # not contraction
ok($objNoSuppress->gt("\x{400}", "\x{415}")); # not contraction
ok($objNoSuppress->gt("\x{451}", "\x{435}")); # not contraction
ok($objNoSuppress->gt("\x{401}", "\x{415}")); # not contraction
ok($objNoSuppress->gt("\x{4C2}", "\x{436}")); # not contraction
ok($objNoSuppress->gt("\x{4C1}", "\x{416}")); # not contraction
ok($objNoSuppress->gt("\x{45D}", "\x{438}")); # not contraction
ok($objNoSuppress->gt("\x{40D}", "\x{418}")); # not contraction
ok($objNoSuppress->gt("\x{4E3}", "\x{438}")); # not contraction
ok($objNoSuppress->gt("\x{4E2}", "\x{418}")); # not contraction
ok($objNoSuppress->gt("\x{4EF}", "\x{443}")); # not contraction
ok($objNoSuppress->gt("\x{4EE}", "\x{423}")); # not contraction

# 66

$objNoSuppress->change(level => 3);

ok($objNoSuppress->eq("\x{4D1}", "\x{430}\x{306}")); # not suppressed
ok($objNoSuppress->eq("\x{4D0}", "\x{410}\x{306}")); # not suppressed
ok($objNoSuppress->eq("\x{4D3}", "\x{430}\x{308}")); # not suppressed
ok($objNoSuppress->eq("\x{4D2}", "\x{410}\x{308}")); # not suppressed
ok($objNoSuppress->eq("\x{4DB}", "\x{4D9}\x{308}")); # not suppressed
ok($objNoSuppress->eq("\x{4DA}", "\x{4D8}\x{308}")); # not suppressed
ok($objNoSuppress->eq("\x{453}", "\x{433}\x{301}")); # not suppressed
ok($objNoSuppress->eq("\x{403}", "\x{413}\x{301}")); # not suppressed
ok($objNoSuppress->eq("\x{4D7}", "\x{435}\x{306}")); # not suppressed
ok($objNoSuppress->eq("\x{4D6}", "\x{415}\x{306}")); # not suppressed
ok($objNoSuppress->eq("\x{4DD}", "\x{436}\x{308}")); # not suppressed
ok($objNoSuppress->eq("\x{4DC}", "\x{416}\x{308}")); # not suppressed
ok($objNoSuppress->eq("\x{4DF}", "\x{437}\x{308}")); # not suppressed
ok($objNoSuppress->eq("\x{4DE}", "\x{417}\x{308}")); # not suppressed
ok($objNoSuppress->eq("\x{4E5}", "\x{438}\x{308}")); # not suppressed
ok($objNoSuppress->eq("\x{4E4}", "\x{418}\x{308}")); # not suppressed
ok($objNoSuppress->eq("\x{457}", "\x{456}\x{308}")); # not suppressed
ok($objNoSuppress->eq("\x{407}", "\x{406}\x{308}")); # not suppressed
ok($objNoSuppress->eq("\x{439}", "\x{438}\x{306}")); # not suppressed
ok($objNoSuppress->eq("\x{419}", "\x{418}\x{306}")); # not suppressed
ok($objNoSuppress->eq("\x{4E7}", "\x{43E}\x{308}")); # not suppressed
ok($objNoSuppress->eq("\x{4E6}", "\x{41E}\x{308}")); # not suppressed
ok($objNoSuppress->eq("\x{4EB}", "\x{4E9}\x{308}")); # not suppressed
ok($objNoSuppress->eq("\x{4EA}", "\x{4E8}\x{308}")); # not suppressed
ok($objNoSuppress->eq("\x{45C}", "\x{43A}\x{301}")); # not suppressed
ok($objNoSuppress->eq("\x{40C}", "\x{41A}\x{301}")); # not suppressed
ok($objNoSuppress->eq("\x{45E}", "\x{443}\x{306}")); # not suppressed
ok($objNoSuppress->eq("\x{40E}", "\x{423}\x{306}")); # not suppressed
ok($objNoSuppress->eq("\x{4F1}", "\x{443}\x{308}")); # not suppressed
ok($objNoSuppress->eq("\x{4F0}", "\x{423}\x{308}")); # not suppressed
ok($objNoSuppress->eq("\x{4F3}", "\x{443}\x{30B}")); # not suppressed
ok($objNoSuppress->eq("\x{4F2}", "\x{423}\x{30B}")); # not suppressed
ok($objNoSuppress->eq("\x{4F5}", "\x{447}\x{308}")); # not suppressed
ok($objNoSuppress->eq("\x{4F4}", "\x{427}\x{308}")); # not suppressed
ok($objNoSuppress->eq("\x{4F9}", "\x{44B}\x{308}")); # not suppressed
ok($objNoSuppress->eq("\x{4F8}", "\x{42B}\x{308}")); # not suppressed
ok($objNoSuppress->eq("\x{4ED}", "\x{44D}\x{308}")); # not suppressed
ok($objNoSuppress->eq("\x{4EC}", "\x{42D}\x{308}")); # not suppressed
ok($objNoSuppress->eq("\x{477}", "\x{475}\x{30F}")); # not suppressed
ok($objNoSuppress->eq("\x{476}", "\x{474}\x{30F}")); # not suppressed

# 106

for my $i ("", "\0") {
  ok($objNoSuppress->eq("\x{450}", "\x{435}$i\x{300}")); # not contraction
  ok($objNoSuppress->eq("\x{400}", "\x{415}$i\x{300}")); # not contraction
  ok($objNoSuppress->eq("\x{451}", "\x{435}$i\x{308}")); # not contraction
  ok($objNoSuppress->eq("\x{401}", "\x{415}$i\x{308}")); # not contraction
  ok($objNoSuppress->eq("\x{4C2}", "\x{436}$i\x{306}")); # not contraction
  ok($objNoSuppress->eq("\x{4C1}", "\x{416}$i\x{306}")); # not contraction
  ok($objNoSuppress->eq("\x{45D}", "\x{438}$i\x{300}")); # not contraction
  ok($objNoSuppress->eq("\x{40D}", "\x{418}$i\x{300}")); # not contraction
  ok($objNoSuppress->eq("\x{4E3}", "\x{438}$i\x{304}")); # not contraction
  ok($objNoSuppress->eq("\x{4E2}", "\x{418}$i\x{304}")); # not contraction
  ok($objNoSuppress->eq("\x{4EF}", "\x{443}$i\x{304}")); # not contraction
  ok($objNoSuppress->eq("\x{4EE}", "\x{423}$i\x{304}")); # not contraction
}

# 130
