#!./perl
#
# constants.t - tests for Locale::Constants
#

use Locale::Constants;

print "1..3\n";

if (defined LOCALE_CODE_ALPHA_2
    && defined LOCALE_CODE_ALPHA_3
    && defined LOCALE_CODE_NUMERIC)
{
    print "ok 1\n";
}
else
{
    print "not ok 1\n";
}

if (LOCALE_CODE_ALPHA_2 != LOCALE_CODE_ALPHA_3
    && LOCALE_CODE_ALPHA_2 != LOCALE_CODE_NUMERIC
    && LOCALE_CODE_ALPHA_3 != LOCALE_CODE_NUMERIC)
{
    print "ok 2\n";
}
else
{
    print "not ok 2\n";
}

if (defined LOCALE_CODE_DEFAULT
    && (LOCALE_CODE_DEFAULT == LOCALE_CODE_ALPHA_2
	|| LOCALE_CODE_DEFAULT == LOCALE_CODE_ALPHA_3
	|| LOCALE_CODE_DEFAULT == LOCALE_CODE_NUMERIC))
{
    print "ok 3\n";
}
else
{
    print "not ok 3\n";
}

exit 0;
