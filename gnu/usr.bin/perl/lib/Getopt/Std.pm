package Getopt::Std;
require 5.000;
require Exporter;

=head1 NAME

getopt - Process single-character switches with switch clustering

getopts - Process single-character switches with switch clustering

=head1 SYNOPSIS

    use Getopt::Std;
    getopt('oDI');  # -o, -D & -I take arg.  Sets opt_* as a side effect.
    getopts('oif:');  # -o & -i are boolean flags, -f takes an argument
		      # Sets opt_* as a side effect.

=head1 DESCRIPTION

The getopt() functions processes single-character switches with switch
clustering.  Pass one argument which is a string containing all switches
that take an argument.  For each switch found, sets $opt_x (where x is the
switch name) to the value of the argument, or 1 if no argument.  Switches
which take an argument don't care whether there is a space between the
switch and the argument.

=cut

@ISA = qw(Exporter);
@EXPORT = qw(getopt getopts);

# $RCSfile: getopt.pl,v $$Revision: 4.1 $$Date: 92/08/07 18:23:58 $

# Process single-character switches with switch clustering.  Pass one argument
# which is a string containing all switches that take an argument.  For each
# switch found, sets $opt_x (where x is the switch name) to the value of the
# argument, or 1 if no argument.  Switches which take an argument don't care
# whether there is a space between the switch and the argument.

# Usage:
#	getopt('oDI');  # -o, -D & -I take arg.  Sets opt_* as a side effect.

sub getopt {
    local($argumentative) = @_;
    local($_,$first,$rest);
    local $Exporter::ExportLevel;

    while (@ARGV && ($_ = $ARGV[0]) =~ /^-(.)(.*)/) {
	($first,$rest) = ($1,$2);
	if (index($argumentative,$first) >= 0) {
	    if ($rest ne '') {
		shift(@ARGV);
	    }
	    else {
		shift(@ARGV);
		$rest = shift(@ARGV);
	    }
	    eval "\$opt_$first = \$rest;";
	    push( @EXPORT, "\$opt_$first" );
	}
	else {
	    eval "\$opt_$first = 1;";
	    push( @EXPORT, "\$opt_$first" );
	    if ($rest ne '') {
		$ARGV[0] = "-$rest";
	    }
	    else {
		shift(@ARGV);
	    }
	}
    }
    $Exporter::ExportLevel++;
    import Getopt::Std;
}

# Usage:
#   getopts('a:bc');	# -a takes arg. -b & -c not. Sets opt_* as a
#			#  side effect.

sub getopts {
    local($argumentative) = @_;
    local(@args,$_,$first,$rest);
    local($errs) = 0;
    local $Exporter::ExportLevel;

    @args = split( / */, $argumentative );
    while(@ARGV && ($_ = $ARGV[0]) =~ /^-(.)(.*)/) {
	($first,$rest) = ($1,$2);
	$pos = index($argumentative,$first);
	if($pos >= 0) {
	    if(defined($args[$pos+1]) and ($args[$pos+1] eq ':')) {
		shift(@ARGV);
		if($rest eq '') {
		    ++$errs unless @ARGV;
		    $rest = shift(@ARGV);
		}
		eval "\$opt_$first = \$rest;";
		push( @EXPORT, "\$opt_$first" );
	    }
	    else {
		eval "\$opt_$first = 1";
		push( @EXPORT, "\$opt_$first" );
		if($rest eq '') {
		    shift(@ARGV);
		}
		else {
		    $ARGV[0] = "-$rest";
		}
	    }
	}
	else {
	    print STDERR "Unknown option: $first\n";
	    ++$errs;
	    if($rest ne '') {
		$ARGV[0] = "-$rest";
	    }
	    else {
		shift(@ARGV);
	    }
	}
    }
    $Exporter::ExportLevel++;
    import Getopt::Std;
    $errs == 0;
}

1;

