#!./perl
#
# rename.t - tests for Locale::Country with "uk" aliases to "gb"
#

use Locale::Country;

local $SIG{__WARN__} = sub { };		# muffle warnings from carp

Locale::Country::rename_country('gb' => 'Great Britain');

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
 '!defined code2country("uk")',              # code for United Kingdom is 'gb'

 #---- this call should return 0, since code doesn't exist --------------
 '!Locale::Country::rename_country("ukz", "United Karz")',

 #---- some successful examples -----------------------------------------
 'code2country("BO") eq "Bolivia"',
 'code2country("pk") eq "Pakistan"',
 'code2country("sn") eq "Senegal"',
 'code2country("us") eq "United States"',
 'code2country("ad") eq "Andorra"',          # first in DATA segment
 'code2country("zw") eq "Zimbabwe"',         # last in DATA segment
 'code2country("gb") eq "Great Britain"',    # normally "United Kingdom"

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

 'country2code("Great Britain") eq "gb"',
 'country2code("Great Britain", LOCALE_CODE_ALPHA_3) eq "gbr"',
 'country2code("Great Britain", LOCALE_CODE_NUMERIC) eq "826"',

 'country2code("United Kingdom") eq "gb"',
 'country2code("United Kingdom", LOCALE_CODE_ALPHA_3)  eq "gbr"',
 'country2code("United Kingdom", LOCALE_CODE_NUMERIC)  eq "826"',

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
