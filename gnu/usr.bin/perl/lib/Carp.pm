package Carp;

=head1 NAME

carp    - warn of errors (from perspective of caller)

cluck   - warn of errors with stack backtrace
          (not exported by default)

croak   - die of errors (from perspective of caller)

confess - die of errors with stack backtrace

=head1 SYNOPSIS

    use Carp;
    croak "We're outta here!";

    use Carp qw(cluck);
    cluck "This is how we got here!";

=head1 DESCRIPTION

The Carp routines are useful in your own modules because
they act like die() or warn(), but report where the error
was in the code they were called from.  Thus if you have a 
routine Foo() that has a carp() in it, then the carp() 
will report the error as occurring where Foo() was called, 
not where carp() was called.

=head2 Forcing a Stack Trace

As a debugging aid, you can force Carp to treat a croak as a confess
and a carp as a cluck across I<all> modules. In other words, force a
detailed stack trace to be given.  This can be very helpful when trying
to understand why, or from where, a warning or error is being generated.

This feature is enabled by 'importing' the non-existant symbol
'verbose'. You would typically enable it by saying

    perl -MCarp=verbose script.pl

or by including the string C<MCarp=verbose> in the L<PERL5OPT>
environment variable.

=cut

# This package is heavily used. Be small. Be fast. Be good.

$CarpLevel = 0;		# How many extra package levels to skip on carp.
$MaxEvalLen = 0;	# How much eval '...text...' to show. 0 = all.
$MaxArgLen = 64;        # How much of each argument to print. 0 = all.
$MaxArgNums = 8;        # How many arguments to print. 0 = all.

require Exporter;
@ISA = ('Exporter');
@EXPORT = qw(confess croak carp);
@EXPORT_OK = qw(cluck verbose);
@EXPORT_FAIL = qw(verbose);	# hook to enable verbose mode

sub export_fail {
    shift;
    if ($_[0] eq 'verbose') {
	local $^W = 0;
	*shortmess = \&longmess;
	shift;
    }
    return @_;
}


sub longmess {
    my $error = join '', @_;
    my $mess = "";
    my $i = 1 + $CarpLevel;
    my ($pack,$file,$line,$sub,$hargs,$eval,$require);
    my (@a);
    while (do { { package DB; @a = caller($i++) } } ) {
      ($pack,$file,$line,$sub,$hargs,undef,$eval,$require) = @a;
	if ($error =~ m/\n$/) {
	    $mess .= $error;
	} else {
	    if (defined $eval) {
	        if ($require) {
		    $sub = "require $eval";
		} else {
		    $eval =~ s/([\\\'])/\\$1/g;
		    if ($MaxEvalLen && length($eval) > $MaxEvalLen) {
			substr($eval,$MaxEvalLen) = '...';
		    }
		    $sub = "eval '$eval'";
		}
	    } elsif ($sub eq '(eval)') {
		$sub = 'eval {...}';
	    }
	    if ($hargs) {
	      @a = @DB::args;	# must get local copy of args
	      if ($MaxArgNums and @a > $MaxArgNums) {
		$#a = $MaxArgNums;
		$a[$#a] = "...";
	      }
	      for (@a) {
		$_ = "undef", next unless defined $_;
		if (ref $_) {
		  $_ .= '';
		  s/'/\\'/g;
		}
		else {
		  s/'/\\'/g;
		  substr($_,$MaxArgLen) = '...'
		    if $MaxArgLen and $MaxArgLen < length;
		}
		$_ = "'$_'" unless /^-?[\d.]+$/;
		s/([\200-\377])/sprintf("M-%c",ord($1)&0177)/eg;
		s/([\0-\37\177])/sprintf("^%c",ord($1)^64)/eg;
	      }
	      $sub .= '(' . join(', ', @a) . ')';
	    }
	    $mess .= "\t$sub " if $error eq "called";
	    $mess .= "$error at $file line $line\n";
	}
	$error = "called";
    }
    # this kludge circumvents die's incorrect handling of NUL
    my $msg = \($mess || $error);
    $$msg =~ tr/\0//d;
    $$msg;
}

sub shortmess {	# Short-circuit &longmess if called via multiple packages
    my $error = join '', @_;
    my ($prevpack) = caller(1);
    my $extra = $CarpLevel;
    my $i = 2;
    my ($pack,$file,$line);
    my %isa = ($prevpack,1);

    @isa{@{"${prevpack}::ISA"}} = ()
	if(defined @{"${prevpack}::ISA"});

    while (($pack,$file,$line) = caller($i++)) {
	if(defined @{$pack . "::ISA"}) {
	    my @i = @{$pack . "::ISA"};
	    my %i;
	    @i{@i} = ();
	    @isa{@i,$pack} = ()
		if(exists $i{$prevpack} || exists $isa{$pack});
	}

	next
	    if(exists $isa{$pack});

	if ($extra-- > 0) {
	    %isa = ($pack,1);
	    @isa{@{$pack . "::ISA"}} = ()
		if(defined @{$pack . "::ISA"});
	}
	else {
	    # this kludge circumvents die's incorrect handling of NUL
	    (my $msg = "$error at $file line $line\n") =~ tr/\0//d;
	    return $msg;
	}
    }
    continue {
	$prevpack = $pack;
    }

    goto &longmess;
}

sub confess { die longmess @_; }
sub croak { die shortmess @_; }
sub carp { warn shortmess @_; }
sub cluck { warn longmess @_; }

1;
