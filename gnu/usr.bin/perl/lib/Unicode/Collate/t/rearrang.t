
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
BEGIN { plan tests => 23 };

use strict;
use warnings;
use Unicode::Collate;

ok(1);

#########################

my $Collator = Unicode::Collate->new(
  table => 'keys.txt',
  normalization => undef,
  UCA_Version => 9,
);

# rearrange : 0x0E40..0x0E44, 0x0EC0..0x0EC4 (default)

##### 2..9

my %old_rearrange = $Collator->change(rearrange => undef);

ok($Collator->gt("\x{0E41}A", "\x{0E40}B"));
ok($Collator->gt("A\x{0E41}A", "A\x{0E40}B"));

$Collator->change(rearrange => [ 0x61 ]);
 # U+0061, 'a': This is a Unicode value, never a native value.

ok($Collator->gt("ab", "AB")); # as 'ba' > 'AB'

$Collator->change(%old_rearrange);

ok($Collator->lt("ab", "AB"));
ok($Collator->lt("\x{0E40}", "\x{0E41}"));
ok($Collator->lt("\x{0E40}A", "\x{0E41}B"));
ok($Collator->lt("\x{0E41}A", "\x{0E40}B"));
ok($Collator->lt("A\x{0E41}A", "A\x{0E40}B"));

##### 10..13

my $all_undef_8 = Unicode::Collate->new(
  table => undef,
  normalization => undef,
  overrideCJK => undef,
  overrideHangul => undef,
  UCA_Version => 8,
);

ok($all_undef_8->lt("\x{0E40}", "\x{0E41}"));
ok($all_undef_8->lt("\x{0E40}A", "\x{0E41}B"));
ok($all_undef_8->lt("\x{0E41}A", "\x{0E40}B"));
ok($all_undef_8->lt("A\x{0E41}A", "A\x{0E40}B"));

##### 14..18

my $no_rearrange = Unicode::Collate->new(
  table => undef,
  normalization => undef,
  rearrange => [],
  UCA_Version => 9,
);

ok($no_rearrange->lt("A", "B"));
ok($no_rearrange->lt("\x{0E40}", "\x{0E41}"));
ok($no_rearrange->lt("\x{0E40}A", "\x{0E41}B"));
ok($no_rearrange->gt("\x{0E41}A", "\x{0E40}B"));
ok($no_rearrange->gt("A\x{0E41}A", "A\x{0E40}B"));

##### 19..23

my $undef_rearrange = Unicode::Collate->new(
  table => undef,
  normalization => undef,
  rearrange => undef,
  UCA_Version => 9,
);

ok($undef_rearrange->lt("A", "B"));
ok($undef_rearrange->lt("\x{0E40}", "\x{0E41}"));
ok($undef_rearrange->lt("\x{0E40}A", "\x{0E41}B"));
ok($undef_rearrange->gt("\x{0E41}A", "\x{0E40}B"));
ok($undef_rearrange->gt("A\x{0E41}A", "A\x{0E40}B"));

