package Test::Harness;

BEGIN {require 5.002;}
use Exporter;
use Benchmark;
use Config;
use FileHandle;
use strict;

use vars qw($VERSION $verbose $switches $have_devel_corestack $curtest
	    @ISA @EXPORT @EXPORT_OK);
$have_devel_corestack = 0;

$VERSION = "1.1502";

@ISA=('Exporter');
@EXPORT= qw(&runtests);
@EXPORT_OK= qw($verbose $switches);

format STDOUT_TOP =
Failed Test  Status Wstat Total Fail  Failed  List of failed
-------------------------------------------------------------------------------
.

format STDOUT =
@<<<<<<<<<<<<<< @>> @>>>> @>>>> @>>> ^##.##%  ^<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
{ $curtest->{name},
                $curtest->{estat},
                    $curtest->{wstat},
                          $curtest->{max},
                                $curtest->{failed},
                                     $curtest->{percent},
                                              $curtest->{canon}
}
~~                                            ^<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
                                              $curtest->{canon}
.


$verbose = 0;
$switches = "-w";

sub runtests {
    my(@tests) = @_;
    local($|) = 1;
    my($test,$te,$ok,$next,$max,$pct,$totok,@failed,%failedtests);
    my $totmax = 0;
    my $files = 0;
    my $bad = 0;
    my $good = 0;
    my $total = @tests;

    # pass -I flags to children
    my $old5lib = $ENV{PERL5LIB};
    local($ENV{'PERL5LIB'}) = join($Config{path_sep}, @INC);

    if ($^O eq 'VMS') { $switches =~ s/-(\S*[A-Z]\S*)/"-$1"/g }

    my $t_start = new Benchmark;
    while ($test = shift(@tests)) {
	$te = $test;
	chop($te);
	if ($^O eq 'VMS') { $te =~ s/^.*\.t\./[.t./; }
	print "$te" . '.' x (20 - length($te));
	my $fh = new FileHandle;
	$fh->open($test) or print "can't open $test. $!\n";
	my $first = <$fh>;
	my $s = $switches;
	$s .= q[ "-T"] if $first =~ /^#!.*\bperl.*-\w*T/;
	$fh->close or print "can't close $test. $!\n";
	my $cmd = "$^X $s $test|";
	$cmd = "MCR $cmd" if $^O eq 'VMS';
	$fh->open($cmd) or print "can't run $test. $!\n";
	$ok = $next = $max = 0;
	@failed = ();
	while (<$fh>) {
	    if( $verbose ){
		print $_;
	    }
	    if (/^1\.\.([0-9]+)/) {
		$max = $1;
		$totmax += $max;
		$files++;
		$next = 1;
	    } elsif ($max && /^(not\s+)?ok\b/) {
		my $this = $next;
		if (/^not ok\s*(\d*)/){
		    $this = $1 if $1 > 0;
		    push @failed, $this;
		} elsif (/^ok\s*(\d*)/) {
		    $this = $1 if $1 > 0;
		    $ok++;
		    $totok++;
		}
		if ($this > $next) {
		    # warn "Test output counter mismatch [test $this]\n";
		    # no need to warn probably
		    push @failed, $next..$this-1;
		} elsif ($this < $next) {
		    #we have seen more "ok" lines than the number suggests
		    warn "Confused test output: test $this answered after test ", $next-1, "\n";
		    $next = $this;
		}
		$next = $this + 1;
	    }
	}
	$fh->close; # must close to reap child resource values
	my $wstatus = $?;
	my $estatus;
	$estatus = ($^O eq 'VMS'
		       ? eval 'use vmsish "status"; $estatus = $?'
		       : $wstatus >> 8);
	if ($wstatus) {
	    my ($failed, $canon, $percent) = ('??', '??');
	    printf "dubious\n\tTest returned status $estatus (wstat %d, 0x%x)\n",
		    $wstatus,$wstatus;
	    print "\t\t(VMS status is $estatus)\n" if $^O eq 'VMS';
	    if (corestatus($wstatus)) { # until we have a wait module
		if ($have_devel_corestack) {
		    Devel::CoreStack::stack($^X);
		} else {
		    print "\ttest program seems to have generated a core\n";
		}
	    }
	    $bad++;
	    if ($max) {
	      if ($next == $max + 1 and not @failed) {
		print "\tafter all the subtests completed successfully\n";
		$percent = 0;
		$failed = 0;	# But we do not set $canon!
	      } else {
		push @failed, $next..$max;
		$failed = @failed;
		(my $txt, $canon) = canonfailed($max,@failed);
		$percent = 100*(scalar @failed)/$max;
		print "DIED. ",$txt;
	      }
	    }
	    $failedtests{$test} = { canon => $canon,  max => $max || '??',
				    failed => $failed, 
				    name => $test, percent => $percent,
				    estat => $estatus, wstat => $wstatus,
				  };
	} elsif ($ok == $max && $next == $max+1) {
	    if ($max) {
		print "ok\n";
	    } else {
		print "skipping test on this platform\n";
	    }
	    $good++;
	} elsif ($max) {
	    if ($next <= $max) {
		push @failed, $next..$max;
	    }
	    if (@failed) {
		my ($txt, $canon) = canonfailed($max,@failed);
		print $txt;
		$failedtests{$test} = { canon => $canon,  max => $max,
					failed => scalar @failed,
					name => $test, percent => 100*(scalar @failed)/$max,
					estat => '', wstat => '',
				      };
	    } else {
		print "Don't know which tests failed: got $ok ok, expected $max\n";
		$failedtests{$test} = { canon => '??',  max => $max,
					failed => '??', 
					name => $test, percent => undef,
					estat => '', wstat => '',
				      };
	    }
	    $bad++;
	} elsif ($next == 0) {
	    print "FAILED before any test output arrived\n";
	    $bad++;
	    $failedtests{$test} = { canon => '??',  max => '??',
				    failed => '??',
				    name => $test, percent => undef,
				    estat => '', wstat => '',
				  };
	}
    }
    my $t_total = timediff(new Benchmark, $t_start);
    
    if ($^O eq 'VMS') {
	if (defined $old5lib) {
	    $ENV{PERL5LIB} = $old5lib;
	} else {
	    delete $ENV{PERL5LIB};
	}
    }
    if ($bad == 0 && $totmax) {
	    print "All tests successful.\n";
    } elsif ($total==0){
	die "FAILED--no tests were run for some reason.\n";
    } elsif ($totmax==0) {
	my $blurb = $total==1 ? "script" : "scripts";
	die "FAILED--$total test $blurb could be run, alas--no output ever seen\n";
    } else {
	$pct = sprintf("%.2f", $good / $total * 100);
	my $subpct = sprintf " %d/%d subtests failed, %.2f%% okay.",
	$totmax - $totok, $totmax, 100*$totok/$totmax;
	my $script;
	for $script (sort keys %failedtests) {
	  $curtest = $failedtests{$script};
	  write;
	}
	if ($bad) {
	    die "Failed $bad/$total test scripts, $pct% okay.$subpct\n";
	}
    }
    printf("Files=%d,  Tests=%d, %s\n", $files, $totmax, timestr($t_total, 'nop'));

    return ($bad == 0 && $totmax) ;
}

my $tried_devel_corestack;
sub corestatus {
    my($st) = @_;
    my($ret);

    eval {require 'wait.ph'};
    if ($@) {
      SWITCH: {
	    $ret = ($st & 0200); # Tim says, this is for 90%
	}
    } else {
	$ret = WCOREDUMP($st);
    }

    eval { require Devel::CoreStack; $have_devel_corestack++ } 
      unless $tried_devel_corestack++;

    $ret;
}

sub canonfailed ($@) {
    my($max,@failed) = @_;
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
	$canon = "@canon";
    } else {
	push @result, "FAILED test $last\n";
	$canon = $last;
    }

    push @result, "\tFailed $failed/$max tests, ";
    push @result, sprintf("%.2f",100*(1-$failed/$max)), "% okay\n";
    my $txt = join "", @result;
    ($txt, $canon);
}

1;
__END__

=head1 NAME

Test::Harness - run perl standard test scripts with statistics

=head1 SYNOPSIS

use Test::Harness;

runtests(@tests);

=head1 DESCRIPTION

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

Any output from the testscript to standard error is ignored and
bypassed, thus will be seen by the user. Lines written to standard
output containing C</^(not\s+)?ok\b/> are interpreted as feedback for
runtests().  All other lines are discarded.

It is tolerated if the test numbers after C<ok> are omitted. In this
case Test::Harness maintains temporarily its own counter until the
script supplies test numbers again. So the following test script

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

The global variable $Test::Harness::verbose is exportable and can be
used to let runtests() display the standard output of the script
without altering the behavior otherwise.

The global variable $Test::Harness::switches is exportable and can be
used to set perl command line options used for running the test
script(s). The default value is C<-w>.

=head1 EXPORT

C<&runtests> is exported by Test::Harness per default.

=head1 DIAGNOSTICS

=over 4

=item C<All tests successful.\nFiles=%d,  Tests=%d, %s>

If all tests are successful some statistics about the performance are
printed.

=item C<FAILED tests %s\n\tFailed %d/%d tests, %.2f%% okay.>

For any single script that has failing subtests statistics like the
above are printed.

=item C<Test returned status %d (wstat %d)>

Scripts that return a non-zero exit status, both C<$? E<gt>E<gt> 8> and C<$?> are
printed in a message similar to the above.

=item C<Failed 1 test, %.2f%% okay. %s>

=item C<Failed %d/%d tests, %.2f%% okay. %s>

If not all tests were successful, the script dies with one of the
above messages.

=back

=head1 SEE ALSO

See L<Benchmark> for the underlying timing routines.

=head1 AUTHORS

Either Tim Bunce or Andreas Koenig, we don't know. What we know for
sure is, that it was inspired by Larry Wall's TEST script that came
with perl distributions for ages. Numerous anonymous contributors
exist. Current maintainer is Andreas Koenig.

=head1 BUGS

Test::Harness uses $^X to determine the perl binary to run the tests
with. Test scripts running via the shebang (C<#!>) line may not be
portable because $^X is not consistent for shebang scripts across
platforms. This is no problem when Test::Harness is run with an
absolute path to the perl binary or when $^X can be found in the path.

=cut
