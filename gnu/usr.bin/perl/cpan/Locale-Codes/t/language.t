#!./perl
#
# language.t - tests for Locale::Language
#

BEGIN {
	chdir 't' if -d 't';
	#@INC = '../lib';
}

use Locale::Language;

no utf8; # we contain Latin-1

#-----------------------------------------------------------------------
# This is an array of tests. Each test is eval'd as an expression.
# If it evaluates to FALSE, then "not ok N" is printed for the test,
# otherwise "ok N".
#-----------------------------------------------------------------------
@TESTS =
(
	#================================================
	# TESTS FOR code2language
	#================================================

 #---- selection of examples which should all result in undef -----------
 '!defined code2language()',                 # no argument => undef returned
 '!defined code2language(undef)',            # undef arg   => undef returned
 '!defined code2language("zz")',             # illegal code => undef
 '!defined code2language("jp")',             # ja for lang, jp for country

 #---- test recent changes ----------------------------------------------
 'code2language("ae") eq "Avestan"',
 'code2language("bs") eq "Bosnian"',
 'code2language("ch") eq "Chamorro"',
 'code2language("ce") eq "Chechen"',
 'code2language("cu") eq "Church Slavic"',
 'code2language("cv") eq "Chuvash"',
 'code2language("hz") eq "Herero"',
 'code2language("ho") eq "Hiri Motu"',
 'code2language("ki") eq "Kikuyu"',
 'code2language("kj") eq "Kuanyama"',
 'code2language("kv") eq "Komi"',
 'code2language("mh") eq "Marshall"',
 'code2language("nv") eq "Navajo"',
 'code2language("nr") eq "Ndebele, South"',
 'code2language("nd") eq "Ndebele, North"',
 'code2language("ng") eq "Ndonga"',
 'code2language("nn") eq "Norwegian Nynorsk"',
 'code2language("nb") eq "Norwegian Bokmal"',
 'code2language("ny") eq "Chichewa; Nyanja"',
 'code2language("oc") eq "Occitan (post 1500)"',
 'code2language("os") eq "Ossetian; Ossetic"',
 'code2language("pi") eq "Pali"',
 '!defined code2language("sh")',             # Serbo-Croatian withdrawn
 'code2language("se") eq "Sami"',
 'code2language("sc") eq "Sardinian"',
 'code2language("kw") eq "Cornish"',
 'code2language("gv") eq "Manx"',
 'code2language("lb") eq "Letzeburgesch"',
 'code2language("he") eq "Hebrew"',
 '!defined code2language("iw")',             # Hebrew withdrawn
 'code2language("id") eq "Indonesian"',
 '!defined code2language("in")',             # Indonesian withdrawn
 'code2language("iu") eq "Inuktitut"',
 'code2language("ug") eq "Uighur"',
 '!defined code2language("ji")',             # Yiddish withdrawn
 'code2language("yi") eq "Yiddish"',
 'code2language("za") eq "Zhuang"',

 #---- some successful examples -----------------------------------------
 'code2language("DA") eq "Danish"',
 'code2language("eo") eq "Esperanto"',
 'code2language("fi") eq "Finnish"',
 'code2language("en") eq "English"',
 'code2language("aa") eq "Afar"',            # first in DATA segment
 'code2language("zu") eq "Zulu"',            # last in DATA segment

	#================================================
	# TESTS FOR language2code
	#================================================

 #---- selection of examples which should all result in undef -----------
 '!defined language2code()',                 # no argument => undef returned
 '!defined language2code(undef)',            # undef arg   => undef returned
 '!defined language2code("Banana")',         # illegal lang name => undef

 #---- some successful examples -----------------------------------------
 'language2code("Japanese")  eq "ja"',
 'language2code("japanese")  eq "ja"',
 'language2code("japanese")  ne "jp"',
 'language2code("French")    eq "fr"',
 'language2code("Greek")     eq "el"',
 'language2code("english")   eq "en"',
 'language2code("ESTONIAN")  eq "et"',
 'language2code("Afar")      eq "aa"',       # first in DATA segment
 'language2code("Zulu")      eq "zu"',       # last in DATA segment
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
