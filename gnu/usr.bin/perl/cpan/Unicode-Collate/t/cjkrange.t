
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
BEGIN { $| = 1; print "1..421\n"; } # 1 + 42 x @Versions
my $count = 0;
sub ok ($;$) {
    my $p = my $r = shift;
    if (@_) {
	my $x = shift;
	$p = !defined $x ? !defined $r : !defined $r ? 0 : $r eq $x;
    }
    print $p ? "ok" : "not ok", ' ', ++$count, "\n";
}

use Unicode::Collate;

ok(1);

#########################

my $coll = Unicode::Collate->new(
    table => 'keys.txt',
    normalization => undef,
);

# CJK UI Ext > CJK UI.
# [ UCA_Version 8: Ext.A < UI and BMP < Ext.B (code point order) ]

# 4E00..9FA5 are CJK UI.
# 9FA6..9FBB are CJK UI since UCA_Version 14 (Unicode 4.1).
# 9FBC..9FC3 are CJK UI since UCA_Version 18 (Unicode 5.1).
# 9FC4..9FCB are CJK UI since UCA_Version 20 (Unicode 5.2).
# 9FCC       is  CJK UI since UCA_Version 24 (Unicode 6.1).

# 3400..4DB5   are CJK UI Ext.A since UCA_Version 8  (Unicode 3.0).
# 20000..2A6D6 are CJK UI Ext.B since UCA_Version 8  (Unicode 3.1).
# 2A700..2B734 are CJK UI Ext.C since UCA_Version 20 (Unicode 5.2).
# 2B740..2B81D are CJK UI Ext.D since UCA_Version 22 (Unicode 6.0).

my @Versions = (8, 9, 11, 14, 16, 18, 20, 22, 24, 26);

for my $v (@Versions) {
    $coll->change(UCA_Version => $v);

    # Ext.A > UI
    ok($coll->cmp("\x{3400}", "\x{4E00}") == ($v >=  9 ? 1 : -1)); # UI
    ok($coll->cmp("\x{3400}", "\x{9FA5}") == ($v >=  9 ? 1 : -1)); # UI
    ok($coll->cmp("\x{3400}", "\x{9FA6}") == ($v >= 14 ? 1 : -1)); # new
    ok($coll->cmp("\x{3400}", "\x{9FBB}") == ($v >= 14 ? 1 : -1)); # new
    ok($coll->cmp("\x{3400}", "\x{9FBC}") == ($v >= 18 ? 1 : -1)); # new
    ok($coll->cmp("\x{3400}", "\x{9FC3}") == ($v >= 18 ? 1 : -1)); # new
    ok($coll->cmp("\x{3400}", "\x{9FC4}") == ($v >= 20 ? 1 : -1)); # new
    ok($coll->cmp("\x{3400}", "\x{9FCB}") == ($v >= 20 ? 1 : -1)); # new
    ok($coll->cmp("\x{3400}", "\x{9FCC}") == ($v >= 24 ? 1 : -1)); # new
    ok($coll->cmp("\x{3400}", "\x{9FCD}") == -1); # na
    ok($coll->cmp("\x{3400}", "\x{9FFF}") == -1); # na

    # UI < UI
    ok($coll->cmp("\x{4E00}", "\x{9FA5}") == -1); # UI < UI
    ok($coll->cmp("\x{9FA5}", "\x{9FA6}") == -1); # UI < new
    ok($coll->cmp("\x{9FA6}", "\x{9FBB}") == -1); # new < new
    ok($coll->cmp("\x{9FBB}", "\x{9FBC}") == -1); # new < new
    ok($coll->cmp("\x{9FBC}", "\x{9FC3}") == -1); # new < new
    ok($coll->cmp("\x{9FC3}", "\x{9FC4}") == -1); # new < new
    ok($coll->cmp("\x{9FC4}", "\x{9FCB}") == -1); # new < new
    ok($coll->cmp("\x{9FCB}", "\x{9FCC}") == -1); # new < new
    ok($coll->cmp("\x{9FCC}", "\x{9FCD}") == -1); # new < na
    ok($coll->cmp("\x{9FCD}", "\x{9FFF}") == -1); # na < na

    # Ext.A < Ext.B
    ok($coll->cmp("\x{3400}", "\x{20000}") == -1);

    # Ext.A
    ok($coll->cmp("\x{3400}", "\x{4DB5}") == -1); # A < A
    ok($coll->cmp("\x{2FFF}", "\x{3400}") == ($v >= 8 ? 1 : -1)); # na > A
    ok($coll->cmp("\x{2FFF}", "\x{4DB5}") == ($v >= 8 ? 1 : -1)); # na > A
    ok($coll->cmp("\x{2FFF}", "\x{4DB6}") == -1); # na < na
    ok($coll->cmp("\x{2FFF}", "\x{4DBF}") == -1); # na < na

    # Ext.B
    ok($coll->cmp("\x{20000}","\x{2A6D6}") == -1); # B < B
    ok($coll->cmp("\x{2FFF}", "\x{20000}") == ($v >= 9 ? 1 : -1)); # na > B
    ok($coll->cmp("\x{2FFF}", "\x{2A6D6}") == ($v >= 9 ? 1 : -1)); # na > B
    ok($coll->cmp("\x{2FFF}", "\x{2A6D7}") == -1); # na < na
    ok($coll->cmp("\x{2FFF}", "\x{2A6DF}") == -1); # na < na

    # Ext.C
    ok($coll->cmp("\x{2A700}","\x{2B734}") == -1); # C < C
    ok($coll->cmp("\x{2FFF}", "\x{2A700}") == ($v >= 20 ? 1 : -1)); # na > C
    ok($coll->cmp("\x{2FFF}", "\x{2B734}") == ($v >= 20 ? 1 : -1)); # na > C
    ok($coll->cmp("\x{2FFF}", "\x{2B735}") == -1); # na < na
    ok($coll->cmp("\x{2FFF}", "\x{2B73F}") == -1); # na < na

    # Ext.D
    ok($coll->cmp("\x{2B740}","\x{2B81D}") == -1); # D < D
    ok($coll->cmp("\x{2FFF}", "\x{2B740}") == ($v >= 22 ? 1 : -1)); # na > D
    ok($coll->cmp("\x{2FFF}", "\x{2B81D}") == ($v >= 22 ? 1 : -1)); # na > D
    ok($coll->cmp("\x{2FFF}", "\x{2B81E}") == -1); # na < na
    ok($coll->cmp("\x{2FFF}", "\x{2B81F}") == -1); # na < na
}

