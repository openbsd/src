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

   if      ($test[0] eq "rename_language") {
      shift(@test);
      $test[2]  = $type{$test[2]}
        if (@test == 3  &&  $test[2]  &&  exists $type{$test[2]});
      return Locale::Codes::Language::rename_language(@test,"nowarn");

   } elsif ($test[0] eq "add_language") {
      shift(@test);
      $test[2]  = $type{$test[2]}
        if (@test == 3  &&  $test[2]  &&  exists $type{$test[2]});
      return Locale::Codes::Language::add_language(@test,"nowarn");

   } elsif ($test[0] eq "delete_language") {
      shift(@test);
      $test[1]  = $type{$test[1]}
        if (@test == 2  &&  $test[1]  &&  exists $type{$test[1]});
      return Locale::Codes::Language::delete_language(@test,"nowarn");

   } elsif ($test[0] eq "add_language_alias") {
      shift(@test);
      return Locale::Codes::Language::add_language_alias(@test,"nowarn");

   } elsif ($test[0] eq "delete_language_alias") {
      shift(@test);
      return Locale::Codes::Language::delete_language_alias(@test,"nowarn");

   } elsif ($test[0] eq "language2code") {
      shift(@test);
      $test[1]  = $type{$test[1]}
        if (@test == 2  &&  $test[1]  &&  exists $type{$test[1]});
      return language2code(@test);

   } else {
      $test[1]  = $type{$test[1]}
        if (@test == 2  &&  $test[1]  &&  exists $type{$test[1]});
      return code2language(@test);
   }
}

$tests = "

zu ~ Zulu

rename_language zu NewName LOCALE_LANG_FOO ~ 0

rename_language zu English LOCALE_LANG_ALPHA_2 ~ 0

rename_language zu NewName LOCALE_LANG_ALPHA_3 ~ 0

zu ~ Zulu

rename_language zu NewName LOCALE_LANG_ALPHA_2 ~ 1

zu ~ NewName

";

print "language (semi-private)...\n";
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
