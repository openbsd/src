
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
BEGIN { $| = 1; print "1..45\n"; }
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

my $Collator = Unicode::Collate->new(
    table => 'keys.txt',
    normalization => undef,
);

# [001F] UNIT SEPARATOR
{
    ok($Collator->eq("\0",   "\x1F"));
    ok($Collator->eq("\x1F", "\x{200B}"));
    ok($Collator->eq("\0",   "\x{200B}"));
    ok($Collator->eq("\x{313}", "\x{343}"));
    ok($Collator->eq("\x{2000}", "\x{2001}"));
    ok($Collator->eq("\x{200B}", "\x{200C}"));
    ok($Collator->eq("\x{304C}", "\x{304B}\x{3099}"));

    $Collator->change(identical => 1);

    ok($Collator->lt("\0",   "\x1F"));
    ok($Collator->lt("\x1F", "\x{200B}"));
    ok($Collator->lt("\0",   "\x{200B}"));
    ok($Collator->lt("\x{313}", "\x{343}"));
    ok($Collator->lt("\x{2000}", "\x{2001}"));
    ok($Collator->lt("\x{200B}", "\x{200C}"));
    ok($Collator->gt("\x{304C}", "\x{304B}\x{3099}"));

    $Collator->change(identical => 0);

    ok($Collator->eq("\0",   "\x1F"));
    ok($Collator->eq("\x1F", "\x{200B}"));
    ok($Collator->eq("\0",   "\x{200B}"));
    ok($Collator->eq("\x{313}", "\x{343}"));
    ok($Collator->eq("\x{2000}", "\x{2001}"));
    ok($Collator->eq("\x{200B}", "\x{200C}"));
    ok($Collator->eq("\x{304C}", "\x{304B}\x{3099}"));
}

#### 22

eval { require Unicode::Normalize };
if (!$@) {
    $Collator->change(normalization => "NFD");

    $Collator->change(identical => 1);

    ok($Collator->lt("\0", "\x{200B}"));
    ok($Collator->eq("\x{313}", "\x{343}"));
    ok($Collator->lt("\x{2000}", "\x{2001}"));
    ok($Collator->lt("\x{200B}", "\x{200C}"));
    ok($Collator->eq("\x{304C}", "\x{304B}\x{3099}"));

    $Collator->change(identical => 0);

    ok($Collator->eq("\0", "\x{200B}"));
    ok($Collator->eq("\x{313}", "\x{343}"));
    ok($Collator->eq("\x{2000}", "\x{2001}"));
    ok($Collator->eq("\x{200B}", "\x{200C}"));
    ok($Collator->eq("\x{304C}", "\x{304B}\x{3099}"));
} else {
    ok(1) for 1..10;
}

$Collator->change(normalization => undef, identical => 1);

##### 32

ok($Collator->viewSortKey("\0"),       '[| | | | 0000 0000]');
ok($Collator->viewSortKey("\x{200B}"), '[| | | | 0000 200B]');

ok($Collator->viewSortKey('a'),
    '[0A15 | 0020 | 0002 | FFFF | 0000 0061]');

ok($Collator->viewSortKey("\x{304C}"),
    '[1926 | 0020 013D | 000E 0002 | FFFF FFFF | 0000 304C]');

ok($Collator->viewSortKey("\x{100000}"),
    '[FBE0 8000 | 0020 | 0002 | FFFF FFFF | 0010 0000]');

eval { require Unicode::Normalize };
if (!$@) {
    $Collator->change(normalization => "NFD");

    ok($Collator->viewSortKey("\x{304C}"),
    '[1926 | 0020 013D | 000E 0002 | FFFF FFFF | 0000 304B 0000 3099]');
} else {
    ok(1);
}

$Collator->change(normalization => undef);

##### 38

$Collator->change(level => 3);

ok($Collator->viewSortKey("\x{304C}"),
    '[1926 | 0020 013D | 000E 0002 | | 0000 304C]');

$Collator->change(level => 2);

ok($Collator->viewSortKey("\x{304C}"),
    '[1926 | 0020 013D | | | 0000 304C]');

$Collator->change(level => 1);

ok($Collator->viewSortKey("\x{304C}"),
    '[1926 | | | | 0000 304C]');

##### 41

$Collator->change(UCA_Version => 8);

ok($Collator->viewSortKey("\x{304C}"),
    '[1926||||0000 304C]');

$Collator->change(level => 2);

ok($Collator->viewSortKey("\x{304C}"),
    '[1926|0020 013D|||0000 304C]');

$Collator->change(level => 3);

ok($Collator->viewSortKey("\x{304C}"),
    '[1926|0020 013D|000E 0002||0000 304C]');

$Collator->change(level => 4);

ok($Collator->viewSortKey("\x{304C}"),
    '[1926|0020 013D|000E 0002|FFFF FFFF|0000 304C]');

##### 45
