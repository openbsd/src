# -*- Mode: cperl; cperl-indent-level: 4 -*-
# $Id: Harness.pm,v 1.6 2002/10/27 22:25:27 millert Exp $

package Test::Harness;

require 5.004;
use Test::Harness::Straps;
use Test::Harness::Assert;
use Exporter;
use Benchmark;
use Config;
use strict;

use vars qw($VERSION $Verbose $Switches $Have_Devel_Corestack $Curtest
            $Columns $verbose $switches $ML $Strap
            @ISA @EXPORT @EXPORT_OK
           );

# Backwards compatibility for exportable variable names.
*verbose  = \$Verbose;
*switches = \$Switches;

$Have_Devel_Corestack = 0;

$VERSION = '2.26';

$ENV{HARNESS_ACTIVE} = 1;

END {
    # For VMS.
    delete $ENV{HARNESS_ACTIVE};
}

# Some experimental versions of OS/2 build have broken $?
my $Ignore_Exitcode = $ENV{HARNESS_IGNORE_EXITCODE};

my $Files_In_Dir = $ENV{HARNESS_FILELEAK_IN_DIR};

$Strap = Test::Harness::Straps->new;

@ISA = ('Exporter');
@EXPORT    = qw(&runtests);
@EXPORT_OK = qw($verbose $switches);

$Verbose  = $ENV{HARNESS_VERBOSE} || 0;
$Switches = "-w";
$Columns  = $ENV{HARNESS_COLUMNS} || $ENV{COLUMNS} || 80;
$Columns--;             # Some shells have trouble with a full line of text.


=head1 NAME

Test::Harness - run perl standard test scripts with statistics

=head1 SYNOPSIS

  use Test::Harness;

  runtests(@test_files);

=head1 DESCRIPTION

B<STOP!> If all you want to do is write a test script, consider using
Test::Simple.  Otherwise, read on.

(By using the Test module, you can write test scripts without
knowing the exact output this module expects.  However, if you need to
know the specifics, read on!)

Perl test scripts print to standard output C<"ok N"> for each single
test, where C<N> is an increasing sequence of integers. The first line
output by a standard test script is C<"1..M"> with C<M> being the
number of tests that should be run within the test
script. Test::Harness::runtests(@tests) runs all the testscripts
named as arguments and checks standard output for the expected
C<"ok N"> strings.

After all tests have been performed, runtests() prints some
performance statistics that are computed by the Benchmark module.

=head2 The test script output

The following explains how Test::Harness interprets the output of your
test program.

=over 4

=item B<'1..M'>

This header tells how many tests there will be.  For example, C<1..10>
means you plan on running 10 tests.  This is a safeguard in case your
test dies quietly in the middle of its run.

It should be the first non-comment line output by your test program.

In certain instances, you may not know how many tests you will
ultimately be running.  In this case, it is permitted for the 1..M
header to appear as the B<last> line output by your test (again, it
can be followed by further comments).

Under B<no> circumstances should 1..M appear in the middle of your
output or more than once.


=item B<'ok', 'not ok'.  Ok?>

Any output from the testscript to standard error is ignored and
bypassed, thus will be seen by the user. Lines written to standard
output containing C</^(not\s+)?ok\b/> are interpreted as feedback for
runtests().  All other lines are discarded.

C</^not ok/> indicates a failed test.  C</^ok/> is a successful test.


=item B<test numbers>

Perl normally expects the 'ok' or 'not ok' to be followed by a test
number.  It is tolerated if the test numbers after 'ok' are
omitted. In this case Test::Harness maintains temporarily its own
counter until the script supplies test numbers again. So the following
test script

    print <<END;
    1..6
    not ok
    ok
    not ok
    ok
    ok
    END

will generate

    FAILED tests 1, 3, 6
    Failed 3/6 tests, 50.00% okay

=item B<test names>

Anything after the test number but before the # is considered to be
the name of the test.

  ok 42 this is the name of the test

Currently, Test::Harness does nothing with this information.

=item B<Skipping tests>

If the standard output line contains the substring C< # Skip> (with
variations in spacing and case) after C<ok> or C<ok NUMBER>, it is
counted as a skipped test.  If the whole testscript succeeds, the
count of skipped tests is included in the generated output.
C<Test::Harness> reports the text after C< # Skip\S*\s+> as a reason
for skipping.

  ok 23 # skip Insufficient flogiston pressure.

Similarly, one can include a similar explanation in a C<1..0> line
emitted if the test script is skipped completely:

  1..0 # Skipped: no leverage found

=item B<Todo tests>

If the standard output line contains the substring C< # TODO> after
C<not ok> or C<not ok NUMBER>, it is counted as a todo test.  The text
afterwards is the thing that has to be done before this test will
succeed.

  not ok 13 # TODO harness the power of the atom

=begin _deprecated

Alternatively, you can specify a list of what tests are todo as part
of the test header.

  1..23 todo 5 12 23

This only works if the header appears at the beginning of the test.

This style is B<deprecated>.

=end _deprecated

These tests represent a feature to be implemented or a bug to be fixed
and act as something of an executable "thing to do" list.  They are
B<not> expected to succeed.  Should a todo test begin succeeding,
Test::Harness will report it as a bonus.  This indicates that whatever
you were supposed to do has been done and you should promote this to a
normal test.

=item B<Bail out!>

As an emergency measure, a test script can decide that further tests
are useless (e.g. missing dependencies) and testing should stop
immediately. In that case the test script prints the magic words

  Bail out!

to standard output. Any message after these words will be displayed by
C<Test::Harness> as the reason why testing is stopped.

=item B<Comments>

Additional comments may be put into the testing output on their own
lines.  Comment lines should begin with a '#', Test::Harness will
ignore them.

  ok 1
  # Life is good, the sun is shining, RAM is cheap.
  not ok 2
  # got 'Bush' expected 'Gore'

=item B<Anything else>

Any other output Test::Harness sees it will silently ignore B<BUT WE
PLAN TO CHANGE THIS!> If you wish to place additional output in your
test script, please use a comment.

=back


=head2 Taint mode

Test::Harness will honor the C<-T> in the #! line on your test files.  So
if you begin a test with:

    #!perl -T

the test will be run with taint mode on.


=head2 Configuration variables.

These variables can be used to configure the behavior of
Test::Harness.  They are exported on request.

=over 4

=item B<$Test::Harness::verbose>

The global variable $Test::Harness::verbose is exportable and can be
used to let runtests() display the standard output of the script
without altering the behavior otherwise.

=item B<$Test::Harness::switches>

The global variable $Test::Harness::switches is exportable and can be
used to set perl command line options used for running the test
script(s). The default value is C<-w>.

=back


=head2 Failure

It will happen, your tests will fail.  After you mop up your ego, you
can begin examining the summary report:

  t/base..............ok
  t/nonumbers.........ok
  t/ok................ok
  t/test-harness......ok
  t/waterloo..........dubious
          Test returned status 3 (wstat 768, 0x300)
  DIED. FAILED tests 1, 3, 5, 7, 9, 11, 13, 15, 17, 19
          Failed 10/20 tests, 50.00% okay
  Failed Test  Stat Wstat Total Fail  Failed  List of Failed
  -----------------------------------------------------------------------
  t/waterloo.t    3   768    20   10  50.00%  1 3 5 7 9 11 13 15 17 19
  Failed 1/5 test scripts, 80.00% okay. 10/44 subtests failed, 77.27% okay.

Everything passed but t/waterloo.t.  It failed 10 of 20 tests and
exited with non-zero status indicating something dubious happened.

The columns in the summary report mean:

=over 4

=item B<Failed Test>

The test file which failed.

=item B<Stat>

If the test exited with non-zero, this is its exit status.

=item B<Wstat>

The wait status of the test I<umm, I need a better explanation here>.

=item B<Total>

Total number of tests expected to run.

=item B<Fail>

Number which failed, either from "not ok" or because they never ran.

=item B<Failed>

Percentage of the total tests which failed.

=item B<List of Failed>

A list of the tests which failed.  Successive failures may be
abbreviated (ie. 15-20 to indicate that tests 15, 16, 17, 18, 19 and
20 failed).

=back


=head2 Functions

Test::Harness currently only has one function, here it is.

=over 4

=item B<runtests>

  my $allok = runtests(@test_files);

This runs all the given @test_files and divines whether they passed
or failed based on their output to STDOUT (details above).  It prints
out each individual test which failed along with a summary report and
a how long it all took.

It returns true if everything was ok.  Otherwise it will die() with
one of the messages in the DIAGNOSTICS section.

=for _private
This is just _run_all_tests() plus _show_results()

=cut

sub runtests {
    my(@tests) = @_;

    local ($\, $,);

    my($tot, $failedtests) = _run_all_tests(@tests);
    _show_results($tot, $failedtests);

    my $ok = _all_ok($tot);

    assert(($ok xor keys %$failedtests), 
           q{ok status jives with $failedtests});

    return $ok;
}

=begin _private

=item B<_all_ok>

  my $ok = _all_ok(\%tot);

Tells you if this test run is overall successful or not.

=cut

sub _all_ok {
    my($tot) = shift;

    return $tot->{bad} == 0 && ($tot->{max} || $tot->{skipped}) ? 1 : 0;
}

=item B<_globdir>

  my @files = _globdir $dir;

Returns all the files in a directory.  This is shorthand for backwards
compatibility on systems where glob() doesn't work right.

=cut

sub _globdir { 
    opendir DIRH, shift; 
    my @f = readdir DIRH; 
    closedir DIRH; 

    return @f;
}

=item B<_run_all_tests>

  my($total, $failed) = _run_all_tests(@test_files);

Runs all the given @test_files (as runtests()) but does it quietly (no
report).  $total is a hash ref summary of all the tests run.  Its keys
and values are this:

    bonus           Number of individual todo tests unexpectedly passed
    max             Number of individual tests ran
    ok              Number of individual tests passed
    sub_skipped     Number of individual tests skipped
    todo            Number of individual todo tests

    files           Number of test files ran
    good            Number of test files passed
    bad             Number of test files failed
    tests           Number of test files originally given
    skipped         Number of test files skipped

If $total->{bad} == 0 and $total->{max} > 0, you've got a successful
test.

$failed is a hash ref of all the test scripts which failed.  Each key
is the name of a test script, each value is another hash representing
how that script failed.  Its keys are these:

    name        Name of the test which failed
    estat       Script's exit value
    wstat       Script's wait status
    max         Number of individual tests
    failed      Number which failed
    percent     Percentage of tests which failed
    canon       List of tests which failed (as string).

Needless to say, $failed should be empty if everything passed.

B<NOTE> Currently this function is still noisy.  I'm working on it.

=cut

#'#
sub _run_all_tests {
    my(@tests) = @_;
    local($|) = 1;
    my(%failedtests);

    # Test-wide totals.
    my(%tot) = (
                bonus    => 0,
                max      => 0,
                ok       => 0,
                files    => 0,
                bad      => 0,
                good     => 0,
                tests    => scalar @tests,
                sub_skipped  => 0,
                todo     => 0,
                skipped  => 0,
                bench    => 0,
               );

    my @dir_files = _globdir $Files_In_Dir if defined $Files_In_Dir;
    my $t_start = new Benchmark;

    my $width = _leader_width(@tests);
    foreach my $tfile (@tests) {

        my($leader, $ml) = _mk_leader($tfile, $width);
        local $ML = $ml;
        print $leader;

        $tot{files}++;

        $Strap->{_seen_header} = 0;
        my %results = $Strap->analyze_file($tfile) or
          do { warn "$Strap->{error}\n";  next };

        # state of the current test.
        my @failed = grep { !$results{details}[$_-1]{ok} }
                     1..@{$results{details}};
        my %test = (
                    ok          => $results{ok},
                    'next'      => $Strap->{'next'},
                    max         => $results{max},
                    failed      => \@failed,
                    bonus       => $results{bonus},
                    skipped     => $results{skip},
                    skip_reason => $Strap->{_skip_reason},
                    skip_all    => $Strap->{skip_all},
                    ml          => $ml,
                   );

        $tot{bonus}       += $results{bonus};
        $tot{max}         += $results{max};
        $tot{ok}          += $results{ok};
        $tot{todo}        += $results{todo};
        $tot{sub_skipped} += $results{skip};

        my($estatus, $wstatus) = @results{qw(exit wait)};

        if ($wstatus) {
            $failedtests{$tfile} = _dubious_return(\%test, \%tot, 
                                                  $estatus, $wstatus);
            $failedtests{$tfile}{name} = $tfile;
        }
        elsif ($results{passing}) {
            if ($test{max} and $test{skipped} + $test{bonus}) {
                my @msg;
                push(@msg, "$test{skipped}/$test{max} skipped: $test{skip_reason}")
                    if $test{skipped};
                push(@msg, "$test{bonus}/$test{max} unexpectedly succeeded")
                    if $test{bonus};
                print "$test{ml}ok\n        ".join(', ', @msg)."\n";
            } elsif ($test{max}) {
                print "$test{ml}ok\n";
            } elsif (defined $test{skip_all} and length $test{skip_all}) {
                print "skipped\n        all skipped: $test{skip_all}\n";
                $tot{skipped}++;
            } else {
                print "skipped\n        all skipped: no reason given\n";
                $tot{skipped}++;
            }
            $tot{good}++;
        }
        else {
            if ($test{max}) {
                if ($test{'next'} <= $test{max}) {
                    push @{$test{failed}}, $test{'next'}..$test{max};
                }
                if (@{$test{failed}}) {
                    my ($txt, $canon) = canonfailed($test{max},$test{skipped},
                                                    @{$test{failed}});
                    print "$test{ml}$txt";
                    $failedtests{$tfile} = { canon   => $canon,
                                             max     => $test{max},
                                             failed  => scalar @{$test{failed}},
                                             name    => $tfile, 
                                             percent => 100*(scalar @{$test{failed}})/$test{max},
                                             estat   => '',
                                             wstat   => '',
                                           };
                } else {
                    print "Don't know which tests failed: got $test{ok} ok, ".
                          "expected $test{max}\n";
                    $failedtests{$tfile} = { canon   => '??',
                                             max     => $test{max},
                                             failed  => '??',
                                             name    => $tfile, 
                                             percent => undef,
                                             estat   => '', 
                                             wstat   => '',
                                           };
                }
                $tot{bad}++;
            } elsif ($test{'next'} == 0) {
                print "FAILED before any test output arrived\n";
                $tot{bad}++;
                $failedtests{$tfile} = { canon       => '??',
                                         max         => '??',
                                         failed      => '??',
                                         name        => $tfile,
                                         percent     => undef,
                                         estat       => '', 
                                         wstat       => '',
                                       };
            }
        }

        if (defined $Files_In_Dir) {
            my @new_dir_files = _globdir $Files_In_Dir;
            if (@new_dir_files != @dir_files) {
                my %f;
                @f{@new_dir_files} = (1) x @new_dir_files;
                delete @f{@dir_files};
                my @f = sort keys %f;
                print "LEAKED FILES: @f\n";
                @dir_files = @new_dir_files;
            }
        }
    }
    $tot{bench} = timediff(new Benchmark, $t_start);

    $Strap->_restore_PERL5LIB;

    return(\%tot, \%failedtests);
}

=item B<_mk_leader>

  my($leader, $ml) = _mk_leader($test_file, $width);

Generates the 't/foo........' $leader for the given $test_file as well
as a similar version which will overwrite the current line (by use of
\r and such).  $ml may be empty if Test::Harness doesn't think you're
on TTY.

The $width is the width of the "yada/blah.." string.

=cut

sub _mk_leader {
    my($te, $width) = @_;
    chomp($te);
    $te =~ s/\.\w+$/./;

    if ($^O eq 'VMS') { $te =~ s/^.*\.t\./\[.t./s; }
    my $blank = (' ' x 77);
    my $leader = "$te" . '.' x ($width - length($te));
    my $ml = "";

    $ml = "\r$blank\r$leader"
      if -t STDOUT and not $ENV{HARNESS_NOTTY} and not $Verbose;

    return($leader, $ml);
}

=item B<_leader_width>

  my($width) = _leader_width(@test_files);

Calculates how wide the leader should be based on the length of the
longest test name.

=cut

sub _leader_width {
    my $maxlen = 0;
    my $maxsuflen = 0;
    foreach (@_) {
        my $suf    = /\.(\w+)$/ ? $1 : '';
        my $len    = length;
        my $suflen = length $suf;
        $maxlen    = $len    if $len    > $maxlen;
        $maxsuflen = $suflen if $suflen > $maxsuflen;
    }
    # + 3 : we want three dots between the test name and the "ok"
    return $maxlen + 3 - $maxsuflen;
}


sub _show_results {
    my($tot, $failedtests) = @_;

    my $pct;
    my $bonusmsg = _bonusmsg($tot);

    if (_all_ok($tot)) {
        print "All tests successful$bonusmsg.\n";
    } elsif (!$tot->{tests}){
        die "FAILED--no tests were run for some reason.\n";
    } elsif (!$tot->{max}) {
        my $blurb = $tot->{tests}==1 ? "script" : "scripts";
        die "FAILED--$tot->{tests} test $blurb could be run, ".
            "alas--no output ever seen\n";
    } else {
        $pct = sprintf("%.2f", $tot->{good} / $tot->{tests} * 100);
        my $percent_ok = 100*$tot->{ok}/$tot->{max};
        my $subpct = sprintf " %d/%d subtests failed, %.2f%% okay.",
                              $tot->{max} - $tot->{ok}, $tot->{max}, 
                              $percent_ok;

        my($fmt_top, $fmt) = _create_fmts($failedtests);

        # Now write to formats
        for my $script (sort keys %$failedtests) {
          $Curtest = $failedtests->{$script};
          write;
        }
        if ($tot->{bad}) {
            $bonusmsg =~ s/^,\s*//;
            print "$bonusmsg.\n" if $bonusmsg;
            die "Failed $tot->{bad}/$tot->{tests} test scripts, $pct% okay.".
                "$subpct\n";
        }
    }

    printf("Files=%d, Tests=%d, %s\n",
           $tot->{files}, $tot->{max}, timestr($tot->{bench}, 'nop'));
}


my %Handlers = ();
$Strap->{callback} = sub {
    my($self, $line, $type, $totals) = @_;
    print $line if $Verbose;

    my $meth = $Handlers{$type};
    $meth->($self, $line, $type, $totals) if $meth;
};


$Handlers{header} = sub {
    my($self, $line, $type, $totals) = @_;

    warn "Test header seen more than once!\n" if $self->{_seen_header};

    $self->{_seen_header}++;

    warn "1..M can only appear at the beginning or end of tests\n"
      if $totals->{seen} && 
         $totals->{max}  < $totals->{seen};
};

$Handlers{test} = sub {
    my($self, $line, $type, $totals) = @_;

    my $curr = $totals->{seen};
    my $next = $self->{'next'};
    my $max  = $totals->{max};
    my $detail = $totals->{details}[-1];

    if( $detail->{ok} ) {
        _print_ml("ok $curr/$max");

        if( $detail->{type} eq 'skip' ) {
            $self->{_skip_reason} = $detail->{reason}
              unless defined $self->{_skip_reason};
            $self->{_skip_reason} = 'various reasons'
              if $self->{_skip_reason} ne $detail->{reason};
        }
    }
    else {
        _print_ml("NOK $curr");
    }

    if( $curr > $next ) {
        print "Test output counter mismatch [test $curr]\n";
    }
    elsif( $curr < $next ) {
        print "Confused test output: test $curr answered after ".
              "test ", $next - 1, "\n";
    }

};

$Handlers{bailout} = sub {
    my($self, $line, $type, $totals) = @_;

    die "FAILED--Further testing stopped" .
      ($self->{bailout_reason} ? ": $self->{bailout_reason}\n" : ".\n");
};


sub _print_ml {
    print join '', $ML, @_ if $ML;
}


sub _bonusmsg {
    my($tot) = @_;

    my $bonusmsg = '';
    $bonusmsg = (" ($tot->{bonus} subtest".($tot->{bonus} > 1 ? 's' : '').
               " UNEXPECTEDLY SUCCEEDED)")
        if $tot->{bonus};

    if ($tot->{skipped}) {
        $bonusmsg .= ", $tot->{skipped} test"
                     . ($tot->{skipped} != 1 ? 's' : '');
        if ($tot->{sub_skipped}) {
            $bonusmsg .= " and $tot->{sub_skipped} subtest"
                         . ($tot->{sub_skipped} != 1 ? 's' : '');
        }
        $bonusmsg .= ' skipped';
    }
    elsif ($tot->{sub_skipped}) {
        $bonusmsg .= ", $tot->{sub_skipped} subtest"
                     . ($tot->{sub_skipped} != 1 ? 's' : '')
                     . " skipped";
    }

    return $bonusmsg;
}

# Test program go boom.
sub _dubious_return {
    my($test, $tot, $estatus, $wstatus) = @_;
    my ($failed, $canon, $percent) = ('??', '??');

    printf "$test->{ml}dubious\n\tTest returned status $estatus ".
           "(wstat %d, 0x%x)\n",
           $wstatus,$wstatus;
    print "\t\t(VMS status is $estatus)\n" if $^O eq 'VMS';

    if (corestatus($wstatus)) { # until we have a wait module
        if ($Have_Devel_Corestack) {
            Devel::CoreStack::stack($^X);
        } else {
            print "\ttest program seems to have generated a core\n";
        }
    }

    $tot->{bad}++;

    if ($test->{max}) {
        if ($test->{'next'} == $test->{max} + 1 and not @{$test->{failed}}) {
            print "\tafter all the subtests completed successfully\n";
            $percent = 0;
            $failed = 0;        # But we do not set $canon!
        }
        else {
            push @{$test->{failed}}, $test->{'next'}..$test->{max};
            $failed = @{$test->{failed}};
            (my $txt, $canon) = canonfailed($test->{max},$test->{skipped},@{$test->{failed}});
            $percent = 100*(scalar @{$test->{failed}})/$test->{max};
            print "DIED. ",$txt;
        }
    }

    return { canon => $canon,  max => $test->{max} || '??',
             failed => $failed, 
             percent => $percent,
             estat => $estatus, wstat => $wstatus,
           };
}


sub _create_fmts {
    my($failedtests) = @_;

    my $failed_str = "Failed Test";
    my $middle_str = " Stat Wstat Total Fail  Failed  ";
    my $list_str = "List of Failed";

    # Figure out our longest name string for formatting purposes.
    my $max_namelen = length($failed_str);
    foreach my $script (keys %$failedtests) {
        my $namelen = length $failedtests->{$script}->{name};
        $max_namelen = $namelen if $namelen > $max_namelen;
    }

    my $list_len = $Columns - length($middle_str) - $max_namelen;
    if ($list_len < length($list_str)) {
        $list_len = length($list_str);
        $max_namelen = $Columns - length($middle_str) - $list_len;
        if ($max_namelen < length($failed_str)) {
            $max_namelen = length($failed_str);
            $Columns = $max_namelen + length($middle_str) + $list_len;
        }
    }

    my $fmt_top = "format STDOUT_TOP =\n"
                  . sprintf("%-${max_namelen}s", $failed_str)
                  . $middle_str
                  . $list_str . "\n"
                  . "-" x $Columns
                  . "\n.\n";

    my $fmt = "format STDOUT =\n"
              . "@" . "<" x ($max_namelen - 1)
              . "  @>> @>>>> @>>>> @>>> ^##.##%  "
              . "^" . "<" x ($list_len - 1) . "\n"
              . '{ $Curtest->{name}, $Curtest->{estat},'
              . '  $Curtest->{wstat}, $Curtest->{max},'
              . '  $Curtest->{failed}, $Curtest->{percent},'
              . '  $Curtest->{canon}'
              . "\n}\n"
              . "~~" . " " x ($Columns - $list_len - 2) . "^"
              . "<" x ($list_len - 1) . "\n"
              . '$Curtest->{canon}'
              . "\n.\n";

    eval $fmt_top;
    die $@ if $@;
    eval $fmt;
    die $@ if $@;

    return($fmt_top, $fmt);
}

{
    my $tried_devel_corestack;

    sub corestatus {
        my($st) = @_;

        eval {
            local $^W = 0;  # *.ph files are often *very* noisy
            require 'wait.ph'
        };
        return if $@;
        my $did_core = defined &WCOREDUMP ? WCOREDUMP($st) : $st & 0200;

        eval { require Devel::CoreStack; $Have_Devel_Corestack++ } 
          unless $tried_devel_corestack++;

        return $did_core;
    }
}

sub canonfailed ($@) {
    my($max,$skipped,@failed) = @_;
    my %seen;
    @failed = sort {$a <=> $b} grep !$seen{$_}++, @failed;
    my $failed = @failed;
    my @result = ();
    my @canon = ();
    my $min;
    my $last = $min = shift @failed;
    my $canon;
    if (@failed) {
        for (@failed, $failed[-1]) { # don't forget the last one
            if ($_ > $last+1 || $_ == $last) {
                if ($min == $last) {
                    push @canon, $last;
                } else {
                    push @canon, "$min-$last";
                }
                $min = $_;
            }
            $last = $_;
        }
        local $" = ", ";
        push @result, "FAILED tests @canon\n";
        $canon = join ' ', @canon;
    } else {
        push @result, "FAILED test $last\n";
        $canon = $last;
    }

    push @result, "\tFailed $failed/$max tests, ";
    push @result, sprintf("%.2f",100*(1-$failed/$max)), "% okay";
    my $ender = 's' x ($skipped > 1);
    my $good = $max - $failed - $skipped;
    my $goodper = sprintf("%.2f",100*($good/$max));
    push @result, " (less $skipped skipped test$ender: $good okay, ".
                  "$goodper%)"
         if $skipped;
    push @result, "\n";
    my $txt = join "", @result;
    ($txt, $canon);
}

=end _private

=back

=cut


1;
__END__


=head1 EXPORT

C<&runtests> is exported by Test::Harness per default.

C<$verbose> and C<$switches> are exported upon request.


=head1 DIAGNOSTICS

=over 4

=item C<All tests successful.\nFiles=%d,  Tests=%d, %s>

If all tests are successful some statistics about the performance are
printed.

=item C<FAILED tests %s\n\tFailed %d/%d tests, %.2f%% okay.>

For any single script that has failing subtests statistics like the
above are printed.

=item C<Test returned status %d (wstat %d)>

Scripts that return a non-zero exit status, both C<$? E<gt>E<gt> 8>
and C<$?> are printed in a message similar to the above.

=item C<Failed 1 test, %.2f%% okay. %s>

=item C<Failed %d/%d tests, %.2f%% okay. %s>

If not all tests were successful, the script dies with one of the
above messages.

=item C<FAILED--Further testing stopped: %s>

If a single subtest decides that further testing will not make sense,
the script dies with this message.

=back

=head1 ENVIRONMENT

=over 4

=item C<HARNESS_ACTIVE>

Harness sets this before executing the individual tests.  This allows
the tests to determine if they are being executed through the harness
or by any other means.

=item C<HARNESS_COLUMNS>

This value will be used for the width of the terminal. If it is not
set then it will default to C<COLUMNS>. If this is not set, it will
default to 80. Note that users of Bourne-sh based shells will need to
C<export COLUMNS> for this module to use that variable.

=item C<HARNESS_COMPILE_TEST>

When true it will make harness attempt to compile the test using
C<perlcc> before running it.

B<NOTE> This currently only works when sitting in the perl source
directory!

=item C<HARNESS_FILELEAK_IN_DIR>

When set to the name of a directory, harness will check after each
test whether new files appeared in that directory, and report them as

  LEAKED FILES: scr.tmp 0 my.db

If relative, directory name is with respect to the current directory at
the moment runtests() was called.  Putting absolute path into 
C<HARNESS_FILELEAK_IN_DIR> may give more predictable results.

=item C<HARNESS_IGNORE_EXITCODE>

Makes harness ignore the exit status of child processes when defined.

=item C<HARNESS_NOTTY>

When set to a true value, forces it to behave as though STDOUT were
not a console.  You may need to set this if you don't want harness to
output more frequent progress messages using carriage returns.  Some
consoles may not handle carriage returns properly (which results in a
somewhat messy output).

=item C<HARNESS_PERL_SWITCHES>

Its value will be prepended to the switches used to invoke perl on
each test.  For example, setting C<HARNESS_PERL_SWITCHES> to C<-W> will
run all tests with all warnings enabled.

=item C<HARNESS_VERBOSE>

If true, Test::Harness will output the verbose results of running
its tests.  Setting $Test::Harness::verbose will override this.

=back

=head1 EXAMPLE

Here's how Test::Harness tests itself

  $ cd ~/src/devel/Test-Harness
  $ perl -Mblib -e 'use Test::Harness qw(&runtests $verbose);
    $verbose=0; runtests @ARGV;' t/*.t
  Using /home/schwern/src/devel/Test-Harness/blib
  t/base..............ok
  t/nonumbers.........ok
  t/ok................ok
  t/test-harness......ok
  All tests successful.
  Files=4, Tests=24, 2 wallclock secs ( 0.61 cusr + 0.41 csys = 1.02 CPU)

=head1 SEE ALSO

L<Test> and L<Test::Simple> for writing test scripts, L<Benchmark> for
the underlying timing routines, L<Devel::CoreStack> to generate core
dumps from failed tests and L<Devel::Cover> for test coverage
analysis.

=head1 AUTHORS

Either Tim Bunce or Andreas Koenig, we don't know. What we know for
sure is, that it was inspired by Larry Wall's TEST script that came
with perl distributions for ages. Numerous anonymous contributors
exist.  Andreas Koenig held the torch for many years.

Current maintainer is Michael G Schwern E<lt>schwern@pobox.comE<gt>

=head1 TODO

Provide a way of running tests quietly (ie. no printing) for automated
validation of tests.  This will probably take the form of a version
of runtests() which rather than printing its output returns raw data
on the state of the tests.  (Partially done in Test::Harness::Straps)

Fix HARNESS_COMPILE_TEST without breaking its core usage.

Figure a way to report test names in the failure summary.

Rework the test summary so long test names are not truncated as badly.
(Partially done with new skip test styles)

Deal with VMS's "not \nok 4\n" mistake.

Add option for coverage analysis.

=for _private
Keeping whittling away at _run_all_tests()

=for _private
Clean up how the summary is printed.  Get rid of those damned formats.

=head1 BUGS

HARNESS_COMPILE_TEST currently assumes it's run from the Perl source
directory.

=cut
