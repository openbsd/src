#!./perl
#
# script.t - tests for Locale::Script
#

use Locale::Script;

#-----------------------------------------------------------------------
# This is an array of tests specs. Each spec is [TEST, OK_TO_DIE]
# Each TEST is eval'd as an expression.
# If it evaluates to FALSE, then "not ok N" is printed for the test,
# otherwise "ok N". If the eval dies, then the OK_TO_DIE flag is checked.
# If it is true (1), the test is treated as passing, otherwise it failed.
#-----------------------------------------------------------------------
@TESTS =
(
	#================================================
	# TESTS FOR code2script
	#================================================

 #---- selection of examples which should all result in undef -----------
 ['!defined code2script()', 0],                  # no argument
 ['!defined code2script(undef)', 0],             # undef argument
 ['!defined code2script("aa")', 0],              # illegal code
 ['!defined code2script("aa", LOCALE_CODE_ALPHA_2)', 0],        # illegal code
 ['!defined code2script("aa", LOCALE_CODE_ALPHA_3)', 0],        # illegal code
 ['!defined code2script("aa", LOCALE_CODE_NUMERIC)', 0],        # illegal code

 #---- some successful examples -----------------------------------------
 ['code2script("BO") eq "Tibetan"', 0],
 ['code2script("Bo") eq "Tibetan"', 0],
 ['code2script("bo") eq "Tibetan"', 0],
 ['code2script("bo", LOCALE_CODE_ALPHA_2) eq "Tibetan"', 0],
 ['code2script("bod", LOCALE_CODE_ALPHA_3) eq "Tibetan"', 0],
 ['code2script("330", LOCALE_CODE_NUMERIC) eq "Tibetan"', 0],

 ['code2script("yi", LOCALE_CODE_ALPHA_2) eq "Yi"', 0], # last in DATA
 ['code2script("Yii", LOCALE_CODE_ALPHA_3) eq "Yi"', 0],
 ['code2script("460", LOCALE_CODE_NUMERIC) eq "Yi"', 0],

 ['code2script("am") eq "Aramaic"', 0],          # first in DATA segment


	#================================================
	# TESTS FOR script2code
	#================================================

 #---- selection of examples which should all result in undef -----------
 ['!defined code2script("BO", LOCALE_CODE_ALPHA_3)', 0],
 ['!defined code2script("BO", LOCALE_CODE_NUMERIC)', 0],
 ['!defined script2code()', 0],                  # no argument
 ['!defined script2code(undef)', 0],             # undef argument
 ['!defined script2code("Banana")', 0],          # illegal script name

 #---- some successful examples -----------------------------------------
 ['script2code("meroitic")                   eq "me"', 0],
 ['script2code("burmese")                    eq "my"', 0],
 ['script2code("Pahlavi")                    eq "ph"', 0],
 ['script2code("Vai", LOCALE_CODE_ALPHA_3)   eq "vai"', 0],
 ['script2code("Tamil", LOCALE_CODE_NUMERIC) eq "346"', 0],
 ['script2code("Latin")                      eq "la"', 0],
 ['script2code("Latin", LOCALE_CODE_ALPHA_3) eq "lat"', 0],

	#================================================
	# TESTS FOR script_code2code
	#================================================

 #---- selection of examples which should all result in undef -----------
 ['!defined script_code2code("bo", LOCALE_CODE_ALPHA_3, LOCALE_CODE_ALPHA_3)', 0],
 ['!defined script_code2code("aa", LOCALE_CODE_ALPHA_2, LOCALE_CODE_ALPHA_3)', 0],
 ['!defined script_code2code("aa", LOCALE_CODE_ALPHA_3, LOCALE_CODE_ALPHA_3)', 0],
 ['!defined script_code2code("aa", LOCALE_CODE_ALPHA_2)', 1],
 ['!defined script_code2code()', 1],                  # no argument
 ['!defined script_code2code(undef)', 1],             # undef argument

 #---- some successful examples -----------------------------------------
 ['script_code2code("BO", LOCALE_CODE_ALPHA_2, LOCALE_CODE_ALPHA_3) eq "bod"', 0],
 ['script_code2code("bod", LOCALE_CODE_ALPHA_3, LOCALE_CODE_ALPHA_2) eq "bo"', 0],
 ['script_code2code("Phx", LOCALE_CODE_ALPHA_3, LOCALE_CODE_ALPHA_2) eq "ph"', 0],
 ['script_code2code("295", LOCALE_CODE_NUMERIC, LOCALE_CODE_ALPHA_3) eq "pqd"', 0],
 ['script_code2code(170, LOCALE_CODE_NUMERIC, LOCALE_CODE_ALPHA_3) eq "tna"', 0],
 ['script_code2code("rr", LOCALE_CODE_ALPHA_2, LOCALE_CODE_NUMERIC) eq "620"', 0],

);

print "1..", int(@TESTS), "\n";

$testid = 1;
foreach $test (@TESTS)
{
    eval "print (($test->[0]) ? \"ok $testid\\n\" : \"not ok $testid\\n\" )";
    if ($@)
    {
	if (!$test->[1])
	{
	    print "not ok $testid\n";
	}
	else
	{
	    print "ok $testid\n";
	}
    }
    ++$testid;
}

exit 0;
