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
use Locale::Codes::Currency;

%type = ( "LOCALE_CURR_ALPHA"    => LOCALE_CURR_ALPHA,
          "LOCALE_CURR_NUMERIC"  => LOCALE_CURR_NUMERIC,
        );

sub test {
   my(@test) = @_;
   $test[1]  = $type{$test[1]}
     if (@test == 2  &&  $test[1]  &&  exists $type{$test[1]});
   return code2currency(@test);
}

$tests = "

ukp ~ _undef_

zz ~ _undef_

zzz ~ _undef_

zzzz ~ _undef_

~ _undef_

_undef_ ~ _undef_

BOB
   ~
   Boliviano

all
   ~
   Lek

bnd
   ~
   Brunei Dollar

bob
   ~
   Boliviano

byr
   ~
   Belarussian Ruble

chf
   ~
   Swiss Franc

cop
   ~
   Colombian Peso

dkk
   ~
   Danish Krone

fjd
   ~
   Fiji Dollar

idr
   ~
   Rupiah

mmk
   ~
   Kyat

mvr
   ~
   Rufiyaa

mwk
   ~
   Kwacha

rub
   ~
   Russian Ruble

zmw
   ~
   Zambian Kwacha

zwl
   ~
   Zimbabwe Dollar

";

print "code2currency...\n";
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
