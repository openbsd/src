# This is a replacement for the old BEGIN preamble which heads (or
# should head) up every core test program to prepare it for running.
# Now instead of:
#
# BEGIN {
#   chdir 't' if -d 't';
#   @INC = '../lib';
# }
#
# Its primary purpose is to clear @INC so core tests don't pick up
# modules from an installed Perl.
#
# t/TEST will use -MTestInit.  You may "use TestInit" in the test
# programs but it is not required.
#
# P.S. This documentation is not in POD format in order to avoid
# problems when there are fundamental bugs in perl.

package TestInit;

$VERSION = 1.02;

# Let tests know they're running in the perl core.  Useful for modules
# which live dual lives on CPAN.
# Don't interfere with the taintedness of %ENV, this could perturbate tests.
# This feels like a better solution than the original, from
# http://www.xray.mpe.mpg.de/mailing-lists/perl5-porters/2003-07/msg00154.html
$ENV{PERL_CORE} = $^X;

sub new_inc {
    if (${^TAINT}) {
	@INC = @_;
    } else {
	@INC = (@_, '.');
    }
}

sub set_opt {
    my $sep;
    if ($^O eq 'VMS') {
	$sep = '|';
    } elsif ($^O eq 'MSWin32') {
	$sep = ';';
    } else {
	$sep = ':';
    }

    my $lib = join $sep, @_;
    if (exists $ENV{PERL5LIB}) {
	$ENV{PERL5LIB} = $lib . substr $ENV{PERL5LIB}, 0, 0;
    } else {
	$ENV{PERL5LIB} = $lib;
    }
}

my @up_2_t = ('../../lib', '../../t');
# This is incompatible with the import options.
if (-f 't/TEST' && -f 'MANIFEST' && -d 'lib' && -d 'ext') {
    # We're being run from the top level. Try to change directory, and set
    # things up correctly. This is a 90% solution, but for hand-running tests,
    # that's good enough
    if ($0 =~ s!^((?:ext|dist|cpan)[\\/][^\\/]+)[\//](.*\.t)$!$2!) {
	# Looks like a test in ext.
	chdir $1 or die "Can't chdir '$1': $!";
	new_inc(@up_2_t);
	set_opt(@up_2_t);
	$^X =~ s!^\./!../../!;
	$^X =~ s!^\.\\!..\\..\\!;
    } else {
	chdir 't' or die "Can't chdir 't': $!";
	new_inc('../lib');
    }
} else {
    new_inc('../lib');
}

sub import {
    my $self = shift;
    my $abs;
    foreach (@_) {
	if ($_ eq 'U2T') {
	    @new_inc = @up_2_t;
	} elsif ($_ eq 'NC') {
	    delete $ENV{PERL_CORE}
	} elsif ($_ eq 'A') {
	    $abs = 1;
	} else {
	    die "Unknown option '$_'";
	}
    }

    if ($abs) {
	if(!@new_inc) {
	    @new_inc = '../lib';
	}
	@INC = @new_inc;
	require File::Spec::Functions;
	# Forcibly untaint this.
	@new_inc = map { $_ = File::Spec::Functions::rel2abs($_); /(.*)/; $1 }
	    @new_inc;
	$^X = File::Spec::Functions::rel2abs($^X);
    }

    if (@new_inc) {
	new_inc(@new_inc);
	set_opt(@new_inc);
    }
}

$0 =~ s/\.dp$//; # for the test.deparse make target
1;

