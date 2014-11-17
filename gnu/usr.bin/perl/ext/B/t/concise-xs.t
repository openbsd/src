#!./perl

# 2 purpose file: 1-test 2-demonstrate (via args, -v -a options)

=head1 SYNOPSIS

To verify that B::Concise properly reports whether functions are XS,
perl, or optimized constant subs, we test against a few core packages
which have a stable API, and which have functions of all 3 types.

=head1 WHAT IS TESTED

5 core packages are tested; Digest::MD5, B, B::Deparse, Data::Dumper,
and POSIX.  These have a mix of the 3 expected implementation types;
perl, XS, and constant (optimized constant subs).

%$testpkgs specifies what packages are tested; each package is loaded,
and the stash is scanned for the function-names in that package.

Each value in %$testpkgs is a hash-of-lists (HoL) whose keys are
implementation-types and values are lists of function-names of that type.

To keep these HoLs smaller and more manageable, they may carry an
additional 'dflt' => $impl_Type, which means that unnamed functions
are expected to be of that default implementation type.  Those unnamed
functions are known from the scan of the package stash.

=head1 HOW THEY'RE TESTED

Each function is 'rendered' by B::Concise, and result is matched
against regexs for each possible implementation-type.  For some
packages, some functions may be unimplemented on some platforms.

To slay this maintenance dragon, the regexs used in like() match
against renderings which indicate that there is no implementation.

If a function is implemented differently on different platforms, the
test for that function will fail on one of those platforms.  These
specific functions can be skipped by a 'skip' => [ @list ] to the HoL
mentioned previously.  See usage for skip in B's HoL, which avoids
testing a function which doesn't exist on non-threaded builds.

=head1 OPTIONS AND ARGUMENTS

C<-v> and C<-V> trigger 2 levels of verbosity.

C<-a> uses Module::CoreList to run all core packages through the test, which
gives some interesting results.

C<-c> causes the expected XS/non-XS results to be marked with
corrections, which are then reported at program END, in a form that's
readily cut-and-pastable into this file.


C<< -r <file> >> reads a file, as written by C<-c>, and adjusts the expected
results accordingly.  The file is 'required', so @INC settings apply.

If module-names are given as args, those packages are run through the
test harness; this is handy for collecting further items to test, and
may be useful otherwise (ie just to see).

=head1 EXAMPLES

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
    unshift @INC, 't';
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
use Test::More 'no_plan';

require_ok("B::Concise");

my %matchers = 
    ( constant	=> qr{ (?-x: is a constant sub, optimized to a \w+)
		      |(?-x: exists in stash, but has no START) }x,
      XS	=> qr/ is XS code/,
      perl	=> qr/ (next|db)state/,
      noSTART	=> qr/ exists in stash, but has no START/,
);

my $testpkgs = {
    # packages to test, with expected types for named funcs

    Digest::MD5 => { perl => [qw/ import /],
		     dflt => 'XS' },

    Data::Dumper => { XS => [qw/ bootstrap Dumpxs /,
			$] >= 5.015 ? qw/_vstring / : () ],
		    $] >= 5.015
			?  (constant => ['_bad_vsmg']) : (),
		      dflt => 'perl' },
    B => { 
	dflt => 'constant',		# all but 47/297
	skip => [ 'regex_padav' ],	# threaded only
	perl => [qw(
		    walksymtable walkoptree_slow walkoptree_exec
		    timing_info savesym peekop parents objsym debug
		    compile_stats clearsym class
		    )],
	XS => [qw(
		  warnhook walkoptree_debug walkoptree threadsv_names
		  svref_2object sv_yes sv_undef sv_no save_BEGINs
		  regex_padav ppname perlstring opnumber minus_c
		  main_start main_root main_cv init_av inc_gv hash
		  formfeed end_av dowarn diehook defstash curstash
		  cstring comppadlist check_av cchar cast_I32 bootstrap
		  begin_av amagic_generation sub_generation address
		  unitcheck_av) ],
    },

    B::Deparse => { dflt => 'perl',	# 236 functions

	XS => [qw( svref_2object perlstring opnumber main_start
		   main_root main_cv )],

	constant => [qw/ ASSIGN CVf_LVALUE
		     CVf_METHOD LIST_CONTEXT OP_CONST OP_LIST OP_RV2SV
		     OP_STRINGIFY OPf_KIDS OPf_MOD OPf_REF OPf_SPECIAL
		     OPf_STACKED OPf_WANT OPf_WANT_LIST OPf_WANT_SCALAR
		     OPf_WANT_VOID OPpCONST_BARE OPpCONST_NOVER
		     OPpENTERSUB_AMPER OPpEXISTS_SUB OPpITER_REVERSED
		     OPpLVAL_INTRO OPpOUR_INTRO OPpSLICE OPpSORT_DESCEND
		     OPpSORT_INPLACE OPpSORT_INTEGER OPpSORT_NUMERIC
		     OPpSORT_REVERSE OPpREVERSE_INPLACE OPpTARGET_MY
		     OPpTRANS_COMPLEMENT OPpTRANS_DELETE OPpTRANS_SQUASH
		     PMf_CONTINUE PMf_EVAL PMf_EXTENDED PMf_FOLD PMf_GLOBAL
		     PMf_KEEP PMf_NONDESTRUCT
		     PMf_MULTILINE PMf_ONCE PMf_SINGLELINE
		     POSTFIX SVf_FAKE SVf_IOK SVf_NOK SVf_POK SVf_ROK
		     SVpad_OUR SVs_RMG SVs_SMG SWAP_CHILDREN OPpPAD_STATE
		     OPpCONST_ARYBASE RXf_SKIPWHITE/,
		     $] >= 5.015 ? qw(
		     OP_GLOB PMf_SKIPWHITE RXf_PMf_CHARSET RXf_PMf_KEEPCOPY
		     OPpEVAL_BYTES OPpSUBSTR_REPL_FIRST) : (),
		     $] >= 5.019 ? qw(OP_PUSHMARK OP_NULL) : (),
		    'CVf_LOCKED', # This ends up as a constant, pre or post 5.10
		    ],
		 },

    POSIX => { dflt => 'constant',			# all but 252/589
	       skip => [qw/ _POSIX_JOB_CONTROL /,	# platform varying
			# Might be XS or imported from Fcntl, depending on your
			# perl version:
			qw / S_ISBLK S_ISCHR S_ISDIR S_ISFIFO S_ISREG /,
			# Might be XS or AUTOLOADed, depending on your perl
			# version:
			qw /WEXITSTATUS WIFEXITED WIFSIGNALED WIFSTOPPED
			    WSTOPSIG WTERMSIG/,
		       'int_macro_int', # Removed in POSIX 1.16
		       ],
	       perl => [qw/ import croak AUTOLOAD /,
			$] >= 5.015
			    ? qw/load_imports usage printf sprintf perror/
			    : (),
			],

	       XS => [qw/ write wctomb wcstombs uname tzset tzname
		      ttyname tmpnam times tcsetpgrp tcsendbreak
		      tcgetpgrp tcflush tcflow tcdrain tanh tan
		      sysconf strxfrm strtoul strtol strtod
		      strftime strcoll sinh sigsuspend sigprocmask
		      sigpending sigaction setuid setsid setpgid
		      setlocale setgid read pipe pause pathconf
		      open nice modf mktime mkfifo mbtowc mbstowcs
		      mblen lseek log10 localeconv ldexp lchown
		      isxdigit isupper isspace ispunct isprint
		      islower isgraph isdigit iscntrl isalpha
		      isalnum getcwd frexp fpathconf
		      fmod floor dup2 dup difftime cuserid ctime
		      ctermid cosh constant close clock ceil
		      bootstrap atan asin asctime acos access abort
		      _exit
		      /, $] >= 5.015 ? ('sleep') : () ],
	       },

    IO::Socket => { dflt => 'constant',		# 157/190

		    perl => [qw/ timeout socktype sockopt sockname
			     socketpair socket sockdomain sockaddr_un
			     sockaddr_in shutdown setsockopt send
			     register_domain recv protocol peername
			     new listen import getsockopt croak
			     connected connect configure confess close
			     carp bind atmark accept sockaddr_in6
			     blocking/ ],

		    XS => [qw/ unpack_sockaddr_un unpack_sockaddr_in
			   sockatmark sockaddr_family pack_sockaddr_un
			   pack_sockaddr_in inet_ntoa inet_aton
			   unpack_sockaddr_in6 pack_sockaddr_in6
			   /],
            # skip inet_ntop and inet_pton as they're not exported by default
		},
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
    	(will report failures, since no XS funcs are known apriori)

EODIE
    ;

if (%opts) {
    require Data::Dumper;
    Data::Dumper->import('Dumper');
    { my $x = \*Data::Dumper::Sortkeys } # shut up 'used once' warning
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
    my ($pkg, $fntypes) = @_;
    require_ok($pkg);

    # build %stash: keys are func-names, vals filled in below
    my (%stash) = map
	( ($_ => 0)
	  => ( grep exists &{"$pkg\::$_"}	# grab CODE symbols
	       => grep !/__ANON__/		# but not anon subs
	       => keys %{$pkg.'::'}		# from symbol table
	       ));

    for my $type (keys %matchers) {
	foreach my $fn (@{$fntypes->{$type}}) {
	    carp "$fn can only be one of $type, $stash{$fn}\n"
		if $stash{$fn};
	    $stash{$fn} = $type;
	}
    }
    # set default type for un-named functions
    my $dflt = $fntypes->{dflt} || 'perl';
    for my $k (keys %stash) {
	$stash{$k} = $dflt unless $stash{$k};
    }
    $stash{$_} = 'skip' foreach @{$fntypes->{skip}};

    if ($opts{v}) {
	diag("fntypes: " => Dumper($fntypes));
	diag("$pkg stash: " => Dumper(\%stash));
    }
    foreach my $fn (reverse sort keys %stash) {
	next if $stash{$fn} eq 'skip';
	my $res = checkXS("${pkg}::$fn", $stash{$fn});
	if ($res ne '1') {
	    push @{$report{$pkg}{$res}}, $fn;
	}
    }
}

sub checkXS {
    my ($func_name, $want) = @_;

    croak "unknown type $want: $func_name\n"
	unless defined $matchers{$want};

    my ($buf, $err) = render($func_name);
    my $res = like($buf, $matchers{$want}, "$want sub:\t $func_name");

    unless ($res) {
	# test failed. return type that would give success
	for my $m (keys %matchers) {
	    return $m if $buf =~ $matchers{$m};
	}
    }
    $res;
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
    { my $x = \*Module::CoreList::version } # shut up 'used once' warning
    my $mods = $Module::CoreList::version{'5.009002'};
    $mods = [ sort keys %$mods ];
    print Dumper($mods);

    foreach my $pkgnm (@$mods) {
	test_pkg($pkgnm);
    }
}

END {
    if ($opts{c}) {
	{ my $x = \*Data::Dumper::Indent } # shut up 'used once' warning
	$Data::Dumper::Indent = 1;
	print "Corrections: ", Dumper(\%report);

	foreach my $pkg (sort keys %report) {
	    for my $type (keys %matchers) {
		print "$pkg: $type: @{$report{$pkg}{$type}}\n"
		    if @{$report{$pkg}{$type}};
	    }
	}
    }
}

__END__
