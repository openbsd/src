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

   if ($test[0] eq "alias_code") {
      shift(@test);
      $test[2]  = $type{$test[2]}
        if (@test == 3  &&  $test[2]  &&  exists $type{$test[2]});
      return Locale::Codes::Country::alias_code(@test,"nowarn");

   } elsif ($test[0] eq "country2code") {
      shift(@test);
      $test[1]  = $type{$test[1]}   if (@test == 2  &&  $test[1]  &&  exists $type{$test[1]});
      return country2code(@test);

   } else {
      $test[1]  = $type{$test[1]}
        if (@test == 2  &&  $test[1]  &&  exists $type{$test[1]});
      return code2country(@test);
   }
}

$tests = "

gb
   ~
   United Kingdom

uk
   ~
   _undef_

country2code
United Kingdom
   ~
   gb

alias_code uk gb LOCALE_CODE_FOO ~ 0

alias_code uk x1 ~ 0

alias_code us gb ~ 0

alias_code uk gb LOCALE_CODE_ALPHA_3 ~ 0

gb
   ~
   United Kingdom

uk
   ~
   _undef_

country2code
United Kingdom
   ~
   gb

alias_code uk gb ~ uk

gb
   ~
   United Kingdom

uk
   ~
   United Kingdom

country2code
United Kingdom
   ~
   uk

";

print "alias_code...\n";
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
