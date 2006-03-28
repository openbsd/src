
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
BEGIN { plan tests => 65 };

use strict;
use warnings;
use Unicode::Collate;

our $IsEBCDIC = ord("A") != 0x41;

#########################

ok(1);

my $Collator = Unicode::Collate->new(
  table => 'keys.txt',
  normalization => undef,
);

##############

my %old_level = $Collator->change(level => 2);

my $str;

my $orig = "This is a Perl book.";
my $sub = "PERL";
my $rep = "camel";
my $ret = "This is a camel book.";

$str = $orig;
if (my($pos,$len) = $Collator->index($str, $sub)) {
  substr($str, $pos, $len, $rep);
}

ok($str, $ret);

$Collator->change(%old_level);

$str = $orig;
if (my($pos,$len) = $Collator->index($str, $sub)) {
  substr($str, $pos, $len, $rep);
}

ok($str, $orig);

##############

my $match;

$Collator->change(level => 1);

$str = "Pe\x{300}rl";
$sub = "pe";
$ret = "Pe\x{300}";
$match = undef;
if (my($pos, $len) = $Collator->index($str, $sub)) {
    $match = substr($str, $pos, $len);
}
ok($match, $ret);

$str = "P\x{300}e\x{300}\x{301}\x{303}rl";
$sub = "pE";
$ret = "P\x{300}e\x{300}\x{301}\x{303}";
$match = undef;
if (my($pos, $len) = $Collator->index($str, $sub)) {
    $match = substr($str, $pos, $len);
}
ok($match, $ret);

$Collator->change(level => 2);

$str = "Pe\x{300}rl";
$sub = "pe";
$ret = undef;
$match = undef;
if (my($pos, $len) = $Collator->index($str, $sub)) {
    $match = substr($str, $pos, $len);
}
ok($match, $ret);

$str = "P\x{300}e\x{300}\x{301}\x{303}rl";
$sub = "pE";
$ret = undef;
$match = undef;
if (my($pos, $len) = $Collator->index($str, $sub)) {
    $match = substr($str, $pos, $len);
}
ok($match, $ret);

$str = "Pe\x{300}rl";
$sub = "pe\x{300}";
$ret = "Pe\x{300}";
$match = undef;
if (my($pos, $len) = $Collator->index($str, $sub)) {
    $match = substr($str, $pos, $len);
}
ok($match, $ret);

$str = "P\x{300}e\x{300}\x{301}\x{303}rl";
$sub = "p\x{300}E\x{300}\x{301}\x{303}";
$ret = "P\x{300}e\x{300}\x{301}\x{303}";
$match = undef;
if (my($pos, $len) = $Collator->index($str, $sub)) {
    $match = substr($str, $pos, $len);
}
ok($match, $ret);

##############

$Collator->change(level => 1);

$str = $IsEBCDIC
    ? "Ich mu\x{0059} studieren Perl."
    : "Ich mu\x{00DF} studieren Perl.";
$sub = $IsEBCDIC
    ? "m\x{00DC}ss"
    : "m\x{00FC}ss";
$ret = $IsEBCDIC
    ? "mu\x{0059}"
    : "mu\x{00DF}";
$match = undef;
if (my($pos, $len) = $Collator->index($str, $sub)) {
    $match = substr($str, $pos, $len);
}
ok($match, $ret);

$Collator->change(%old_level);

$match = undef;
if (my($pos, $len) = $Collator->index($str, $sub)) {
    $match = substr($str, $pos, $len);
}
ok($match, undef);

$match = undef;
if (my($pos,$len) = $Collator->index("", "")) {
    $match = substr("", $pos, $len);
}
ok($match, "");

$match = undef;
if (my($pos,$len) = $Collator->index("", "abc")) {
    $match = substr("", $pos, $len);
}
ok($match, undef);

##############

$Collator->change(level => 1);

$str = "\0\cA\0\cAe\0\x{300}\cA\x{301}\cB\x{302}\0 \0\cA";
$sub = "e";
$ret = "e\0\x{300}\cA\x{301}\cB\x{302}\0";
$match = undef;
if (my($pos, $len) = $Collator->index($str, $sub)) {
    $match = substr($str, $pos, $len);
}
ok($match, $ret);

$Collator->change(level => 1);

$str = "\0\cA\0\cAe\0\cA\x{300}\0\cAe";
$sub = "e";
$ret = "e\0\cA\x{300}\0\cA";
$match = undef;
if (my($pos, $len) = $Collator->index($str, $sub)) {
    $match = substr($str, $pos, $len);
}
ok($match, $ret);


$Collator->change(%old_level);

$str = "e\x{300}";
$sub = "e";
$ret = undef;
$match = undef;
if (my($pos, $len) = $Collator->index($str, $sub)) {
    $match = substr($str, $pos, $len);
}
ok($match, $ret);

##############

$Collator->change(level => 1);

$str = "The Perl is a language, and the perl is an interpreter.";
$sub = "PERL";

$match = undef;
if (my($pos, $len) = $Collator->index($str, $sub, -40)) {
    $match = substr($str, $pos, $len);
}
ok($match, "Perl");

$match = undef;
if (my($pos, $len) = $Collator->index($str, $sub, 4)) {
    $match = substr($str, $pos, $len);
}
ok($match, "Perl");

$match = undef;
if (my($pos, $len) = $Collator->index($str, $sub, 5)) {
    $match = substr($str, $pos, $len);
}
ok($match, "perl");

$match = undef;
if (my($pos, $len) = $Collator->index($str, $sub, 32)) {
    $match = substr($str, $pos, $len);
}
ok($match, "perl");

$match = undef;
if (my($pos, $len) = $Collator->index($str, $sub, 33)) {
    $match = substr($str, $pos, $len);
}
ok($match, undef);

$match = undef;
if (my($pos, $len) = $Collator->index($str, $sub, 100)) {
    $match = substr($str, $pos, $len);
}
ok($match, undef);

$Collator->change(%old_level);

##############

my @ret;

$Collator->change(level => 1);

$ret = $Collator->match("P\cBe\x{300}\cBrl and PERL", "pe");
ok($ret);
ok($$ret eq "P\cBe\x{300}\cB");

@ret = $Collator->match("P\cBe\x{300}\cBrl and PERL", "pe");
ok($ret[0], "P\cBe\x{300}\cB");

$str = $IsEBCDIC ? "mu\x{0059}" : "mu\x{00DF}";
$sub = $IsEBCDIC ? "m\x{00DC}ss" : "m\x{00FC}ss";

($ret) = $Collator->match($str, $sub);
ok($ret, $str);

$str = $IsEBCDIC ? "mu\x{0059}" : "mu\x{00DF}";
$sub = $IsEBCDIC ? "m\x{00DC}s" : "m\x{00FC}s";

($ret) = $Collator->match($str, $sub);
ok($ret, undef);

$ret = join ':', $Collator->gmatch("P\cBe\x{300}\cBrl, perl, and PERL", "pe");
ok($ret eq "P\cBe\x{300}\cB:pe:PE");

$ret = $Collator->gmatch("P\cBe\x{300}\cBrl, perl, and PERL", "pe");
ok($ret == 3);

$str = "ABCDEF";
$sub = "cde";
$ret = $Collator->match($str, $sub);
$str = "01234567";
ok($ret && $$ret, "CDE");

$str = "ABCDEF";
$sub = "cde";
($ret) = $Collator->match($str, $sub);
$str = "01234567";
ok($ret, "CDE");


$Collator->change(level => 3);

$ret = $Collator->match("P\cBe\x{300}\cBrl and PERL", "pe");
ok($ret, undef);

@ret = $Collator->match("P\cBe\x{300}\cBrl and PERL", "pe");
ok(@ret == 0);

$ret = join ':', $Collator->gmatch("P\cBe\x{300}\cBrl and PERL", "pe");
ok($ret eq "");

$ret = $Collator->gmatch("P\cBe\x{300}\cBrl and PERL", "pe");
ok($ret == 0);

$ret = join ':', $Collator->gmatch("P\cBe\x{300}\cBrl, perl, and PERL", "pe");
ok($ret eq "pe");

$ret = $Collator->gmatch("P\cBe\x{300}\cBrl, perl, and PERL", "pe");
ok($ret == 1);

$str = $IsEBCDIC ? "mu\x{0059}" : "mu\x{00DF}";
$sub = $IsEBCDIC ? "m\x{00DC}ss" : "m\x{00FC}ss";

($ret) = $Collator->match($str, $sub);
ok($ret, undef);

$Collator->change(%old_level);

##############

$Collator->change(level => 1);

sub strreverse { scalar reverse shift }

$str = "P\cBe\x{300}\cBrl and PERL.";
$ret = $Collator->subst($str, "perl", 'Camel');
ok($ret, 1);
ok($str, "Camel and PERL.");

$str = "P\cBe\x{300}\cBrl and PERL.";
$ret = $Collator->subst($str, "perl", \&strreverse);
ok($ret, 1);
ok($str, "lr\cB\x{300}e\cBP and PERL.");

$str = "P\cBe\x{300}\cBrl and PERL.";
$ret = $Collator->gsubst($str, "perl", 'Camel');
ok($ret, 2);
ok($str, "Camel and Camel.");

$str = "P\cBe\x{300}\cBrl and PERL.";
$ret = $Collator->gsubst($str, "perl", \&strreverse);
ok($ret, 2);
ok($str, "lr\cB\x{300}e\cBP and LREP.");

$str = "Camel donkey zebra came\x{301}l CAMEL horse cAm\0E\0L...";
$Collator->gsubst($str, "camel", sub { "<b>$_[0]</b>" });
ok($str, "<b>Camel</b> donkey zebra <b>came\x{301}l</b> "
	. "<b>CAMEL</b> horse <b>cAm\0E\0L</b>...");

$Collator->change(level => 3);

$str = "P\cBe\x{300}\cBrl and PERL.";
$ret = $Collator->subst($str, "perl", "Camel");
ok(! $ret);
ok($str, "P\cBe\x{300}\cBrl and PERL.");

$str = "P\cBe\x{300}\cBrl and PERL.";
$ret = $Collator->subst($str, "perl", \&strreverse);
ok(! $ret);
ok($str, "P\cBe\x{300}\cBrl and PERL.");

$str = "P\cBe\x{300}\cBrl and PERL.";
$ret = $Collator->gsubst($str, "perl", "Camel");
ok($ret, 0);
ok($str, "P\cBe\x{300}\cBrl and PERL.");

$str = "P\cBe\x{300}\cBrl and PERL.";
$ret = $Collator->gsubst($str, "perl", \&strreverse);
ok($ret, 0);
ok($str, "P\cBe\x{300}\cBrl and PERL.");

$Collator->change(%old_level);

##############

$str = "Perl and Camel";
$ret = $Collator->gsubst($str, "\cA\cA\0", "AB");
ok($ret, 15);
ok($str, "ABPABeABrABlAB ABaABnABdAB ABCABaABmABeABlAB");

$str = '';
$ret = $Collator->subst($str, "", "ABC");
ok($ret, 1);
ok($str, "ABC");

$str = '';
$ret = $Collator->gsubst($str, "", "ABC");
ok($ret, 1);
ok($str, "ABC");

$str = 'PPPPP';
$ret = $Collator->gsubst($str, 'PP', "ABC");
ok($ret, 2);
ok($str, "ABCABCP");

##############

# Shifted; ignorable after variable

($ret) = $Collator->match("A?\x{300}!\x{301}\x{344}B\x{315}", "?!");
ok($ret, "?\x{300}!\x{301}\x{344}");

$Collator->change(alternate => 'Non-ignorable');

($ret) = $Collator->match("A?\x{300}!\x{301}B\x{315}", "?!");
ok($ret, undef);

