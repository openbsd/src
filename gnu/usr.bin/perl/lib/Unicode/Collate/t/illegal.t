
BEGIN {
    unless ("A" eq pack('U', 0x41)) {
	print "1..0 # Unicode::Collate " .
	    "cannot stringify a Unicode code point\n";
	exit 0;
    }
}

BEGIN {
    if ($ENV{PERL_CORE}) {
        chdir('t') if -d 't';
        @INC = $^O eq 'MacOS' ? qw(::lib) : qw(../lib);
    }
}

use Test;
use strict;
use warnings;

BEGIN {
    use Unicode::Collate;

    unless (exists &Unicode::Collate::bootstrap or 5.008 <= $]) {
	print "1..0 # skipped: XSUB, or Perl 5.8.0 or later".
		" needed for this test\n";
	print $@;
	exit;
    }
}

BEGIN { plan tests => 40 };

ok(1);

#########################

no warnings 'utf8';

# NULL is tailorable but illegal code points are not.
# illegal code points should be always ingored
# (cf. UCA, 7.1.1 Illegal code points).

my $illeg = Unicode::Collate->new(
  entry => <<'ENTRIES',
0000  ; [.0020.0000.0000.0000] # [0000] NULL
0001  ; [.0021.0000.0000.0001] # [0001] START OF HEADING
FFFE  ; [.0022.0000.0000.FFFE] # <noncharacter-FFFE> (invalid)
FFFF  ; [.0023.0000.0000.FFFF] # <noncharacter-FFFF> (invalid)
D800  ; [.0024.0000.0000.D800] # <surrogate-D800> (invalid)
DFFF  ; [.0025.0000.0000.DFFF] # <surrogate-DFFF> (invalid)
FDD0  ; [.0026.0000.0000.FDD0] # <noncharacter-FDD0> (invalid)
FDEF  ; [.0027.0000.0000.FDEF] # <noncharacter-FDEF> (invalid)
0002  ; [.0030.0000.0000.0002] # [0002] START OF TEXT
10FFFF; [.0040.0000.0000.10FFFF] # <noncharacter-10FFFF> (invalid)
110000; [.0041.0000.0000.110000] # <out-of-range 110000> (invalid)
0041  ; [.1000.0020.0008.0041] # latin A
0041 0000 ; [.1100.0020.0008.0041] # latin A + NULL
0041 FFFF ; [.1200.0020.0008.0041] # latin A + FFFF (invalid)
ENTRIES
  level => 1,
  table => undef,
  normalization => undef,
);

# 2..12
ok($illeg->lt("", "\x00"));
ok($illeg->lt("", "\x01"));
ok($illeg->eq("", "\x{FFFE}"));
ok($illeg->eq("", "\x{FFFF}"));
ok($illeg->eq("", "\x{D800}"));
ok($illeg->eq("", "\x{DFFF}"));
ok($illeg->eq("", "\x{FDD0}"));
ok($illeg->eq("", "\x{FDEF}"));
ok($illeg->lt("", "\x02"));
ok($illeg->eq("", "\x{10FFFF}"));
ok($illeg->eq("", "\x{110000}"));

# 13..22
ok($illeg->lt("\x00", "\x01"));
ok($illeg->lt("\x01", "\x02"));
ok($illeg->ne("\0", "\x{D800}"));
ok($illeg->ne("\0", "\x{DFFF}"));
ok($illeg->ne("\0", "\x{FDD0}"));
ok($illeg->ne("\0", "\x{FDEF}"));
ok($illeg->ne("\0", "\x{FFFE}"));
ok($illeg->ne("\0", "\x{FFFF}"));
ok($illeg->ne("\0", "\x{10FFFF}"));
ok($illeg->ne("\0", "\x{110000}"));

# 23..26
ok($illeg->eq("A",   "A\x{FFFF}"));
ok($illeg->gt("A\0", "A\x{FFFF}"));
ok($illeg->lt("A",  "A\0"));
ok($illeg->lt("AA", "A\0"));

##################

my($match, $str, $sub, $ret);

my $Collator = Unicode::Collate->new(
  table => 'keys.txt',
  level => 1,
  normalization => undef,
);

$sub = "pe";


$str = "Pe\x{300}\x{301}rl";
$ret = "Pe\x{300}\x{301}";
($match) = $Collator->match($str, $sub);
ok($match, $ret);

$str = "Pe\x{300}\0\0\x{301}rl";
$ret = "Pe\x{300}\0\0\x{301}";
($match) = $Collator->match($str, $sub);
ok($match, $ret);

$str = "Pe\x{DA00}\x{301}\x{DFFF}rl";
$ret = "Pe\x{DA00}\x{301}\x{DFFF}";
($match) = $Collator->match($str, $sub);
ok($match, $ret);

$str = "Pe\x{FFFF}\x{301}rl";
$ret = "Pe\x{FFFF}\x{301}";
($match) = $Collator->match($str, $sub);
ok($match, $ret);

$str = "Pe\x{110000}\x{301}rl";
$ret = "Pe\x{110000}\x{301}";
($match) = $Collator->match($str, $sub);
ok($match, $ret);

$str = "Pe\x{300}\x{d801}\x{301}rl";
$ret = "Pe\x{300}\x{d801}\x{301}";
($match) = $Collator->match($str, $sub);
ok($match, $ret);

$str = "Pe\x{300}\x{ffff}\x{301}rl";
$ret = "Pe\x{300}\x{ffff}\x{301}";
($match) = $Collator->match($str, $sub);
ok($match, $ret);

$str = "Pe\x{300}\x{110000}\x{301}rl";
$ret = "Pe\x{300}\x{110000}\x{301}";
($match) = $Collator->match($str, $sub);
ok($match, $ret);

$str = "Pe\x{D9ab}\x{DFFF}rl";
$ret = "Pe\x{D9ab}\x{DFFF}";
($match) = $Collator->match($str, $sub);
ok($match, $ret);

$str = "Pe\x{FFFF}rl";
$ret = "Pe\x{FFFF}";
($match) = $Collator->match($str, $sub);
ok($match, $ret);

$str = "Pe\x{110000}rl";
$ret = "Pe\x{110000}";
($match) = $Collator->match($str, $sub);
ok($match, $ret);

$str = "Pe\x{300}\x{D800}\x{DFFF}rl";
$ret = "Pe\x{300}\x{D800}\x{DFFF}";
($match) = $Collator->match($str, $sub);
ok($match, $ret);

$str = "Pe\x{300}\x{FFFF}rl";
$ret = "Pe\x{300}\x{FFFF}";
($match) = $Collator->match($str, $sub);
ok($match, $ret);

$str = "Pe\x{300}\x{110000}rl";
$ret = "Pe\x{300}\x{110000}";
($match) = $Collator->match($str, $sub);
ok($match, $ret);


