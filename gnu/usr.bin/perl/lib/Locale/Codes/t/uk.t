#!./perl
#
# uk.t - tests for Locale::Country with "uk" aliases to "gb"
#

BEGIN {
	chdir 't' if -d 't';
	@INC = '../lib';
}

use Locale::Country;

Locale::Country::alias_code('uk' => 'gb');

#-----------------------------------------------------------------------
# This is an array of tests. Each test is eval'd as an expression.
# If it evaluates to FALSE, then "not ok N" is printed for the test,
# otherwise "ok N".
#-----------------------------------------------------------------------
@TESTS =
(
	#================================================
	# TESTS FOR code2country
	#================================================

 #---- selection of examples which should all result in undef -----------
 '!defined code2country()',                  # no argument
 '!defined code2country(undef)',             # undef argument
 '!defined code2country("zz")',              # illegal code
 '!defined code2country("ja")',              # should be jp for country

 #---- some successful examples -----------------------------------------
 'code2country("BO") eq "Bolivia"',
 'code2country("pk") eq "Pakistan"',
 'code2country("sn") eq "Senegal"',
 'code2country("us") eq "United States"',
 'code2country("ad") eq "Andorra"',          # first in DATA segment
 'code2country("zw") eq "Zimbabwe"',         # last in DATA segment
 'code2country("uk") eq "United Kingdom"',   # normally "gb"

	#================================================
	# TESTS FOR country2code
	#================================================

 #---- selection of examples which should all result in undef -----------
 '!defined country2code()',                  # no argument
 '!defined country2code(undef)',             # undef argument
 '!defined country2code("Banana")',          # illegal country name

 #---- some successful examples -----------------------------------------
 'country2code("japan")          eq "jp"',
 'country2code("japan")          ne "ja"',
 'country2code("Japan")          eq "jp"',
 'country2code("United States")  eq "us"',
 'country2code("United Kingdom") eq "uk"',
 'country2code("Andorra")        eq "ad"',    # first in DATA segment
 'country2code("Zimbabwe")       eq "zw"',    # last in DATA segment
);

print "1..", int(@TESTS), "\n";

$testid = 1;
foreach $test (@TESTS)
{
    eval "print (($test) ? \"ok $testid\\n\" : \"not ok $testid\\n\" )";
    print "not ok $testid\n" if $@;
    ++$testid;
}

exit 0;
