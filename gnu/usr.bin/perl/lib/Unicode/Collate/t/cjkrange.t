
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

use Test;
BEGIN { plan tests => 51 };

use strict;
use warnings;
use Unicode::Collate;

ok(1);

my $Collator = Unicode::Collate->new(
  table => 'keys.txt',
  normalization => undef,
);

# U+9FA6..U+9FBB are CJK UI since Unicode 4.1.0.
# U+3400 is CJK UI ExtA, then greater than any CJK UI.

##### 2..11
ok($Collator->lt("\x{9FA5}", "\x{3400}")); # UI < ExtA
ok($Collator->lt("\x{9FA6}", "\x{3400}")); # new UI < ExtA
ok($Collator->lt("\x{9FBB}", "\x{3400}")); # new UI < ExtA
ok($Collator->gt("\x{9FBC}", "\x{3400}")); # Unassigned > ExtA
ok($Collator->gt("\x{9FFF}", "\x{3400}")); # Unassigned > ExtA
ok($Collator->lt("\x{9FA6}", "\x{9FBB}")); # new UI > new UI
ok($Collator->lt("\x{3400}","\x{20000}")); # ExtA < ExtB
ok($Collator->lt("\x{3400}","\x{2A6D6}")); # ExtA < ExtB
ok($Collator->gt("\x{9FFF}","\x{20000}")); # Unassigned > ExtB
ok($Collator->gt("\x{9FFF}","\x{2A6D6}")); # Unassigned > ExtB

##### 12..21
$Collator->change(UCA_Version => 11);
ok($Collator->lt("\x{9FA5}", "\x{3400}")); # UI < ExtA
ok($Collator->gt("\x{9FA6}", "\x{3400}")); # Unassigned > ExtA
ok($Collator->gt("\x{9FBB}", "\x{3400}")); # Unassigned > ExtA
ok($Collator->gt("\x{9FBC}", "\x{3400}")); # Unassigned > ExtA
ok($Collator->gt("\x{9FFF}", "\x{3400}")); # Unassigned > ExtA
ok($Collator->lt("\x{9FA6}", "\x{9FBB}")); # Unassigned > Unassigned
ok($Collator->lt("\x{3400}","\x{20000}")); # ExtA < ExtB
ok($Collator->lt("\x{3400}","\x{2A6D6}")); # ExtA < ExtB
ok($Collator->gt("\x{9FFF}","\x{20000}")); # Unassigned > ExtB
ok($Collator->gt("\x{9FFF}","\x{2A6D6}")); # Unassigned > ExtB

##### 22..31
$Collator->change(UCA_Version => 9);
ok($Collator->lt("\x{9FA5}", "\x{3400}")); # UI < ExtA
ok($Collator->gt("\x{9FA6}", "\x{3400}")); # Unassigned > ExtA
ok($Collator->gt("\x{9FBB}", "\x{3400}")); # Unassigned > ExtA
ok($Collator->gt("\x{9FBC}", "\x{3400}")); # Unassigned > ExtA
ok($Collator->gt("\x{9FFF}", "\x{3400}")); # Unassigned > ExtA
ok($Collator->lt("\x{9FA6}", "\x{9FBB}")); # Unassigned > Unassigned
ok($Collator->lt("\x{3400}","\x{20000}")); # ExtA < ExtB
ok($Collator->lt("\x{3400}","\x{2A6D6}")); # ExtA < ExtB
ok($Collator->gt("\x{9FFF}","\x{20000}")); # Unassigned > ExtB
ok($Collator->gt("\x{9FFF}","\x{2A6D6}")); # Unassigned > ExtB

##### 32..41
$Collator->change(UCA_Version => 8);
ok($Collator->gt("\x{9FA5}", "\x{3400}")); # UI > ExtA
ok($Collator->gt("\x{9FA6}", "\x{3400}")); # Unassigned > ExtA
ok($Collator->gt("\x{9FBB}", "\x{3400}")); # Unassigned > ExtA
ok($Collator->gt("\x{9FBC}", "\x{3400}")); # Unassigned > ExtA
ok($Collator->gt("\x{9FFF}", "\x{3400}")); # Unassigned > ExtA
ok($Collator->lt("\x{9FA6}", "\x{9FBB}")); # new UI > new UI
ok($Collator->lt("\x{3400}","\x{20000}")); # ExtA < Unassigned(ExtB)
ok($Collator->lt("\x{3400}","\x{2A6D6}")); # ExtA < Unassigned(ExtB)
ok($Collator->lt("\x{9FFF}","\x{20000}")); # Unassigned < Unassigned(ExtB)
ok($Collator->lt("\x{9FFF}","\x{2A6D6}")); # Unassigned < Unassigned(ExtB)

##### 42..51
$Collator->change(UCA_Version => 14);
ok($Collator->lt("\x{9FA5}", "\x{3400}")); # UI < ExtA
ok($Collator->lt("\x{9FA6}", "\x{3400}")); # new UI < ExtA
ok($Collator->lt("\x{9FBB}", "\x{3400}")); # new UI < ExtA
ok($Collator->gt("\x{9FBC}", "\x{3400}")); # Unassigned > ExtA
ok($Collator->gt("\x{9FFF}", "\x{3400}")); # Unassigned > ExtA
ok($Collator->lt("\x{9FA6}", "\x{9FBB}")); # new UI > new UI
ok($Collator->lt("\x{3400}","\x{20000}")); # ExtA < ExtB
ok($Collator->lt("\x{3400}","\x{2A6D6}")); # ExtA < ExtB
ok($Collator->gt("\x{9FFF}","\x{20000}")); # Unassigned > ExtB
ok($Collator->gt("\x{9FFF}","\x{2A6D6}")); # Unassigned > ExtB

