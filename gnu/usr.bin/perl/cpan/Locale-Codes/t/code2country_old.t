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
use Locale::Country;

%type = ( "LOCALE_CODE_ALPHA_2" => LOCALE_CODE_ALPHA_2,
          "LOCALE_CODE_ALPHA_3" => LOCALE_CODE_ALPHA_3,
          "LOCALE_CODE_NUMERIC" => LOCALE_CODE_NUMERIC,
        );

sub test {
   my(@test) = @_;
   $test[1]  = $type{$test[1]}   if (@test == 2  &&  $test[1]  &&  exists $type{$test[1]});
   return code2country(@test);
}

$tests = "

~ _undef_

_undef_ ~ _undef_

zz ~ _undef_

zz LOCALE_CODE_ALPHA_2 ~ _undef_

zz LOCALE_CODE_ALPHA_3 ~ _undef_

zz LOCALE_CODE_NUMERIC ~ _undef_

ja ~ _undef_

uk ~ _undef_

BO
   ~
   Bolivia, Plurinational State of

BO
LOCALE_CODE_ALPHA_2
   ~
   Bolivia, Plurinational State of

bol
LOCALE_CODE_ALPHA_3
   ~
   Bolivia, Plurinational State of

pk ~ Pakistan

sn ~ Senegal

us
   ~
   United States

ad ~ Andorra

ad LOCALE_CODE_ALPHA_2 ~ Andorra

and LOCALE_CODE_ALPHA_3 ~ Andorra

020 LOCALE_CODE_NUMERIC ~ Andorra

48 LOCALE_CODE_NUMERIC ~ Bahrain

zw ~ Zimbabwe

gb
   ~
   United Kingdom

kz ~ Kazakhstan

mo ~ Macao

tl LOCALE_CODE_ALPHA_2 ~ Timor-Leste

tls LOCALE_CODE_ALPHA_3 ~ Timor-Leste

626 LOCALE_CODE_NUMERIC ~ Timor-Leste

BO LOCALE_CODE_ALPHA_3 ~ _undef_

BO LOCALE_CODE_NUMERIC ~ _undef_

ax
   ~
   Aland Islands

ala
LOCALE_CODE_ALPHA_3
   ~
   Aland Islands

248
LOCALE_CODE_NUMERIC
   ~
   Aland Islands

scg
LOCALE_CODE_ALPHA_3
   ~
   _undef_

891
LOCALE_CODE_NUMERIC
   ~
   _undef_

rou LOCALE_CODE_ALPHA_3 ~ Romania

";

print "code2country (old)...\n";
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

