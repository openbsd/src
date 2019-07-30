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
        );

sub test {
   my(@test) = @_;

   if      ($test[0] eq "rename_country") {
      shift(@test);
      $test[2]  = $type{$test[2]}
        if (@test == 3  &&  $test[2]  &&  exists $type{$test[2]});
      return Locale::Codes::Country::rename_country(@test,"nowarn");

   } elsif ($test[0] eq "add_country") {
      shift(@test);
      $test[2]  = $type{$test[2]}
        if (@test == 3  &&  $test[2]  &&  exists $type{$test[2]});
      return Locale::Codes::Country::add_country(@test,"nowarn");

   } elsif ($test[0] eq "delete_country") {
      shift(@test);
      $test[1]  = $type{$test[1]}
        if (@test == 2  &&  $test[1]  &&  exists $type{$test[1]});
      return Locale::Codes::Country::delete_country(@test,"nowarn");

   } elsif ($test[0] eq "add_country_alias") {
      shift(@test);
      return Locale::Codes::Country::add_country_alias(@test,"nowarn");

   } elsif ($test[0] eq "delete_country_alias") {
      shift(@test);
      return Locale::Codes::Country::delete_country_alias(@test,"nowarn");

   } elsif ($test[0] eq "rename_country_code") {
      shift(@test);
      $test[2]  = $type{$test[2]}
        if (@test == 3  &&  $test[2]  &&  exists $type{$test[2]});
      return Locale::Codes::Country::rename_country_code(@test,"nowarn");

   } elsif ($test[0] eq "add_country_code_alias") {
      shift(@test);
      $test[2]  = $type{$test[2]}
        if (@test == 3  &&  $test[2]  &&  exists $type{$test[2]});
      return Locale::Codes::Country::add_country_code_alias(@test,"nowarn");

   } elsif ($test[0] eq "delete_country_code_alias") {
      shift(@test);
      $test[1]  = $type{$test[1]}
        if (@test == 2  &&  $test[1]  &&  exists $type{$test[1]});
      return Locale::Codes::Country::delete_country_code_alias(@test,"nowarn");

   } elsif ($test[0] eq "country2code") {
      shift(@test);
      $test[1]  = $type{$test[1]}
        if (@test == 2  &&  $test[1]  &&  exists $type{$test[1]});
      return country2code(@test);

   } else {
      shift(@test)  if ($test[0] eq "code2country");
      $test[1]  = $type{$test[1]}
        if (@test == 2  &&  $test[1]  &&  exists $type{$test[1]});
      return code2country(@test);
   }
}

$tests = "

###################################
# Test rename_country

gb
   ~
   United Kingdom of Great Britain and Northern Ireland

rename_country x1 NewName ~ 0

rename_country gb NewName LOCALE_CODE_FOO ~ 0

rename_country gb Macao ~ 0

rename_country gb NewName LOCALE_CODE_ALPHA_3 ~ 0

gb
   ~
   United Kingdom of Great Britain and Northern Ireland

rename_country gb NewName ~ 1

gb
   ~
   NewName

us
   ~
   United States of America

rename_country
us
The United States
   ~
   1

us
   ~
   The United States

###################################
# Test add_country

xx ~ _undef_

add_country xx Bolivia ~ 0

add_country fi Xxxxx ~ 0

add_country xx Xxxxx ~ 1

xx ~ Xxxxx

###################################
# Test add_country_alias

add_country_alias FooBar NewName ~ 0

add_country_alias Australia Angola ~ 0

country2code Australia ~ au

country2code DownUnder ~ _undef_

add_country_alias Australia DownUnder ~ 1

country2code DownUnder ~ au

###################################
# Test delete_country_alias

country2code uk ~ gb

delete_country_alias Foobar ~ 0

delete_country_alias UK ~ 1

country2code uk ~ _undef_

delete_country_alias Angola ~ 0

###################################
# Test delete_country

country2code Angola                     ~ ao

country2code Angola LOCALE_CODE_ALPHA_3 ~ ago

delete_country ao                       ~ 1

country2code Angola                     ~ _undef_

country2code Angola LOCALE_CODE_ALPHA_3 ~ ago

###################################
# Test rename_country_code

code2country zz           ~ _undef_

code2country ar           ~ Argentina

country2code Argentina    ~ ar

rename_country_code ar us ~ 0

rename_country_code ar zz ~ 1

rename_country_code us ar ~ 0

code2country zz           ~ Argentina

code2country ar           ~ Argentina

country2code Argentina    ~ zz

rename_country_code zz ar ~ 1

code2country zz           ~ Argentina

code2country ar           ~ Argentina

country2code Argentina    ~ ar

###################################
# Test add_country_code_alias and
# delete_country_code_alias

code2country bm              ~ Bermuda

code2country yy              ~ _undef_

country2code Bermuda         ~ bm

add_country_code_alias bm us ~ 0

add_country_code_alias bm zz ~ 0

add_country_code_alias bm yy ~ 1

code2country bm              ~ Bermuda

code2country yy              ~ Bermuda

country2code Bermuda         ~ bm

delete_country_code_alias us ~ 0

delete_country_code_alias ww ~ 0

delete_country_code_alias yy ~ 1

code2country bm              ~ Bermuda

code2country yy              ~ _undef_

country2code Bermuda         ~ bm

";

print "country (semi-private)...\n";
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
