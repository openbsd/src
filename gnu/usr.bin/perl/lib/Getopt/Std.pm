package Getopt::Std;
require 5.000;
require Exporter;

=head1 NAME

Getopt::Std, getopt, getopts - Process single-character switches with switch clustering

=head1 SYNOPSIS

    use Getopt::Std;

    getopt('oDI');    # -o, -D & -I take arg.  Sets $opt_* as a side effect.
    getopt('oDI', \%opts);    # -o, -D & -I take arg.  Values in %opts
    getopts('oif:');  # -o & -i are boolean flags, -f takes an argument
		      # Sets $opt_* as a side effect.
    getopts('oif:', \%opts);  # options as above. Values in %opts

=head1 DESCRIPTION

The getopt() function processes single-character switches with switch
clustering.  Pass one argument which is a string containing all switches
that take an argument.  For each switch found, sets $opt_x (where x is the
switch name) to the value of the argument if an argument is expected,
or 1 otherwise.  Switches which take an argument don't care whether
there is a space between the switch and the argument.

The getopts() function is similar, but you should pass to it the list of all
switches to be recognized.  If unspecified switches are found on the
command-line, the user will be warned that an unknown option was given.

Note that, if your code is running under the recommended C<use strict
'vars'> pragma, you will need to declare these package variables
with "our":

    our($opt_x, $opt_y);

For those of you who don't like additional global variables being created, getopt()
and getopts() will also accept a hash reference as an optional second argument. 
Hash keys will be x (where x is the switch name) with key values the value of
the argument or 1 if no argument is specified.

To allow programs to process arguments that look like switches, but aren't,
both functions will stop processing switches when they see the argument
C<-->.  The C<--> will be removed from @ARGV.

=cut

@ISA = qw(Exporter);
@EXPORT = qw(getopt getopts);
$VERSION = '1.03';

# Process single-character switches with switch clustering.  Pass one argument
# which is a string containing all switches that take an argument.  For each
# switch found, sets $opt_x (where x is the switch name) to the value of the
# argument, or 1 if no argument.  Switches which take an argument don't care
# whether there is a space between the switch and the argument.

# Usage:
#	getopt('oDI');  # -o, -D & -I take arg.  Sets opt_* as a side effect.

sub getopt (;$$) {
    my ($argumentative, $hash) = @_;
    $argumentative = '' if !defined $argumentative;
    my ($first,$rest);
    local $_;
    local @EXPORT;

    while (@ARGV && ($_ = $ARGV[0]) =~ /^-(.)(.*)/) {
	($first,$rest) = ($1,$2);
	if (/^--$/) {	# early exit if --
	    shift @ARGV;
	    last;
	}
	if (index($argumentative,$first) >= 0) {
	    if ($rest ne '') {
		shift(@ARGV);
	    }
	    else {
		shift(@ARGV);
		$rest = shift(@ARGV);
	    }
	    if (ref $hash) {
	        $$hash{$first} = $rest;
	    }
	    else {
	        ${"opt_$first"} = $rest;
	        push( @EXPORT, "\$opt_$first" );
	    }
	}
	else {
	    if (ref $hash) {
	        $$hash{$first} = 1;
	    }
	    else {
	        ${"opt_$first"} = 1;
	        push( @EXPORT, "\$opt_$first" );
	    }
	    if ($rest ne '') {
		$ARGV[0] = "-$rest";
	    }
	    else {
		shift(@ARGV);
	    }
	}
    }
    unless (ref $hash) { 
	local $Exporter::ExportLevel = 1;
	import Getopt::Std;
    }
}

# Usage:
#   getopts('a:bc');	# -a takes arg. -b & -c not. Sets opt_* as a
#			#  side effect.

sub getopts ($;$) {
    my ($argumentative, $hash) = @_;
    my (@args,$first,$rest);
    my $errs = 0;
    local $_;
    local @EXPORT;

    @args = split( / */, $argumentative );
    while(@ARGV && ($_ = $ARGV[0]) =~ /^-(.)(.*)/) {
	($first,$rest) = ($1,$2);
	if (/^--$/) {	# early exit if --
	    shift @ARGV;
	    last;
	}
	$pos = index($argumentative,$first);
	if ($pos >= 0) {
	    if (defined($args[$pos+1]) and ($args[$pos+1] eq ':')) {
		shift(@ARGV);
		if ($rest eq '') {
		    ++$errs unless @ARGV;
		    $rest = shift(@ARGV);
		}
		if (ref $hash) {
		    $$hash{$first} = $rest;
		}
		else {
		    ${"opt_$first"} = $rest;
		    push( @EXPORT, "\$opt_$first" );
		}
	    }
	    else {
		if (ref $hash) {
		    $$hash{$first} = 1;
		}
		else {
		    ${"opt_$first"} = 1;
		    push( @EXPORT, "\$opt_$first" );
		}
		if ($rest eq '') {
		    shift(@ARGV);
		}
		else {
		    $ARGV[0] = "-$rest";
		}
	    }
	}
	else {
	    warn "Unknown option: $first\n";
	    ++$errs;
	    if ($rest ne '') {
		$ARGV[0] = "-$rest";
	    }
	    else {
		shift(@ARGV);
	    }
	}
    }
    unless (ref $hash) { 
	local $Exporter::ExportLevel = 1;
	import Getopt::Std;
    }
    $errs == 0;
}

1;
