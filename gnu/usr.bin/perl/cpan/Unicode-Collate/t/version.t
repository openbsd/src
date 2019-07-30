
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
BEGIN { $| = 1; print "1..17\n"; }
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

# Fix me when UCA and/or keys.txt is upgraded.
my $UCA_Version = "30";
my $Base_Unicode_Version = "7.0.0";
my $Key_Version = "3.1.1";

ok(Unicode::Collate::UCA_Version, $UCA_Version);
ok(Unicode::Collate->UCA_Version, $UCA_Version);
ok(Unicode::Collate::Base_Unicode_Version, $Base_Unicode_Version);
ok(Unicode::Collate->Base_Unicode_Version, $Base_Unicode_Version);

my $Collator = Unicode::Collate->new(
  table => 'keys.txt',
  normalization => undef,
);

ok($Collator->UCA_Version,   $UCA_Version);
ok($Collator->UCA_Version(), $UCA_Version);
ok($Collator->Base_Unicode_Version,   $Base_Unicode_Version);
ok($Collator->Base_Unicode_Version(), $Base_Unicode_Version);
ok($Collator->version,   $Key_Version);
ok($Collator->version(), $Key_Version);

my $UndefTable = Unicode::Collate->new(
  table => undef,
  normalization => undef,
);

ok($UndefTable->UCA_Version,   $UCA_Version);
ok($UndefTable->UCA_Version(), $UCA_Version);
ok($UndefTable->Base_Unicode_Version,   $Base_Unicode_Version);
ok($UndefTable->Base_Unicode_Version(), $Base_Unicode_Version);
ok($UndefTable->version,   "unknown");
ok($UndefTable->version(), "unknown");

