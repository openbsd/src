#!./perl
#
# all.t - tests for all_* routines in
#	Locale::Country
#	Locale::Language
#	Locale::Currency
#	Locale::Script
#
# There are four tests. We get a list of all codes, convert to
# language/country/currency, # convert back to code,
# and check that they're the same. Then we do the same,
# starting with list of languages/countries/currencies.
#

use Locale::Country;
use Locale::Language;
use Locale::Currency;
use Locale::Script;

print "1..20\n";

my $code;
my $language;
my $country;
my $ok;
my $reverse;
my $currency;
my $script;


#-----------------------------------------------------------------------
# Old API - without codeset specified, default to ALPHA_2
#-----------------------------------------------------------------------
$ok = 1;
foreach $code (all_country_codes())
{
    $country = code2country($code);
    if (!defined $country)
    {
        $ok = 0;
        last;
    }
    $reverse = country2code($country);
    if (!defined $reverse)
    {
        $ok = 0;
        last;
    }
    if ($reverse ne $code)
    {
        $ok = 0;
        last;
    }
}
print ($ok ? "ok 1\n" : "not ok 1\n");

#-----------------------------------------------------------------------
# code to country, back to code, for ALPHA2
#-----------------------------------------------------------------------
$ok = 1;
foreach $code (all_country_codes(LOCALE_CODE_ALPHA_2))
{
    $country = code2country($code, LOCALE_CODE_ALPHA_2);
    if (!defined $country)
    {
        $ok = 0;
        last;
    }
    $reverse = country2code($country, LOCALE_CODE_ALPHA_2);
    if (!defined $reverse)
    {
        $ok = 0;
        last;
    }
    if ($reverse ne $code)
    {
        $ok = 0;
        last;
    }
}
print ($ok ? "ok 2\n" : "not ok 2\n");

#-----------------------------------------------------------------------
# code to country, back to code, for ALPHA3
#-----------------------------------------------------------------------
$ok = 1;
foreach $code (all_country_codes(LOCALE_CODE_ALPHA_3))
{
    $country = code2country($code, LOCALE_CODE_ALPHA_3);
    if (!defined $country)
    {
        $ok = 0;
        last;
    }
    $reverse = country2code($country, LOCALE_CODE_ALPHA_3);
    if (!defined $reverse)
    {
        $ok = 0;
        last;
    }
    if ($reverse ne $code)
    {
        $ok = 0;
        last;
    }
}
print ($ok ? "ok 3\n" : "not ok 3\n");

#-----------------------------------------------------------------------
# code to country, back to code, for NUMERIC
#-----------------------------------------------------------------------
$ok = 1;
foreach $code (all_country_codes(LOCALE_CODE_NUMERIC))
{
    $country = code2country($code, LOCALE_CODE_NUMERIC);
    if (!defined $country)
    {
        $ok = 0;
        last;
    }
    $reverse = country2code($country, LOCALE_CODE_NUMERIC);
    if (!defined $reverse)
    {
        $ok = 0;
        last;
    }
    if ($reverse ne $code)
    {
        $ok = 0;
        last;
    }
}
print ($ok ? "ok 4\n" : "not ok 4\n");


#-----------------------------------------------------------------------
# Old API - country to code, back to country, using default of ALPHA_2
#-----------------------------------------------------------------------
$ok = 1;
foreach $country (all_country_names())
{
    $code = country2code($country);
    if (!defined $code)
    {
        $ok = 0;
        last;
    }
    $reverse = code2country($code);
    if (!defined $reverse)
    {
        $ok = 0;
        last;
    }
    if ($reverse ne $country)
    {
        $ok = 0;
        last;
    }
}
print ($ok ? "ok 5\n" : "not ok 5\n");

#-----------------------------------------------------------------------
# country to code, back to country, using LOCALE_CODE_ALPHA_2
#-----------------------------------------------------------------------
$ok = 1;
foreach $country (all_country_names())
{
    $code = country2code($country, LOCALE_CODE_ALPHA_2);
    if (!defined $code)
    {
        $ok = 0;
        last;
    }
    $reverse = code2country($code, LOCALE_CODE_ALPHA_2);
    if (!defined $reverse)
    {
        $ok = 0;
        last;
    }
    if ($reverse ne $country)
    {
        $ok = 0;
        last;
    }
}
print ($ok ? "ok 6\n" : "not ok 6\n");

#-----------------------------------------------------------------------
# country to code, back to country, using LOCALE_CODE_ALPHA_3
#-----------------------------------------------------------------------
$ok = 1;
foreach $country (all_country_names())
{
    $code = country2code($country, LOCALE_CODE_ALPHA_3);
    if (!defined $code)
    {
	next if ($country eq 'Antarctica'
		 || $country eq 'Bouvet Island'
		 || $country eq 'Cocos (Keeling) Islands'
		 || $country eq 'Christmas Island'
		 || $country eq 'France, Metropolitan'
		 || $country eq 'South Georgia and the South Sandwich Islands'
		 || $country eq 'Heard Island and McDonald Islands'
		 || $country eq 'British Indian Ocean Territory'
		 || $country eq 'French Southern Territories'
		 || $country eq 'United States Minor Outlying Islands'
		 || $country eq 'Mayotte'
		 || $country eq 'Zaire');
        $ok = 0;
        last;
    }
    $reverse = code2country($code, LOCALE_CODE_ALPHA_3);
    if (!defined $reverse)
    {
        $ok = 0;
        last;
    }
    if ($reverse ne $country)
    {
        $ok = 0;
        last;
    }
}
print ($ok ? "ok 7\n" : "not ok 7\n");

#-----------------------------------------------------------------------
# country to code, back to country, using LOCALE_CODE_NUMERIC
#-----------------------------------------------------------------------
$ok = 1;
foreach $country (all_country_names())
{
    $code = country2code($country, LOCALE_CODE_NUMERIC);
    if (!defined $code)
    {
	next if ($country eq 'Antarctica'
		 || $country eq 'Bouvet Island'
		 || $country eq 'Cocos (Keeling) Islands'
		 || $country eq 'Christmas Island'
		 || $country eq 'France, Metropolitan'
		 || $country eq 'South Georgia and the South Sandwich Islands'
		 || $country eq 'Heard Island and McDonald Islands'
		 || $country eq 'British Indian Ocean Territory'
		 || $country eq 'French Southern Territories'
		 || $country eq 'United States Minor Outlying Islands'
		 || $country eq 'Mayotte'
		 || $country eq 'Zaire');
        $ok = 0;
        last;
    }
    $reverse = code2country($code, LOCALE_CODE_NUMERIC);
    if (!defined $reverse)
    {
        $ok = 0;
        last;
    }
    if ($reverse ne $country)
    {
        $ok = 0;
        last;
    }
}
print ($ok ? "ok 8\n" : "not ok 8\n");


$ok = 1;
foreach $code (all_language_codes())
{
    $language = code2language($code);
    if (!defined $language)
    {
        $ok = 0;
        last;
    }
    $reverse = language2code($language);
    if (!defined $reverse)
    {
        $ok = 0;
        last;
    }
    if ($reverse ne $code)
    {
        $ok = 0;
        last;
    }
}
print ($ok ? "ok 9\n" : "not ok 9\n");


$ok = 1;
foreach $language (all_language_names())
{
    $code = language2code($language);
    if (!defined $code)
    {
        $ok = 0;
        last;
    }
    $reverse = code2language($code);
    if (!defined $reverse)
    {
        $ok = 0;
        last;
    }
    if ($reverse ne $language)
    {
        $ok = 0;
        last;
    }
}
print ($ok ? "ok 10\n" : "not ok 10\n");

$ok = 1;
foreach $code (all_currency_codes())
{
    $currency = code2currency($code);
    if (!defined $currency)
    {
        $ok = 0;
        last;
    }
    $reverse = currency2code($currency);
    if (!defined $reverse)
    {
        $ok = 0;
        last;
    }
    #
    # three special cases:
    #	The Kwacha has two codes - used in Zambia and Malawi
    #	The Russian Ruble has two codes - rub and rur
    #	The Belarussian Ruble has two codes - byb and byr
    if ($reverse ne $code
	&& $code ne 'mwk' && $code ne 'zmk'
	&& $code ne 'byr' && $code ne 'byb'
	&& $code ne 'rub' && $code ne 'rur')
    {
        $ok = 0;
        last;
    }
}
print ($ok ? "ok 11\n" : "not ok 11\n");

$ok = 1;
foreach $currency (all_currency_names())
{
    $code = currency2code($currency);
    if (!defined $code)
    {
        $ok = 0;
        last;
    }
    $reverse = code2currency($code);
    if (!defined $reverse)
    {
        $ok = 0;
        last;
    }
    if ($reverse ne $currency)
    {
        $ok = 0;
        last;
    }
}
print ($ok ? "ok 12\n" : "not ok 12\n");

#=======================================================================
#
#	Locale::Script tests
#
#=======================================================================

#-----------------------------------------------------------------------
# Old API - without codeset specified, default to ALPHA_2
#-----------------------------------------------------------------------
$ok = 1;
foreach $code (all_script_codes())
{
    $script = code2script($code);
    if (!defined $script)
    {
        $ok = 0;
        last;
    }
    $reverse = script2code($script);
    if (!defined $reverse)
    {
        $ok = 0;
        last;
    }
    if ($reverse ne $code)
    {
        $ok = 0;
        last;
    }
}
print ($ok ? "ok 13\n" : "not ok 13\n");

#-----------------------------------------------------------------------
# code to script, back to code, for ALPHA2
#-----------------------------------------------------------------------
$ok = 1;
foreach $code (all_script_codes(LOCALE_CODE_ALPHA_2))
{
    $script = code2script($code, LOCALE_CODE_ALPHA_2);
    if (!defined $script)
    {
        $ok = 0;
        last;
    }
    $reverse = script2code($script, LOCALE_CODE_ALPHA_2);
    if (!defined $reverse)
    {
        $ok = 0;
        last;
    }
    if ($reverse ne $code)
    {
        $ok = 0;
        last;
    }
}
print ($ok ? "ok 14\n" : "not ok 14\n");

#-----------------------------------------------------------------------
# code to script, back to code, for ALPHA3
#-----------------------------------------------------------------------
$ok = 1;
foreach $code (all_script_codes(LOCALE_CODE_ALPHA_3))
{
    $script = code2script($code, LOCALE_CODE_ALPHA_3);
    if (!defined $script)
    {
        $ok = 0;
        last;
    }
    $reverse = script2code($script, LOCALE_CODE_ALPHA_3);
    if (!defined $reverse)
    {
        $ok = 0;
        last;
    }
    if ($reverse ne $code)
    {
        $ok = 0;
        last;
    }
}
print ($ok ? "ok 15\n" : "not ok 15\n");

#-----------------------------------------------------------------------
# code to script, back to code, for NUMERIC
#-----------------------------------------------------------------------
$ok = 1;
foreach $code (all_script_codes(LOCALE_CODE_NUMERIC))
{
    $script = code2script($code, LOCALE_CODE_NUMERIC);
    if (!defined $script)
    {
        $ok = 0;
        last;
    }
    $reverse = script2code($script, LOCALE_CODE_NUMERIC);
    if (!defined $reverse)
    {
        $ok = 0;
        last;
    }
    if ($reverse ne $code)
    {
        $ok = 0;
        last;
    }
}
print ($ok ? "ok 16\n" : "not ok 16\n");


#-----------------------------------------------------------------------
# Old API - script to code, back to script, using default of ALPHA_2
#-----------------------------------------------------------------------
$ok = 1;
foreach $script (all_script_names())
{
    $code = script2code($script);
    if (!defined $code)
    {
        $ok = 0;
        last;
    }
    $reverse = code2script($code);
    if (!defined $reverse)
    {
        $ok = 0;
        last;
    }
    if ($reverse ne $script)
    {
        $ok = 0;
        last;
    }
}
print ($ok ? "ok 17\n" : "not ok 17\n");

#-----------------------------------------------------------------------
# script to code, back to script, using LOCALE_CODE_ALPHA_2
#-----------------------------------------------------------------------
$ok = 1;
foreach $script (all_script_names())
{
    $code = script2code($script, LOCALE_CODE_ALPHA_2);
    if (!defined $code)
    {
        $ok = 0;
        last;
    }
    $reverse = code2script($code, LOCALE_CODE_ALPHA_2);
    if (!defined $reverse)
    {
        $ok = 0;
        last;
    }
    if ($reverse ne $script)
    {
        $ok = 0;
        last;
    }
}
print ($ok ? "ok 18\n" : "not ok 18\n");

#-----------------------------------------------------------------------
# script to code, back to script, using LOCALE_CODE_ALPHA_3
#-----------------------------------------------------------------------
$ok = 1;
foreach $script (all_script_names())
{
    $code = script2code($script, LOCALE_CODE_ALPHA_3);
    if (!defined $code)
    {
        $ok = 0;
        last;
    }
    $reverse = code2script($code, LOCALE_CODE_ALPHA_3);
    if (!defined $reverse)
    {
        $ok = 0;
        last;
    }
    if ($reverse ne $script)
    {
        $ok = 0;
        last;
    }
}
print ($ok ? "ok 19\n" : "not ok 19\n");

#-----------------------------------------------------------------------
# script to code, back to script, using LOCALE_CODE_NUMERIC
#-----------------------------------------------------------------------
$ok = 1;
foreach $script (all_script_names())
{
    $code = script2code($script, LOCALE_CODE_NUMERIC);
    if (!defined $code)
    {
        $ok = 0;
        last;
    }
    $reverse = code2script($code, LOCALE_CODE_NUMERIC);
    if (!defined $reverse)
    {
        $ok = 0;
        last;
    }
    if ($reverse ne $script)
    {
        $ok = 0;
        last;
    }
}
print ($ok ? "ok 20\n" : "not ok 20\n");

