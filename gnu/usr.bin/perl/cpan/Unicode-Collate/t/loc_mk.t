
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
BEGIN { $| = 1; print "1..202\n"; }
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

my $objMk = Unicode::Collate::Locale->
    new(locale => 'MK', normalization => undef);

ok($objMk->getlocale, 'mk');

$objMk->change(level => 1);

ok($objMk->gt("\x{453}", "\x{433}")); # not suppressed
ok($objMk->gt("\x{403}", "\x{413}")); # not suppressed
ok($objMk->gt("\x{45C}", "\x{43A}")); # not suppressed
ok($objMk->gt("\x{40C}", "\x{41A}")); # not suppressed

# 6

ok($objMk->eq("\x{4D1}", "\x{430}"));
ok($objMk->eq("\x{4D0}", "\x{410}"));
ok($objMk->eq("\x{4D3}", "\x{430}"));
ok($objMk->eq("\x{4D2}", "\x{410}"));
ok($objMk->eq("\x{4DB}", "\x{4D9}"));
ok($objMk->eq("\x{4DA}", "\x{4D8}"));
ok($objMk->eq("\x{450}", "\x{435}")); # not contraction
ok($objMk->eq("\x{400}", "\x{415}")); # not contraction
ok($objMk->eq("\x{451}", "\x{435}")); # not contraction
ok($objMk->eq("\x{401}", "\x{415}")); # not contraction
ok($objMk->eq("\x{4D7}", "\x{435}"));
ok($objMk->eq("\x{4D6}", "\x{415}"));
ok($objMk->eq("\x{4C2}", "\x{436}")); # not contraction
ok($objMk->eq("\x{4C1}", "\x{416}")); # not contraction
ok($objMk->eq("\x{4DD}", "\x{436}"));
ok($objMk->eq("\x{4DC}", "\x{416}"));
ok($objMk->eq("\x{4DF}", "\x{437}"));
ok($objMk->eq("\x{4DE}", "\x{417}"));
ok($objMk->eq("\x{45D}", "\x{438}")); # not contraction
ok($objMk->eq("\x{40D}", "\x{418}")); # not contraction
ok($objMk->eq("\x{4E3}", "\x{438}")); # not contraction
ok($objMk->eq("\x{4E2}", "\x{418}")); # not contraction
ok($objMk->eq("\x{4E5}", "\x{438}"));
ok($objMk->eq("\x{4E4}", "\x{418}"));
ok($objMk->eq("\x{457}", "\x{456}"));
ok($objMk->eq("\x{407}", "\x{406}"));
ok($objMk->eq("\x{439}", "\x{438}"));
ok($objMk->eq("\x{419}", "\x{418}"));
ok($objMk->eq("\x{4E7}", "\x{43E}"));
ok($objMk->eq("\x{4E6}", "\x{41E}"));
ok($objMk->eq("\x{4EB}", "\x{4E9}"));
ok($objMk->eq("\x{4EA}", "\x{4E8}"));
ok($objMk->eq("\x{4EF}", "\x{443}")); # not contraction
ok($objMk->eq("\x{4EE}", "\x{423}")); # not contraction
ok($objMk->eq("\x{45E}", "\x{443}"));
ok($objMk->eq("\x{40E}", "\x{423}"));
ok($objMk->eq("\x{4F1}", "\x{443}"));
ok($objMk->eq("\x{4F0}", "\x{423}"));
ok($objMk->eq("\x{4F3}", "\x{443}"));
ok($objMk->eq("\x{4F2}", "\x{423}"));
ok($objMk->eq("\x{4F5}", "\x{447}"));
ok($objMk->eq("\x{4F4}", "\x{427}"));
ok($objMk->eq("\x{4F9}", "\x{44B}"));
ok($objMk->eq("\x{4F8}", "\x{42B}"));
ok($objMk->eq("\x{4ED}", "\x{44D}"));
ok($objMk->eq("\x{4EC}", "\x{42D}"));
ok($objMk->eq("\x{477}", "\x{475}"));
ok($objMk->eq("\x{476}", "\x{474}"));

# 54

$objMk->change(level => 2);

ok($objMk->gt("\x{4D1}", "\x{430}"));
ok($objMk->gt("\x{4D0}", "\x{410}"));
ok($objMk->gt("\x{4D3}", "\x{430}"));
ok($objMk->gt("\x{4D2}", "\x{410}"));
ok($objMk->gt("\x{4DB}", "\x{4D9}"));
ok($objMk->gt("\x{4DA}", "\x{4D8}"));
ok($objMk->gt("\x{450}", "\x{435}")); # not contraction
ok($objMk->gt("\x{400}", "\x{415}")); # not contraction
ok($objMk->gt("\x{451}", "\x{435}")); # not contraction
ok($objMk->gt("\x{401}", "\x{415}")); # not contraction
ok($objMk->gt("\x{4D7}", "\x{435}"));
ok($objMk->gt("\x{4D6}", "\x{415}"));
ok($objMk->gt("\x{4C2}", "\x{436}")); # not contraction
ok($objMk->gt("\x{4C1}", "\x{416}")); # not contraction
ok($objMk->gt("\x{4DD}", "\x{436}"));
ok($objMk->gt("\x{4DC}", "\x{416}"));
ok($objMk->gt("\x{4DF}", "\x{437}"));
ok($objMk->gt("\x{4DE}", "\x{417}"));
ok($objMk->gt("\x{45D}", "\x{438}")); # not contraction
ok($objMk->gt("\x{40D}", "\x{418}")); # not contraction
ok($objMk->gt("\x{4E3}", "\x{438}")); # not contraction
ok($objMk->gt("\x{4E2}", "\x{418}")); # not contraction
ok($objMk->gt("\x{4E5}", "\x{438}"));
ok($objMk->gt("\x{4E4}", "\x{418}"));
ok($objMk->gt("\x{457}", "\x{456}"));
ok($objMk->gt("\x{407}", "\x{406}"));
ok($objMk->gt("\x{439}", "\x{438}"));
ok($objMk->gt("\x{419}", "\x{418}"));
ok($objMk->gt("\x{4E7}", "\x{43E}"));
ok($objMk->gt("\x{4E6}", "\x{41E}"));
ok($objMk->gt("\x{4EB}", "\x{4E9}"));
ok($objMk->gt("\x{4EA}", "\x{4E8}"));
ok($objMk->gt("\x{4EF}", "\x{443}")); # not contraction
ok($objMk->gt("\x{4EE}", "\x{423}")); # not contraction
ok($objMk->gt("\x{45E}", "\x{443}"));
ok($objMk->gt("\x{40E}", "\x{423}"));
ok($objMk->gt("\x{4F1}", "\x{443}"));
ok($objMk->gt("\x{4F0}", "\x{423}"));
ok($objMk->gt("\x{4F3}", "\x{443}"));
ok($objMk->gt("\x{4F2}", "\x{423}"));
ok($objMk->gt("\x{4F5}", "\x{447}"));
ok($objMk->gt("\x{4F4}", "\x{427}"));
ok($objMk->gt("\x{4F9}", "\x{44B}"));
ok($objMk->gt("\x{4F8}", "\x{42B}"));
ok($objMk->gt("\x{4ED}", "\x{44D}"));
ok($objMk->gt("\x{4EC}", "\x{42D}"));
ok($objMk->gt("\x{477}", "\x{475}"));
ok($objMk->gt("\x{476}", "\x{474}"));

# 102

$objMk->change(level => 3);

ok($objMk->eq("\x{453}", "\x{433}\x{301}")); # not suppressed
ok($objMk->eq("\x{403}", "\x{413}\x{301}")); # not suppressed
ok($objMk->eq("\x{45C}", "\x{43A}\x{301}")); # not suppressed
ok($objMk->eq("\x{40C}", "\x{41A}\x{301}")); # not suppressed

# 106

for my $i ("", "\0") {
  ok($objMk->eq("\x{4D1}", "\x{430}$i\x{306}"));
  ok($objMk->eq("\x{4D0}", "\x{410}$i\x{306}"));
  ok($objMk->eq("\x{4D3}", "\x{430}$i\x{308}"));
  ok($objMk->eq("\x{4D2}", "\x{410}$i\x{308}"));
  ok($objMk->eq("\x{4DB}", "\x{4D9}$i\x{308}"));
  ok($objMk->eq("\x{4DA}", "\x{4D8}$i\x{308}"));
  ok($objMk->eq("\x{450}", "\x{435}$i\x{300}")); # not contraction
  ok($objMk->eq("\x{400}", "\x{415}$i\x{300}")); # not contraction
  ok($objMk->eq("\x{451}", "\x{435}$i\x{308}")); # not contraction
  ok($objMk->eq("\x{401}", "\x{415}$i\x{308}")); # not contraction
  ok($objMk->eq("\x{4D7}", "\x{435}$i\x{306}"));
  ok($objMk->eq("\x{4D6}", "\x{415}$i\x{306}"));
  ok($objMk->eq("\x{4C2}", "\x{436}$i\x{306}")); # not contraction
  ok($objMk->eq("\x{4C1}", "\x{416}$i\x{306}")); # not contraction
  ok($objMk->eq("\x{4DD}", "\x{436}$i\x{308}"));
  ok($objMk->eq("\x{4DC}", "\x{416}$i\x{308}"));
  ok($objMk->eq("\x{4DF}", "\x{437}$i\x{308}"));
  ok($objMk->eq("\x{4DE}", "\x{417}$i\x{308}"));
  ok($objMk->eq("\x{45D}", "\x{438}$i\x{300}")); # not contraction
  ok($objMk->eq("\x{40D}", "\x{418}$i\x{300}")); # not contraction
  ok($objMk->eq("\x{4E3}", "\x{438}$i\x{304}")); # not contraction
  ok($objMk->eq("\x{4E2}", "\x{418}$i\x{304}")); # not contraction
  ok($objMk->eq("\x{4E5}", "\x{438}$i\x{308}"));
  ok($objMk->eq("\x{4E4}", "\x{418}$i\x{308}"));
  ok($objMk->eq("\x{457}", "\x{456}$i\x{308}"));
  ok($objMk->eq("\x{407}", "\x{406}$i\x{308}"));
  ok($objMk->eq("\x{439}", "\x{438}$i\x{306}"));
  ok($objMk->eq("\x{419}", "\x{418}$i\x{306}"));
  ok($objMk->eq("\x{4E7}", "\x{43E}$i\x{308}"));
  ok($objMk->eq("\x{4E6}", "\x{41E}$i\x{308}"));
  ok($objMk->eq("\x{4EB}", "\x{4E9}$i\x{308}"));
  ok($objMk->eq("\x{4EA}", "\x{4E8}$i\x{308}"));
  ok($objMk->eq("\x{4EF}", "\x{443}$i\x{304}")); # not contraction
  ok($objMk->eq("\x{4EE}", "\x{423}$i\x{304}")); # not contraction
  ok($objMk->eq("\x{45E}", "\x{443}$i\x{306}"));
  ok($objMk->eq("\x{40E}", "\x{423}$i\x{306}"));
  ok($objMk->eq("\x{4F1}", "\x{443}$i\x{308}"));
  ok($objMk->eq("\x{4F0}", "\x{423}$i\x{308}"));
  ok($objMk->eq("\x{4F3}", "\x{443}$i\x{30B}"));
  ok($objMk->eq("\x{4F2}", "\x{423}$i\x{30B}"));
  ok($objMk->eq("\x{4F5}", "\x{447}$i\x{308}"));
  ok($objMk->eq("\x{4F4}", "\x{427}$i\x{308}"));
  ok($objMk->eq("\x{4F9}", "\x{44B}$i\x{308}"));
  ok($objMk->eq("\x{4F8}", "\x{42B}$i\x{308}"));
  ok($objMk->eq("\x{4ED}", "\x{44D}$i\x{308}"));
  ok($objMk->eq("\x{4EC}", "\x{42D}$i\x{308}"));
  ok($objMk->eq("\x{477}", "\x{475}$i\x{30F}"));
  ok($objMk->eq("\x{476}", "\x{474}$i\x{30F}"));
}

# 202
