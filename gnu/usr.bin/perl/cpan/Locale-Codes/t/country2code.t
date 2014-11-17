#!/usr/bin/perl -w

require 5.002;

$runtests=shift(@ARGV);
if ( -f "t/testfunc.pl" ) {
  require "t/testfunc.pl";
  $dir="./lib";
  $tdir="t";
} elsif ( -f "testfunc.pl" ) {
  require "testfunc.pl";
  $dir="../lib";
  $tdir=".";
} else {
  die "ERROR: cannot find testfunc.pl\n";
}

unshift(@INC,$dir);
use Locale::Codes::Country;

%type = ( "LOCALE_CODE_ALPHA_2" => LOCALE_CODE_ALPHA_2,
          "LOCALE_CODE_ALPHA_3" => LOCALE_CODE_ALPHA_3,
          "LOCALE_CODE_NUMERIC" => LOCALE_CODE_NUMERIC,
          "LOCALE_CODE_DOM"     => LOCALE_CODE_DOM,
        );

sub test {
   my(@test) = @_;
   $test[1]  = $type{$test[1]}   if (@test == 2  &&  $test[1]  &&  exists $type{$test[1]});
   return country2code(@test);
}

$tests = "

kazakhstan
   ~
   kz

kazakstan
   ~
   kz

macao
   ~
   mo

macau
   ~
   mo


~ _undef_

_undef_
   ~
   _undef_

Banana
   ~
   _undef_

japan
   ~
   jp

Japan
   ~
   jp

United States
   ~
   us

United Kingdom
   ~
   gb

Andorra
   ~
   ad

Zimbabwe
   ~
   zw

Iran
   ~
   ir

North Korea
   ~
   kp

South Korea
   ~
   kr

Libya
   ~
   ly

Syrian Arab Republic
   ~
   sy

Svalbard
   ~
   _undef_

Jan Mayen
   ~
   _undef_

USA
   ~
   us

United States of America
   ~
   us

Great Britain
   ~
   gb

Burma
   ~
   mm

French Southern and Antarctic Lands
   ~
   tf

Aland Islands
   ~
   ax

Yugoslavia
   ~
   _undef_

Serbia and Montenegro
   ~
   _undef_

East Timor
   ~
   tl

Zaire
   ~
   _undef_

Zaire
retired
   ~
   zr

Congo, The Democratic Republic of the
   ~
   cd

Congo, The Democratic Republic of the
LOCALE_CODE_ALPHA_3
   ~
   cod

Congo, The Democratic Republic of the
LOCALE_CODE_NUMERIC
   ~
   180

Syria
   ~
   sy

# Last codes in each set (we'll assume that if we got these, there's a good
# possiblity that we got all the others).

Zimbabwe
LOCALE_CODE_ALPHA_2
   ~
   zw

Zimbabwe
LOCALE_CODE_ALPHA_3
   ~
   zwe

Zimbabwe
LOCALE_CODE_NUMERIC
   ~
   716

Zimbabwe
LOCALE_CODE_DOM
   ~
   zw

";

print "country2code...\n";
test_Func(\&test,$tests,$runtests);

1;
# Local Variables:
# mode: cperl
# indent-tabs-mode: nil
# cperl-indent-level: 3
# cperl-continued-statement-offset: 2
# cperl-continued-brace-offset: 0
# cperl-brace-offset: 0
# cperl-brace-imaginary-offset: 0
# cperl-label-offset: -2
# End:

