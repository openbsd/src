package Carp;

=head1 NAME

carp - warn of errors (from perspective of caller)

croak - die of errors (from perspective of caller)

confess - die of errors with stack backtrace

=head1 SYNOPSIS

    use Carp;
    croak "We're outta here!";

=head1 DESCRIPTION

The Carp routines are useful in your own modules because
they act like die() or warn(), but report where the error
was in the code they were called from.  Thus if you have a 
routine Foo() that has a carp() in it, then the carp() 
will report the error as occurring where Foo() was called, 
not where carp() was called.

=cut

# This package implements handy routines for modules that wish to throw
# exceptions outside of the current package.

$CarpLevel = 0;		# How many extra package levels to skip on carp.
$MaxEvalLen = 0;	# How much eval '...text...' to show. 0 = all.

require Exporter;
@ISA = Exporter;
@EXPORT = qw(confess croak carp);

sub longmess {
    my $error = shift;
    my $mess = "";
    my $i = 1 + $CarpLevel;
    my ($pack,$file,$line,$sub,$eval,$require);
    while (($pack,$file,$line,$sub,undef,undef,$eval,$require) = caller($i++)) {
	if ($error =~ m/\n$/) {
	    $mess .= $error;
	} else {
	    if (defined $eval) {
	        if ($require) {
		    $sub = "require $eval";
		} else {
		    $eval =~ s/[\\\']/\\$&/g;
		    if ($MaxEvalLen && length($eval) > $MaxEvalLen) {
			substr($eval,$MaxEvalLen) = '...';
		    }
		    $sub = "eval '$eval'";
		}
	    } elsif ($sub eq '(eval)') {
		$sub = 'eval {...}';
	    }
	    $mess .= "\t$sub " if $error eq "called";
	    $mess .= "$error at $file line $line\n";
	}
	$error = "called";
    }
    $mess || $error;
}

sub shortmess {	# Short-circuit &longmess if called via multiple packages
    my $error = $_[0];	# Instead of "shift"
    my ($curpack) = caller(1);
    my $extra = $CarpLevel;
    my $i = 2;
    my ($pack,$file,$line);
    while (($pack,$file,$line) = caller($i++)) {
	if ($pack ne $curpack) {
	    if ($extra-- > 0) {
		$curpack = $pack;
	    }
	    else {
		return "$error at $file line $line\n";
	    }
	}
    }
    goto &longmess;
}

sub confess { die longmess @_; }
sub croak { die shortmess @_; }
sub carp { warn shortmess @_; }

1;
