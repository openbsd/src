
BEGIN {
    unless ('A' eq pack('U', 0x41)) {
	print "1..0 # Unicode::Normalize cannot pack a Unicode code point\n";
	exit 0;
    }
    unless (0x41 == unpack('U', 'A')) {
	print "1..0 # Unicode::Normalize cannot get a Unicode code point\n";
	exit 0;
    }
}

BEGIN {
    if ($ENV{PERL_CORE}) {
        chdir('t') if -d 't';
        @INC = $^O eq 'MacOS' ? qw(::lib) : qw(../lib);
    }
}

BEGIN {
    unless (5.006001 <= $]) {
	print "1..0 # skipped: Perl 5.6.1 or later".
		" needed for this test\n";
	exit;
    }
}

#########################

use strict;
use warnings;
BEGIN { $| = 1; print "1..26\n"; }
my $count = 0;
sub ok ($;$) {
    my $p = my $r = shift;
    if (@_) {
	my $x = shift;
	$p = !defined $x ? !defined $r : !defined $r ? 0 : $r eq $x;
    }
    print $p ? "ok" : "not ok", ' ', ++$count, "\n";
}

use Unicode::Normalize qw(:all);

ok(1);

sub _pack_U   { Unicode::Normalize::pack_U(@_) }
sub _unpack_U { Unicode::Normalize::unpack_U(@_) }

#########################

sub arraynorm {
    my $form   = shift;
    my @string = @_;
    my $result = "";
    my $unproc = "";
    foreach my $str (@string) {
        $unproc .= $str;
        $result .= $form eq 'NFC'  ? NFC_partial ($unproc) :
		   $form eq 'NFD'  ? NFD_partial ($unproc) :
		   $form eq 'NFKC' ? NFKC_partial($unproc) :
		   $form eq 'NFKD' ? NFKD_partial($unproc) :
		   undef;
    }
    $result .= $unproc;
    return $result;
}

my $strD = "\x{3C9}\x{301}\x{1100}\x{1161}\x{11A8}\x{1100}\x{1161}\x{11AA}";
my $strC = "\x{3CE}\x{AC01}\x{AC03}";
my @str1 = (substr($strD,0,3), substr($strD,3,4), substr($strD,7));
my @str2 = (substr($strD,0,1), substr($strD,1,3), substr($strD,4));
ok($strC eq NFC($strD));
ok($strD eq join('', @str1));
ok($strC eq arraynorm('NFC', @str1));
ok($strD eq join('', @str2));
ok($strC eq arraynorm('NFC', @str2));

my @strX = ("\x{300}\x{AC00}", "\x{11A8}");
my $strX =  "\x{300}\x{AC01}";
ok($strX eq NFC(join('', @strX)));
ok($strX eq arraynorm('NFC', @strX));
ok($strX eq NFKC(join('', @strX)));
ok($strX eq arraynorm('NFKC', @strX));

my @strY = ("\x{304B}\x{0308}", "\x{0323}\x{3099}");
my $strY = ("\x{304C}\x{0323}\x{0308}");
ok($strY eq NFC(join('', @strY)));
ok($strY eq arraynorm('NFC', @strY));
ok($strY eq NFKC(join('', @strY)));
ok($strY eq arraynorm('NFKC', @strY));

my @strZ = ("\x{304B}\x{0308}", "\x{0323}", "\x{3099}");
my $strZ = ("\x{304B}\x{3099}\x{0323}\x{0308}");
ok($strZ eq NFD(join('', @strZ)));
ok($strZ eq arraynorm('NFD', @strZ));
ok($strZ eq NFKD(join('', @strZ)));
ok($strZ eq arraynorm('NFKD', @strZ));

# 18

# must modify the source
my $sNFD = "\x{FA19}";
ok(NFD_partial($sNFD), "");
ok($sNFD, "\x{795E}");

my $sNFC = "\x{FA1B}";
ok(NFC_partial($sNFC), "");
ok($sNFC, "\x{798F}");

my $sNFKD = "\x{FA1E}";
ok(NFKD_partial($sNFKD), "");
ok($sNFKD, "\x{7FBD}");

my $sNFKC = "\x{FA26}";
ok(NFKC_partial($sNFKC), "");
ok($sNFKC, "\x{90FD}");

# 26

