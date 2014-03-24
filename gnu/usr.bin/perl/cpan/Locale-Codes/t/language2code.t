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
use Locale::Codes::Language;

%type = ( "LOCALE_LANG_ALPHA_2" => LOCALE_LANG_ALPHA_2,
          "LOCALE_LANG_ALPHA_3" => LOCALE_LANG_ALPHA_3,
          "LOCALE_LANG_TERM"    => LOCALE_LANG_TERM,
        );

sub test {
   my(@test) = @_;
   $test[1]  = $type{$test[1]}
     if (@test == 2  &&  $test[1]  &&  exists $type{$test[1]});
   return language2code(@test);
}

$tests = "

Banana ~ _undef_

~ _undef_

_undef_ ~ _undef_

Afar
   ~
   aa

ESTONIAN
   ~
   et

French
   ~
   fr

Greek
   ~
   el

Japanese
   ~
   ja

Zulu
   ~
   zu

english
   ~
   en

japanese
   ~
   ja

# Last ones in the list

Zulu
LOCALE_LANG_ALPHA_2
   ~
   zu

Zaza
LOCALE_LANG_ALPHA_3
   ~
   zza

Welsh
LOCALE_LANG_TERM
   ~
   cym

Zande languages
LOCALE_LANG_ALPHA_3
   ~
   znd

Zuojiang Zhuang
LOCALE_LANG_ALPHA_3
   ~
   zzj

";

print "language2code...\n";
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
