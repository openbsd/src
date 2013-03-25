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
use Locale::Currency;

%type = ( "LOCALE_CURR_ALPHA"    => LOCALE_CURR_ALPHA,
          "LOCALE_CURR_NUMERIC"  => LOCALE_CURR_NUMERIC,
        );

sub test {
   my(@test) = @_;
   $test[1]  = $type{$test[1]}
     if (@test == 2  &&  $test[1]  &&  exists $type{$test[1]});
   return currency2code(@test);
}

$tests = "

_blank_ ~ _undef_

Banana ~ _undef_

~ _undef_

_undef_ ~ _undef_


Canadian Dollar
   ~
   CAD

Belize Dollar
   ~
   BZD

PULA
   ~
   BWP

Riel
   ~
   KHR

Zimbabwe Dollar
   ~
   ZWL

";

print "currency2code (old)...\n";
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
