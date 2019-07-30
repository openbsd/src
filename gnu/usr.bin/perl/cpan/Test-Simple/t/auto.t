use strict;
use warnings;

use lib 't';

use Test::Tester tests => 5;

use SmallTest;

use MyTest;

{
	my ($prem, @results) = run_tests(
		sub { MyTest::ok(1, "run pass")}
	);

	is_eq($results[0]->{name}, "run pass");
	is_num($results[0]->{ok}, 1);
}

{
	my ($prem, @results) = run_tests(
		sub { MyTest::ok(0, "run fail")}
	);

	is_eq($results[0]->{name}, "run fail");
	is_num($results[0]->{ok}, 0);
}

is_eq(ref(SmallTest::getTest()), "Test::Tester::Delegate");
