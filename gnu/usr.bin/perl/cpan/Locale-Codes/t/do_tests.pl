#!/usr/bin/perl
# Copyright (c) 2016-2018 Sullivan Beck. All rights reserved.
# This program is free software; you can redistribute it and/or modify it
# under the same terms as Perl itself.

use warnings;
use strict;
no strict 'subs';
no strict 'refs';

my %type = ('country'  => 'Country',
            'language' => 'Language',
            'currency' => 'Currency',
            'script'   => 'Script',
            'langfam'  => 'LangFam',
            'langext'  => 'LangExt',
            'langvar'  => 'LangVar',
           );
my $generic_tests;

sub do_tests {
   my($data_type,$inp_file,$test_type,$codeset,$show_errs) = @_;
   my $type = $type{$data_type};
   $::data_type = $data_type;
   $::test_type = $test_type;
   $inp_file    = $data_type  if (! $inp_file);

   my($runtests) = shift(@ARGV);

   # Load the test function and the data for the tests

   my($dir,$tdir);
   if ( -f "t/testfunc.pl" ) {
     require "./t/testfunc.pl";
     require "./t/vals_${inp_file}.pl";
     $dir="./lib";
     $tdir="t";
   } elsif ( -f "testfunc.pl" ) {
     require "./testfunc.pl";
     require "./vals_${inp_file}.pl";
     $dir="../lib";
     $tdir=".";
   } else {
     die "ERROR: cannot find testfunc.pl\n";
   }

   unshift(@INC,$dir);

   $::tests .= $generic_tests  if (! defined($show_errs));

   if ($test_type eq 'old') {
      $::module = "Locale::$type";
      eval("use $::module");
      my $tmp   = $::module . "::show_errors";
      &{ $tmp }(0);
   } elsif ($test_type eq 'func') {
      $::module = "Locale::Codes::$type";
      eval("use $::module");
      my $tmp   = $::module . "::show_errors";
      &{ $tmp }(0);
   } elsif (defined($codeset)) {
      eval("use Locale::Codes");
      $::obj = Locale::Codes->new($data_type,$codeset,$show_errs);
      $::obj->show_errors(1);
   } elsif (defined($show_errs)) {
      eval("use Locale::Codes");
      $::obj = Locale::Codes->new();
      $::obj->type($data_type);
      $::obj->show_errors($show_errs);
   } else {
      eval("use Locale::Codes");
      $::obj = new Locale::Codes $data_type;
      $::obj->show_errors(0);
   }

   print "$::data_type [$::test_type]\n";
   test_Func(\&test,$::tests,$runtests);
}

sub test {
   my ($op,@test) = @_;
   my @ret;

   my $stderr = '';
   {
      local *STDERR;
      open STDERR, '>', \$stderr;
      @ret = _test($op,@test);
   }

   if ($stderr) {
      $stderr =~ s/\n.*//m;
      chomp($stderr);
      return $stderr;
   } else {
      return @ret;
   }
}

sub _test {
   my    ($op,@test) = @_;

   if ($op eq '2code') {
      my $code;
      if ($::obj) {
         $code = $::obj->name2code(@test);
      } else {
         $code = &{ "${::data_type}2code" }(@test);
      }
      return ($code ? lc($code) : $code);

   } elsif ($op eq '2name') {
      if ($::obj) {
         return $::obj->code2name(@test);
      } else {
         return &{ "code2${::data_type}" }(@test)
      }

   } elsif ($op eq 'code2code') {
      my $code;
      if ($::obj) {
         $code = $::obj->code2code(@test);
      } else {
         $code = &{ "${::data_type}_code2code" }(@test);
      }
      return ($code ? lc($code) : $code);

   } elsif ($op eq 'all_codes') {
      my $n;
      if ($test[$#test] =~ /^\d+$/) {
         $n = pop(@test);
      }

      my @tmp;
      if ($::obj) {
         @tmp = $::obj->all_codes(@test);
      } else {
         @tmp = &{ "all_${::data_type}_codes" }(@test);
      }

      if ($n  &&  @tmp > $n) {
         return @tmp[0..($n-1)];
      } else {
         return @tmp;
      }

   } elsif ($op eq 'all_names') {
      my $n;
      if ($test[$#test] =~ /^\d+$/) {
         $n = pop(@test);
      }

      my @tmp;
      if ($::obj) {
         @tmp = $::obj->all_names(@test);
      } else {
         @tmp = &{ "all_${::data_type}_names" }(@test);
      }

      if ($n  &&  @tmp > $n) {
         return @tmp[0..($n-1)];
      } else {
         return @tmp;
      }

   } elsif ($op eq 'rename') {
      if ($::obj) {
         return $::obj->rename_code(@test);
      } else {
         return &{ "${::module}::rename_${::data_type}" }(@test)
      }
   } elsif ($op eq 'add') {
      if ($::obj) {
         return $::obj->add_code(@test);
      } else {
         return &{ "${::module}::add_${::data_type}" }(@test)
      }
   } elsif ($op eq 'delete') {
      if ($::obj) {
         return $::obj->delete_code(@test);
      } else {
         return &{ "${::module}::delete_${::data_type}" }(@test)
      }
   } elsif ($op eq 'add_alias') {
      if ($::obj) {
         return $::obj->add_alias(@test);
      } else {
         return &{ "${::module}::add_${::data_type}_alias" }(@test)
      }
   } elsif ($op eq 'delete_alias') {
      if ($::obj) {
         return $::obj->delete_alias(@test);
      } else {
         return &{ "${::module}::delete_${::data_type}_alias" }(@test)
      }
   } elsif ($op eq 'replace_code') {
      if ($::obj) {
         return $::obj->replace_code(@test);
      } else {
         return &{ "${::module}::rename_${::data_type}_code" }(@test)
      }
   } elsif ($op eq 'add_code_alias') {
      if ($::obj) {
         return $::obj->add_code_alias(@test);
      } else {
         return &{ "${::module}::add_${::data_type}_code_alias" }(@test)
      }
   } elsif ($op eq 'delete_code_alias') {
      if ($::obj) {
         return $::obj->delete_code_alias(@test);
      } else {
         return &{ "${::module}::delete_${::data_type}_code_alias" }(@test)
      }
   } elsif ($op eq 'codeset') {
      if ($::obj) {
         return $::obj->codeset(@test);
      } else {
         return &{ "${::module}::codeset" }(@test)
      }
   } elsif ($op eq 'type') {
      if ($::obj) {
         return $::obj->type(@test);
      } else {
         return &{ "${::module}::type" }(@test)
      }
   }
}

$generic_tests = "
#################

2code
_undef_
   _undef_

2code
   _undef_

2code
_blank_
   _undef_

2code
UnusedName
   _undef_

2code
   _undef_

2code
_undef_
   _undef_

2name
_undef
   _undef_

2name
   _undef_

###

add
AAA
newCode
   1

2code
newCode
   aaa

delete
AAA
   1

2code
newCode
   _undef_

###

add
AAA
newCode
   1

rename
AAA
newCode2
   1

2code
newCode
   aaa

2code
newCode2
   aaa

###

add_alias
newCode2
newAlias
   1

2code
newAlias
   aaa

delete_alias
newAlias
   1

2code
newAlias
   _undef_

###

replace_code
AAA
BBB
   1

2name
AAA
   newCode2

2name
BBB
   newCode2

###

add_code_alias
BBB
CCC
   1

2name
BBB
   newCode2

2name
CCC
   newCode2

delete_code_alias
CCC
   1

2name
CCC
   _undef_

";

1;
# Local Variables:
# mode: cperl
# indent-tabs-mode: nil
# cperl-indent-level: 3
# cperl-continued-statement-offset: 2
# cperl-continued-brace-offset: 0
# cperl-brace-offset: 0
# cperl-brace-imaginary-offset: 0
# cperl-label-offset: 0
# End:

