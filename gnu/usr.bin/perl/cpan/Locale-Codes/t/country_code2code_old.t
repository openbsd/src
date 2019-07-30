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
use Locale::Codes::Constants;

%type = ( "LOCALE_CODE_ALPHA_2" => LOCALE_CODE_ALPHA_2,
          "LOCALE_CODE_ALPHA_3" => LOCALE_CODE_ALPHA_3,
          "LOCALE_CODE_NUMERIC" => LOCALE_CODE_NUMERIC,
        );

sub test {
   my($code,$type_in,$type_out) = @_;
   $type_in  = $type{$type_in}   if ($type_in   &&  exists $type{$type_in});
   $type_out = $type{$type_out}  if ($type_out  &&  exists $type{$type_out});

   return country_code2code($code,$type_in,$type_out);
}

$tests = "

bo LOCALE_CODE_ALPHA_2 LOCALE_CODE_ALPHA_2 ~ bo

bo LOCALE_CODE_ALPHA_3 LOCALE_CODE_ALPHA_3 ~ _undef_

zz LOCALE_CODE_ALPHA_2 LOCALE_CODE_ALPHA_3 ~ _undef_

zz LOCALE_CODE_ALPHA_3 LOCALE_CODE_ALPHA_3 ~ _undef_

zz LOCALE_CODE_ALPHA_2 0 ~ _undef_

bo LOCALE_CODE_ALPHA_2 0 ~ _undef_

_blank_ 0 0 ~ _undef_

BO  LOCALE_CODE_ALPHA_2 LOCALE_CODE_ALPHA_3 ~ bol

bol LOCALE_CODE_ALPHA_3 LOCALE_CODE_ALPHA_2 ~ bo

zwe LOCALE_CODE_ALPHA_3 LOCALE_CODE_ALPHA_2 ~ zw

858 LOCALE_CODE_NUMERIC LOCALE_CODE_ALPHA_3 ~ ury

858 LOCALE_CODE_NUMERIC LOCALE_CODE_ALPHA_3 ~ ury

tr  LOCALE_CODE_ALPHA_2 LOCALE_CODE_NUMERIC ~ 792

";

print "country_code2code (old)...\n";
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
