#!./perl
#
# currency.t - tests for Locale::Currency
#

BEGIN {
        chdir 't' if -d 't';
        @INC = '../lib';
}

use Locale::Currency;

#-----------------------------------------------------------------------
# This is an array of tests. Each test is eval'd as an expression.
# If it evaluates to FALSE, then "not ok N" is printed for the test,
# otherwise "ok N".
#-----------------------------------------------------------------------
@TESTS =
(
	#================================================
	# TESTS FOR code2currency
	#================================================

 #---- selection of examples which should all result in undef -----------
 '!defined code2currency()',                 # no argument => undef returned
 '!defined code2currency(undef)',            # undef arg   => undef returned
 '!defined code2currency("zz")',             # illegal code => undef
 '!defined code2currency("zzzz")',           # illegal code => undef
 '!defined code2currency("zzz")',            # illegal code => undef
 '!defined code2currency("ukp")',            # gbp for sterling, not ukp

 #---- misc tests -------------------------------------------------------
 'code2currency("all") eq "Lek"',
 'code2currency("ats") eq "Schilling"',
 'code2currency("bob") eq "Boliviano"',
 'code2currency("bnd") eq "Brunei Dollar"',
 'code2currency("cop") eq "Colombian Peso"',
 'code2currency("dkk") eq "Danish Krone"',
 'code2currency("fjd") eq "Fiji Dollar"',
 'code2currency("idr") eq "Rupiah"',
 'code2currency("chf") eq "Swiss Franc"',
 'code2currency("mvr") eq "Rufiyaa"',
 'code2currency("mmk") eq "Kyat"',
 'code2currency("mwk") eq "Kwacha"',	# two different codes for Kwacha
 'code2currency("zmk") eq "Kwacha"',    # used in Zambia and Malawi
 'code2currency("byr") eq "Belarussian Ruble"',	# 2 codes for belarussian ruble
 'code2currency("byb") eq "Belarussian Ruble"', #
 'code2currency("rub") eq "Russian Ruble"',	# 2 codes for russian ruble
 'code2currency("rur") eq "Russian Ruble"',     #

 #---- some successful examples -----------------------------------------
 'code2currency("BOB") eq "Boliviano"',
 'code2currency("adp") eq "Andorran Peseta"',  # first in DATA segment
 'code2currency("zwd") eq "Zimbabwe Dollar"',  # last in DATA segment

	#================================================
	# TESTS FOR currency2code
	#================================================

 #---- selection of examples which should all result in undef -----------
 '!defined currency2code()',                 # no argument => undef returned
 '!defined currency2code(undef)',            # undef arg   => undef returned
 '!defined currency2code("")',               # empty string => undef returned
 '!defined currency2code("Banana")',         # illegal curr name => undef

 #---- some successful examples -----------------------------------------
 'currency2code("Kroon")           eq "eek"',
 'currency2code("Markka")         eq "fim"',
 'currency2code("Riel")            eq "khr"',
 'currency2code("PULA")            eq "bwp"',
 'currency2code("Andorran Peseta") eq "adp"',       # first in DATA segment
 'currency2code("Zimbabwe Dollar") eq "zwd"',       # last in DATA segment
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
