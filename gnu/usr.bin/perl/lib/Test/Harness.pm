package Test::Harness;

use Exporter;
use Benchmark;
use Config;
use FileHandle;
use vars qw($VERSION $verbose $switches);
require 5.002;

$VERSION = "1.07";

@ISA=('Exporter');
@EXPORT= qw(&runtests);
@EXPORT_OK= qw($verbose $switches);


$verbose = 0;
$switches = "-w";

sub runtests {
    my(@tests) = @_;
    local($|) = 1;
    my($test,$te,$ok,$next,$max,$pct);
    my $totmax = 0;
    my $files = 0;
    my $bad = 0;
    my $good = 0;
    my $total = @tests;
    local($ENV{'PERL5LIB'}) = join($Config{path_sep}, @INC); # pass -I flags to children

    my $t_start = new Benchmark;
    while ($test = shift(@tests)) {
	$te = $test;
	chop($te);
	print "$te" . '.' x (20 - length($te));
	my $fh = new FileHandle;
	$fh->open("$^X $switches $test|") || (print "can't run. $!\n");
	$ok = $next = $max = 0;
	@failed = ();
	while (<$fh>) {
	    if( $verbose ){
		print $_;
	    }
	    unless (/^\s*\#/) {
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
			warn "Aborting test: output counter mismatch [test $this answered when test $next expected]\n";
			last;
		    }
		    $next = $this + 1;
		}
	    }
	}
	$fh->close; # must close to reap child resource values
	my $wstatus = $?;
	my $estatus = $wstatus >> 8;
	if ($ok == $max && $next == $max+1 && ! $estatus) {
	    print "ok\n";
	    $good++;
	} elsif ($max) {
	    if ($next <= $max) {
		push @failed, $next..$max;
	    }
	    if (@failed) {
		print canonfailed($max,@failed);
	    } else {
		print "Don't know which tests failed for some reason\n";
	    }
	    $bad++;
	} elsif ($next == 0) {
	    print "FAILED before any test output arrived\n";
	    $bad++;
	}
	if ($wstatus) {
	    print "\tTest returned status $estatus (wstat $wstatus)\n";
	}
    }
    my $t_total = timediff(new Benchmark, $t_start);
    
    if ($bad == 0 && $totmax) {
	    print "All tests successful.\n";
    } elsif ($total==0){
	die "FAILED--no tests were run for some reason.\n";
    } elsif ($totmax==0) {
	my $blurb = $total==1 ? "script" : "scripts";
	die "FAILED--$total test $blurb could be run, alas -- no output ever seen\n";
    } else {
	$pct = sprintf("%.2f", $good / $total * 100);
	my $subpct = sprintf " %d/%d subtests failed, %.2f%% okay.",
	$totmax - $totok, $totmax, 100*$totok/$totmax;
	if ($bad == 1) {
	    die "Failed 1 test script, $pct% okay.$subpct\n";
	} else {
	    die "Failed $bad/$total test scripts, $pct% okay.$subpct\n";
	}
    }
    printf("Files=%d,  Tests=%d, %s\n", $files, $totmax, timestr($t_total, 'nop'));
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
    } else {
	push @result, "FAILED test $last\n";
    }

    push @result, "\tFailed $failed/$max tests, ";
    push @result, sprintf("%.2f",100*(1-$failed/$max)), "% okay\n";
    join "", @result;
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
output by a standard test scxript is C<"1..M"> with C<M> being the
number of tests that should be run within the test
script. Test::Harness::runscripts(@tests) runs all the testscripts
named as arguments and checks standard output for the expected
C<"ok N"> strings.

After all tests have been performed, runscripts() prints some
performance statistics that are computed by the Benchmark module.

=head2 The test script output

Any output from the testscript to standard error is ignored and
bypassed, thus will be seen by the user. Lines written to standard
output that look like perl comments (start with C</^\s*\#/>) are
discarded. Lines containing C</^(not\s+)?ok\b/> are interpreted as
feedback for runtests().

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
used to let runscripts() display the standard output of the script
without altering the behavior otherwise.

=head1 EXPORT

C<&runscripts> is exported by Test::Harness per default.

=head1 DIAGNOSTICS

=over 4

=item C<All tests successful.\nFiles=%d,  Tests=%d, %s>

If all tests are successful some statistics about the performance are
printed.

=item C<FAILED tests %s\n\tFailed %d/%d tests, %.2f%% okay.>

For any single script that has failing subtests statistics like the
above are printed.

=item C<Test returned status %d (wstat %d)>

Scripts that return a non-zero exit status, both $?>>8 and $? are
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
with perl distributions for ages. Current maintainer is Andreas
Koenig.

=head1 BUGS

Test::Harness uses $^X to determine the perl binary to run the tests
with. Test scripts running via the shebang (C<#!>) line may not be
portable because $^X is not consistent for shebang scripts across
platforms. This is no problem when Test::Harness is run with an
absolute path to the perl binary or when $^X can be found in the path.

=cut
