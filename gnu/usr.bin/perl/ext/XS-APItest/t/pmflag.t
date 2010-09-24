#!perl
use strict;
use Test::More 'no_plan';

my @warnings;
$SIG{__WARN__} = sub {
    push @warnings, "@_";
};

use XS::APItest 'pmflag';

foreach (["\0", 0],
	 ['Q', 0],
	 ['c', 0x00004000],
	) {
    my ($char, $val) = @$_;
    my $ord = ord $char;
    foreach my $before (0, 1) {
	my $got = pmflag($ord, $before);
	is($got, $before | $val, "Flag $ord, before $before");
	is(@warnings, 1);
	like($warnings[0],
	     qr/^Perl_pmflag\(\) is deprecated, and will be removed from the XS API/);
	@warnings = ();

	no warnings 'deprecated';

	$got = pmflag($ord, $before);
	is($got, $before | $val, "Flag $ord, before $before");
	is(@warnings, 0);
	@warnings = ();

	use warnings;
	$got = pmflag($ord, $before);
	is($got, $before | $val, "Flag $ord, before $before");
	is(@warnings, 1);
	like($warnings[0],
	     qr/^Perl_pmflag\(\) is deprecated, and will be removed from the XS API/);
	@warnings = ();
    }
}
