#!./perl -w

use strict;
use Test::More;
use Config;

plan(skip_all => "POSIX is unavailable")
    unless $Config{extensions} =~ /\bPOSIX\b/;
plan(skip_all => "sigemptyset is unavailable on $^O")
    if $^O eq 'MSWin32' || $^O eq 'NetWare';

require POSIX;
POSIX->import();

my @signo;
my ($min, $max) = (~0, -1);

sub expected_signals {
    my $sigset = shift;
    my $desc = shift;
    my %expected;
    ++$expected{$_} foreach @_;

    local $Test::Builder::Level = $Test::Builder::Level + 1;

    for my $sig ($min..$max) {
	if ($expected{$sig}) {
	    cmp_ok($sigset->ismember($sig), '==', 1,
		   "$desc - sig $sig is a member");
	} else {
	    cmp_ok($sigset->ismember($sig), '==', 0,
		   "$desc - sig $sig is not a member");
	}
    }
}

foreach (@POSIX::EXPORT) {
    next unless /^SIG[A-Z0-9]+$/;
    my $val = eval "POSIX::$_";
    next unless defined $val;
    $min = $val if $min > $val;
    $max = $val if $max < $val;
    push @signo, $val;
}

# Sanity check that we found something:
cmp_ok(scalar @signo, '>=', 6,
       'found at least 6 signals (6 are named in the ANSI C spec)');

my $sigset = POSIX::SigSet->new();
isa_ok($sigset, 'POSIX::SigSet', 'checking the type of the object');
expected_signals($sigset, 'new object');

is($sigset->fillset(), '0 but true', 'fillset');
# because on some systems, not all integers are valid signals...
# so the only thing we can really be confident about is that all the signals
# with names are going to be present:
foreach (@signo) {
    cmp_ok($sigset->ismember($_), '==', 1, "after fillset sig $_ is a member");
}
is($sigset->emptyset(), '0 but true', 'empyset');
expected_signals($sigset, 'after emptyset');

is($sigset->addset($signo[1]), '0 but true', 'addset');
expected_signals($sigset, 'after addset', $signo[1]);
is($sigset->addset($signo[2]), '0 but true', 'addset');
expected_signals($sigset, 'after addset', $signo[1], $signo[2]);
is($sigset->addset($signo[4]), '0 but true', 'addset');
expected_signals($sigset, 'after addset', $signo[1], $signo[2], $signo[4]);
is($sigset->addset($signo[2]), '0 but true', 'addset');
expected_signals($sigset, 'after addset', $signo[1], $signo[2], $signo[4]);
is($sigset->delset($signo[4]), '0 but true', 'delset');
expected_signals($sigset, 'after addset', $signo[1], $signo[2]);
is($sigset->addset($signo[0]), '0 but true', 'addset');
expected_signals($sigset, 'after addset', $signo[0], $signo[1], $signo[2]);
is($sigset->delset($signo[4]), '0 but true', 'delset');
expected_signals($sigset, 'after delset', $signo[0], $signo[1], $signo[2]);
is($sigset->delset($signo[1]), '0 but true', 'delset');
expected_signals($sigset, 'after delset', $signo[0], $signo[2]);
is($sigset->delset($signo[0]), '0 but true', 'delset');
expected_signals($sigset, 'after addset', $signo[2]);
is($sigset->delset($signo[2]), '0 but true', 'delset');
expected_signals($sigset, 'empty again');

foreach ([$signo[0]],
	 [$signo[2]],
	 [$signo[3]],
	 [@signo[2,3,6]],
	) {
    $sigset = POSIX::SigSet->new(@$_);
    isa_ok($sigset, 'POSIX::SigSet', 'checking the type of the object');
    local $" = ', ';
    expected_signals($sigset, "new(@$_)", @$_);
}

done_testing();
