#!./perl

# 2 purpose file: 1-test 2-demonstrate (via args, -v -a options)

=head1 SYNOPSIS

To verify that B::Concise properly reports whether functions are XS or
perl, we test against 2 (currently) core packages which have lots of
XS functions: B and Digest::MD5.  They're listed in %$testpkgs, along
with a list of functions that are (or are not) XS.  For brevity, you
can specify the shorter list; if they're non-xs routines, start list
with a '!'.  Data::Dumper is also tested, partly to prove the non-!
usage.

We demand-load each package, scan its stash for function names, and
mark them as XS/not-XS according to the list given for each package.
Then we test B::Concise's report on each.

=head1 OPTIONS AND ARGUMENTS

C<-v> and C<-V> trigger 2 levels of verbosity.

C<-a> uses Module::CoreList to run all core packages through the test, which
gives some interesting results.

C<-c> causes the expected XS/non-XS results to be marked with
corrections, which are then reported at program END, in a
Data::Dumper statement

C<< -r <file> >> reads a file, as written by C<-c>, and adjusts the expected
results accordingly.  The file is 'required', so @INC settings apply.

If module-names are given as args, those packages are run through the
test harness; this is handy for collecting further items to test, and
may be useful otherwise (ie just to see).

=head1 EXAMPLES

All following examples avoid using PERL_CORE=1, since that changes @INC

=over 4

=item ./perl -Ilib -wS ext/B/t/concise-xs.t -c Storable

Tests Storable.pm for XS/non-XS routines, writes findings (along with
test results) to stdout.  You could edit results to produce a test
file, as in next example

=item ./perl -Ilib -wS ext/B/t/concise-xs.t -r ./storable

Loads file, and uses it to set expectations, and run tests

=item ./perl -Ilib -wS ext/B/t/concise-xs.t -avc > ../foo-avc 2> ../foo-avc2

Gets module list from Module::Corelist, and runs them all through the
test.  Since -c is used, this generates corrections, which are saved
in a file, which is edited down to produce ../all-xs

=item ./perl -Ilib -wS ext/B/t/concise-xs.t -cr ../all-xs > ../foo 2> ../foo2

This runs the tests specified in the file created in previous example.
-c is used again, and stdout verifies that all the expected results
given by -r ../all-xs are now seen.

Looking at ../foo2, you'll see 34 occurrences of the following error:

# err: Can't use an undefined value as a SCALAR reference at
# lib/B/Concise.pm line 634, <DATA> line 1.

=back

=cut

BEGIN {
    if ($ENV{PERL_CORE}) {
	chdir('t') if -d 't';
	@INC = ('.', '../lib');
    } else {
	unshift @INC, 't';
	push @INC, "../../t";
    }
    require Config;
    if (($Config::Config{'extensions'} !~ /\bB\b/) ){
        print "1..0 # Skip -- Perl configured without B module\n";
        exit 0;
    }
    unless ($Config::Config{useperlio}) {
        print "1..0 # Skip -- Perl configured without perlio\n";
        exit 0;
    }
}

use Getopt::Std;
use Carp;
# One 5.009-only test to go when no 6; is integrated (25344)
use Test::More tests => ( 1 * !!$Config::Config{useithreads}
			  + 1 * ($] > 5.009)
			  + 778);

require_ok("B::Concise");

my $testpkgs = {

    Digest::MD5 => [qw/ ! import /],

    B => [qw/ ! class clearsym compile_stats debug objsym parents
	      peekop savesym timing_info walkoptree_exec
	      walkoptree_slow walksymtable /],

    Data::Dumper => [qw/ bootstrap Dumpxs /],

    B::Deparse => [qw/ ASSIGN CVf_ASSERTION CVf_LOCKED CVf_LVALUE
		   CVf_METHOD LIST_CONTEXT OP_CONST OP_LIST OP_RV2SV
		   OP_STRINGIFY OPf_KIDS OPf_MOD OPf_REF OPf_SPECIAL
		   OPf_STACKED OPf_WANT OPf_WANT_LIST OPf_WANT_SCALAR
		   OPf_WANT_VOID OPpCONST_ARYBASE OPpCONST_BARE
		   OPpENTERSUB_AMPER OPpEXISTS_SUB OPpITER_REVERSED
		   OPpLVAL_INTRO OPpOUR_INTRO OPpSLICE OPpSORT_DESCEND
		   OPpSORT_INPLACE OPpSORT_INTEGER OPpSORT_NUMERIC
		   OPpSORT_REVERSE OPpTARGET_MY OPpTRANS_COMPLEMENT
		   OPpTRANS_DELETE OPpTRANS_SQUASH PMf_CONTINUE
		   PMf_EVAL PMf_EXTENDED PMf_FOLD PMf_GLOBAL PMf_KEEP
		   PMf_MULTILINE PMf_ONCE PMf_SINGLELINE PMf_SKIPWHITE
		   POSTFIX SVf_FAKE SVf_IOK SVf_NOK SVf_POK SVf_ROK
		   SVpad_OUR SVs_RMG SVs_SMG SWAP_CHILDREN main_cv
		   main_root main_start opnumber perlstring
		   svref_2object /],

};

############

B::Concise::compile('-nobanner');	# set a silent default
getopts('vaVcr:', \my %opts) or
    die <<EODIE;

usage: PERL_CORE=1 ./perl ext/B/t/concise-xs.t [-av] [module-list]
    tests ability to discern XS funcs using Digest::MD5 package
    -v	: runs verbosely
    -V	: more verbosity
    -a	: runs all modules in CoreList
    -c  : writes test corrections as a Data::Dumper expression
    -r <file>	: reads file of tests, as written by -c
    <args>	: additional modules are loaded and tested
    	(will report failures, since no XS funcs are known aprior)

EODIE
    ;

if (%opts) {
    require Data::Dumper;
    Data::Dumper->import('Dumper');
    $Data::Dumper::Sortkeys = 1;
}
my @argpkgs = @ARGV;
my %report;

if ($opts{r}) {
    my $refpkgs = require "$opts{r}";
    $testpkgs->{$_} = $refpkgs->{$_} foreach keys %$refpkgs;
}

unless ($opts{a}) {
    unless (@argpkgs) {
	foreach $pkg (sort keys %$testpkgs) {
	    test_pkg($pkg, $testpkgs->{$pkg});
	}
    } else {
	foreach $pkg (@argpkgs) {
	    test_pkg($pkg, $testpkgs->{$pkg});
	}
    }
} else {
    corecheck();
}
############

sub test_pkg {
    my ($pkg_name, $xslist) = @_;
    require_ok($pkg_name);

    unless (ref $xslist eq 'ARRAY') {
	warn "no XS/non-XS function list given, assuming empty XS list";
	$xslist = [''];
    }

    my $assumeXS = 0;	# assume list enumerates XS funcs, not perl ones
    $assumeXS = 1	if $xslist->[0] and $xslist->[0] eq '!';

    # build %stash: keys are func-names, vals: 1 if XS, 0 if not
    my (%stash) = map
	( ($_ => $assumeXS)
	  => ( grep exists &{"$pkg_name\::$_"}	# grab CODE symbols
	       => grep !/__ANON__/		# but not anon subs
	       => keys %{$pkg_name.'::'}	# from symbol table
	       ));

    # now invert according to supplied list
    $stash{$_} = int ! $assumeXS foreach @$xslist;

    # and cleanup cruft (easier than preventing)
    delete @stash{'!',''};

    if ($opts{v}) {
	diag("xslist: " => Dumper($xslist));
	diag("$pkg_name stash: " => Dumper(\%stash));
    }
    my $err;
    foreach $func_name (reverse sort keys %stash) {
	my $res = checkXS("${pkg_name}::$func_name", $stash{$func_name});
	if (!$res) {
	    $stash{$func_name} ^= 1;
	    print "$func_name ";
	    $err++;
	}
    }
    $report{$pkg_name} = \%stash if $opts{c} and $err || $opts{v};
}

sub checkXS {
    my ($func_name, $wantXS) = @_;

    my ($buf, $err) = render($func_name);
    if ($wantXS) {
	like($buf, qr/\Q$func_name is XS code/,
	     "XS code:\t $func_name");
    } else {
	unlike($buf, qr/\Q$func_name is XS code/,
	       "perl code:\t $func_name");
    }
    #returns like or unlike, whichever was called
}

sub render {
    my ($func_name) = @_;

    B::Concise::reset_sequence();
    B::Concise::walk_output(\my $buf);

    my $walker = B::Concise::compile($func_name);
    eval { $walker->() };
    diag("err: $@ $buf") if $@;
    diag("verbose: $buf") if $opts{V};

    return ($buf, $@);
}

sub corecheck {

    eval { require Module::CoreList };
    if ($@) {
	warn "Module::CoreList not available on $]\n";
	return;
    }
    my $mods = $Module::CoreList::version{'5.009002'};
    $mods = [ sort keys %$mods ];
    print Dumper($mods);

    foreach my $pkgnm (@$mods) {
	test_pkg($pkgnm);
    }
}

END {
    if ($opts{c}) {
	# print "Corrections: ", Dumper(\%report);
	print "# Tested Package Subroutines, 1's are XS, 0's are perl\n";
	print "\$VAR1 = {\n";

	foreach my $pkg (sort keys %report) {
	    my (@xs, @perl);
	    my $stash = $report{$pkg};

	    @xs   = sort grep $stash->{$_} == 1, keys %$stash;
	    @perl = sort grep $stash->{$_} == 0, keys %$stash;

	    my @list = (@xs > @perl) ? ( '!', @perl) : @xs;
	    print "\t$pkg => [qw/ @list /],\n";
	}
	print "};\n";
    }
}

__END__
