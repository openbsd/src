#!/usr/bin/perl -w
# Copyright (c) 1996-2014 Sullivan Beck. All rights reserved.
# This program is free software; you can redistribute it and/or modify it
# under the same terms as Perl itself.

# SB_TEST.PL
###############################################################################
# HISTORY
#
# 1996-??-??  Wrote initial version for Date::Manip module
#
# 1996-2001   Numerous changes
#
# 2001-03-29  Rewrote to make it easier to drop in for any module.
#
# 2001-06-19  Modifications to make space delimited stuff work better.
#
# 2001-08-23  Added support for undef args.
#
# 2007-08-14  Better support for undef/blank args.
#
# 2008-01-02  Better handling of $runtests.
#
# 2008-01-24  Better handling of undef/blank args when arguements are
#             entered as lists instead of strings.
#
# 2008-01-25  Created a global $testnum variable to store the test number
#             in.
#
# 2008-11-05  Slightly better handling of blank/undef in returned values.
#
# 2009-09-01  Added "-l" value to $runtests.
#
# 2009-09-30  Much better support for references.
#
# 2010-02-05  Fixed bug in passing tests as lists
#
# 2010-04-05  Renamed to testfunc.pl to avoid being called in a core module

###############################################################################

use Storable qw(dclone);

# Usage: test_Func($funcref,$tests,$runtests,@extra)=@_;
#
# This takes a series of tests, runs them, compares the output of the tests
# with expected output, and reports any differences.  Each test consists of
# several parts:
#    a function passed in as a reference ($funcref)
#    a series of arguments to be passed to the function
#    the expected output from the function call
#
# Tests may be passed in in two methods: as a string, or as a reference.
#
# Using the string case, $tests is a newline delimited string.  Each test
# takes one or more lines of the string.  Tests are separated from each
# other by a blank line.
#
# Arguments and return value(s) may be written as a single line:
#    ARG1 ARG2 ... ARGn ~ VAL1 VAL2 ... VALm
# or as multiple lines:
#    ARG1
#    ARG2
#    ...
#    ARGn
#    ~
#    VAL1
#    VAL2
#    ...
#    VALm
#
# If any of the arguments OR values have spaces in them, only the multiline
# form may be used.
#
# If there is exactly one return value, the separating tilde is
# optional:
#    ARG1 ARG2 ... ARGn VAL1
# or:
#    ARG1
#    ARG2
#    ...
#    ARGn
#    VAL
#
# It is valid to have a function with no arguments or with no return
# value (or both).  The "~" must be used:
#
#    ARG1 ARG2 ... ARGn ~
#
#    ~ VAL1 VAL2 ... VALm
#
#    ~
#
# Leading and trailing space is ignored in the multi-line format.
#
# If desired, any of the ARGs or VALs may be the word "_undef_" which
# will be strictly interpreted as the perl undef value. The word "_blank_"
# may also be used to designate a defined but empty string.
#
# They may also be (in the multiline format) of the form:
#
#   \ STRING           : a string reference
#
#   [] LIST            : a list reference (where LIST is a
#                        comma separated list)
#
#   [SEP] LIST         : a list reference (where SEP is a
#                        single character separator)
#
#   {} HASH            : a hash reference (where HASH is
#                        a comma separated list)
#
#   {SEP} HASH         : a hash reference (where SEP is a
#                        single character separator)
#
# Alternately, the tests can be passed in as a list reference:
#    $tests = [
#               [
#                 [ @ARGS1 ],
#                 [ @VALS1 ]
#               ],
#               [
#                 [ @ARGS2 ],
#                 [ @VALS2 ]
#               ], ...
#             ]
#
# @extra are extra arguments which are added to the function call.
#
# There are several ways to run the tests, depending on the value of
# $runtests.
#
# If $runtests is 0, the tests are run in a non-interactive way suitable
# for running as part of a "make test".
#
# If $runtests is a positive number, it runs runs all tests starting at
# that value in a way suitable for running interactively.
#
# If $runtests is a negative number, it runs all tests starting at that
# value, but providing feedback at each test.
#
# If $runtests is a string "=N" (where N is a number), it runs only
# that test.
#
# If $runtests is the string "-l", it lists the tests and the expected
# output without running any.

sub test_Func {
   my($funcref,$tests,$runtests,@extra)=@_;
   my(@tests);

   $runtests     = 0  if (! $runtests);
   my($starttest,$feedback,$endtest,$runtest);
   if      ($runtests eq "0"  or  $runtests eq "-0") {
      $starttest = 1;
      $feedback  = 1;
      $endtest   = 0;
      $runtest   = 1;
   } elsif ($runtests =~ /^\d+$/){
      $starttest = $runtests;
      $feedback  = 0;
      $endtest   = 0;
      $runtest   = 1;
   } elsif ($runtests =~ /^-(\d+)$/) {
      $starttest = $1;
      $feedback  = 1;
      $endtest   = 0;
      $runtest   = 1;
   } elsif ($runtests =~ /^=(\d+)$/) {
      $starttest = $1;
      $feedback  = 1;
      $endtest   = $1;
      $runtest   = 1;
   } elsif ($runtests eq "-l") {
      $starttest = 1;
      $feedback  = 1;
      $endtest   = 0;
      $runtest   = 0;
   } else {
      die "ERROR: unknown argument(s): $runtests";
   }

   my($tests_as_list) = 0;
   if (ref($tests) eq "ARRAY") {
      @tests   = @$tests;
      $tests_as_list = 1;

   } else {
      # Separate tests.

      my($comment)="#";
      my(@lines)=split(/\n/,$tests);
      my(@test);
      while (@lines) {
         my $line = shift(@lines);
         $line =~ s/^\s*//;
         $line =~ s/\s*$//;
         next  if ($line =~ /^$comment/);

         if ($line ne "") {
            push(@test,$line);
            next;
         }

         if (@test) {
            push(@tests,[ @test ]);
            @test=();
         }
      }
      if (@test) {
         push(@tests,[ @test ]);
      }

      # Get arg/val lists for each test.

      foreach my $test (@tests) {
         my(@tmp)=@$test;
         my(@arg,@val);

         # single line test
         @tmp = split(/\s+/,$tmp[0])  if ($#tmp == 0);

         my($sep)=-1;
         my($i);
         for ($i=0; $i<=$#tmp; $i++) {
            if ($tmp[$i] eq "~") {
               $sep=$i;
               last;
            }
         }

         if ($sep<0) {
            @val=pop(@tmp);
            @arg=@tmp;
         } else {
            @arg=@tmp[0..($sep-1)];
            @val=@tmp[($sep+1)..$#tmp];
         }
         $test = [ [@arg],[@val] ];
      }
   }

   my($ntest)=$#tests + 1;
   print "1..$ntest\n"  if ($feedback  &&  $runtest);

   my(@t);
   if ($endtest) {
      @t = ($starttest..$endtest);
   } else {
      @t = ($starttest..$ntest);
   }

   foreach my $t (@t) {
      $::testnum  = $t;

      my (@arg);
      if ($tests_as_list) {
         @arg     = @{ $tests[$t-1][0] };
      } else {
         my $arg  = dclone($tests[$t-1][0]);
         @arg     = @$arg;
         print_to_vals(\@arg);
      }

      my $argprt  = dclone(\@arg);
      my @argprt  = @$argprt;
      vals_to_print(\@argprt);

      my $exp     = dclone($tests[$t-1][1]);
      my @exp     = @$exp;
      print_to_vals(\@exp);
      vals_to_print(\@exp);

      # Run the test

      my ($ans,@ans);
      if ($runtest) {
         @ans = &$funcref(@arg,@extra);
      }
      vals_to_print(\@ans);

      # Compare the results

      foreach my $arg (@arg) {
         $arg = "_undef_"  if (! defined $arg);
         $arg = "_blank_"  if ($arg eq "");
      }
      $arg = join("\n           ",@argprt,@extra);
      $ans = join("\n           ",@ans);
      $exp = join("\n           ",@exp);

      if (! $runtest) {
         print "########################\n";
         print "Test     = $t\n";
         print "Args     = $arg\n";
         print "Expected = $exp\n";
      } elsif ($ans ne $exp) {
         print "not ok $t\n";
         warn "########################\n";
         warn "Args     = $arg\n";
         warn "Expected = $exp\n";
         warn "Got      = $ans\n";
         warn "########################\n";
      } else {
         print "ok $t\n"  if ($feedback);
      }
   }
}

# The following is similar but it takes input from an input file and
# sends output to an output file.
#
# $files is a reference to a list of tests.  If one of the tests is named
# "foobar", the input is from "foobar.in", output is to "foobar.out", and
# the expected output is in "foobar.exp".
#
# The function stored in $funcref is called as:
#    &$funcref($in,$out,@extra)
# where $in is the name of the input file, $out is the name of the output
# file, and @extra are any additional arguments that are required.
#
# The function should return 0 on success, or an error message.

sub test_File {
   my($funcref,$files,$runtests,@extra)=@_;
   my(@files)=@$files;

   $runtests=0  if (! $runtests);

   my($ntest)=$#files + 1;
   print "1..$ntest\n"  if (! $runtests);

   my(@t);
   if ($runtests > 0) {
      @t = ($runtests..$ntest);
   } elsif ($runtests < 0) {
      @t = (-$runtests);
   } else {
      @t = (1..$ntest);
   }

   foreach my $t (@t) {
      $::testnum = $t;
      my $test = $files[$t-1];
      my $expf = "$test.exp";
      my $outf = "$test.out";

      if (! -f $test  ||  ! -f $expf) {
         print "not ok $t\n";
         warn  "Test: $test: missing input/outpuf information\n";
         next;
      }

      my $err  = &$funcref($test,$outf,@extra);
      if ($err) {
         print "not ok $t\n";
         warn  "Test: $test: $err\n";
         next;
      }

      local *FH;
      open(FH,$expf)  ||  do {
         print "not ok $t\n";
         warn  "Test: $test: $!\n";
         next;
      };
      my @exp = <FH>;
      close(FH);
      my $exp = join("",@exp);
      open(FH,$outf)  ||  do {
         print "not ok $t\n";
         warn  "Test: $test: $!\n";
         next;
      };
      my @out = <FH>;
      close(FH);
      my $out = join("",@out);

      if ($out ne $exp) {
         print "not ok $t\n";
         warn  "Test: $test: output differs from expected value\n";
         next;
      }

      print "ok $t\n"  if (! $runtests);
   }
}

# Converts a printable version of arguments to actual arguments
sub print_to_vals {
   my($listref) = @_;

   foreach my $arg (@$listref) {
      next  if (! defined($arg));
      if ($arg eq "_undef_") {
         $arg = undef;

      } elsif ($arg eq "_blank_") {
         $arg = "";

      } elsif ($arg =~ /^\\\s*(.*)/) {
         $str = $1;
         $arg = \$str;

      } elsif ($arg =~ /^\[(.?)\]\s*(.*)/) {
         my($sep,$str) = ($1,$2);
         $sep = ","  if (! $sep);
         my @list = split(/\Q$sep\E/,$str);
         foreach my $e (@list) {
            $e = ""     if ($e eq "_blank_");
            $e = undef  if ($e eq "_undef_");
         }
         $arg = \@list;

      } elsif ($arg =~ /^\{(.?)\}\s*(.*)/) {
         my($sep,$str) = ($1,$2);
         $sep = ","  if (! $sep);
         my %hash = split(/\Q$sep\E/,$str);
         foreach my $key (keys %hash) {
            my $val = $hash{$key};
            $hash{$key} = undef  if ($val eq "_undef_");
            $hash{$key} = ""     if ($val eq "_blank_");
         }
         $arg = \%hash;
      }
   }
}

# Converts arguments to a printable version.
sub vals_to_print {
   my($listref) = @_;

   foreach my $arg (@$listref) {
      if (! defined $arg) {
         $arg = "_undef_";

      } elsif (! ref($arg)) {
         $arg = "_blank_"  if ($arg eq "");

      } else {
         my $ref = ref($arg);
         if      ($ref eq "SCALAR") {
            $arg = "\\ $$arg";

         } elsif ($ref eq "ARRAY") {
            my @list = @$arg;
            foreach my $e (@list) {
               $e = "_undef_", next   if (! defined($e));
               $e = "_blank_"         if ($e eq "");
            }
            $arg = join(" ","[",join(", ",@list),"]");

         } elsif ($ref eq "HASH") {
            %hash = %$arg;
            foreach my $key (keys %hash) {
               my $val = $hash{$key};
               $hash{$key} = "_undef_", next  if (! defined($val));
               $hash{$key} = "_blank_"        if ($val eq "_blank_");
            }
            $arg = join(" ","{",
                        join(", ",map { "$_ => $hash{$_}" }
                             (sort keys %hash)), "}");
            $arg =~ s/  +/ /g;
         }
      }
   }
}

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

