
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
BEGIN { plan tests => 17 };

use strict;
use warnings;
use Unicode::Collate;

ok(1);

#########################

# Fix me when UCA and/or keys.txt is upgraded.
my $UCA_Version = "11";
my $Base_Unicode_Version = "4.0";
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

