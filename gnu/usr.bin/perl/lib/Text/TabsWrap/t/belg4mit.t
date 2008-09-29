#!/usr/bin/perl -I.

use Text::Wrap;

print "1..1\n";

$Text::Wrap::columns = 1;
eval { wrap('', '', 'H4sICNoBwDoAA3NpZwA9jbsNwDAIRHumuC4NklvXTOD0KSJEnwU8fHz4Q8M9i3sGzkS7BBrm
OkCTwsycb4S3DloZuMIYeXpLFqw5LaMhXC2ymhreVXNWMw9YGuAYdfmAbwomoPSyFJuFn2x8
Opr8bBBidccAAAA'); };

if ($@) {
	my $e = $@;
	$e =~ s/^/# /gm;
	print $e;
}
print $@ ? "not ok 1\n" : "ok 1\n";

