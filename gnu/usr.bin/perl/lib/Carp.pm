package Carp;

our $VERSION = '1.17';

our $MaxEvalLen = 0;
our $Verbose    = 0;
our $CarpLevel  = 0;
our $MaxArgLen  = 64;   # How much of each argument to print. 0 = all.
our $MaxArgNums = 8;    # How many arguments to print. 0 = all.

require Exporter;
our @ISA = ('Exporter');
our @EXPORT = qw(confess croak carp);
our @EXPORT_OK = qw(cluck verbose longmess shortmess);
our @EXPORT_FAIL = qw(verbose);	# hook to enable verbose mode

# The members of %Internal are packages that are internal to perl.
# Carp will not report errors from within these packages if it
# can.  The members of %CarpInternal are internal to Perl's warning
# system.  Carp will not report errors from within these packages
# either, and will not report calls *to* these packages for carp and
# croak.  They replace $CarpLevel, which is deprecated.    The
# $Max(EvalLen|(Arg(Len|Nums)) variables are used to specify how the eval
# text and function arguments should be formatted when printed.

# disable these by default, so they can live w/o require Carp
$CarpInternal{Carp}++;
$CarpInternal{warnings}++;
$Internal{Exporter}++;
$Internal{'Exporter::Heavy'}++;

# if the caller specifies verbose usage ("perl -MCarp=verbose script.pl")
# then the following method will be called by the Exporter which knows
# to do this thanks to @EXPORT_FAIL, above.  $_[1] will contain the word
# 'verbose'.

sub export_fail { shift; $Verbose = shift if $_[0] eq 'verbose'; @_ }

sub longmess {
    # Icky backwards compatibility wrapper. :-(
    #
    # The story is that the original implementation hard-coded the
    # number of call levels to go back, so calls to longmess were off
    # by one.  Other code began calling longmess and expecting this
    # behaviour, so the replacement has to emulate that behaviour.
    my $call_pack = defined &{"CORE::GLOBAL::caller"} ? &{"CORE::GLOBAL::caller"}() : caller();
    if ($Internal{$call_pack} or $CarpInternal{$call_pack}) {
      return longmess_heavy(@_);
    }
    else {
      local $CarpLevel = $CarpLevel + 1;
      return longmess_heavy(@_);
    }
};

sub shortmess {
    # Icky backwards compatibility wrapper. :-(
    local @CARP_NOT = defined &{"CORE::GLOBAL::caller"} ? &{"CORE::GLOBAL::caller"}() : caller();
    shortmess_heavy(@_);
};

sub croak   { die  shortmess @_ }
sub confess { die  longmess  @_ }
sub carp    { warn shortmess @_ }
sub cluck   { warn longmess  @_ }

sub caller_info {
  my $i = shift(@_) + 1;
  my %call_info;
  {
  package DB;
  @args = \$i; # A sentinal, which no-one else has the address of
  @call_info{
    qw(pack file line sub has_args wantarray evaltext is_require)
  } = defined &{"CORE::GLOBAL::caller"} ? &{"CORE::GLOBAL::caller"}($i) : caller($i);
  }
  
  unless (defined $call_info{pack}) {
    return ();
  }

  my $sub_name = Carp::get_subname(\%call_info);
  if ($call_info{has_args}) {
    my @args;
    if (@DB::args == 1 && ref $DB::args[0] eq ref \$i && $DB::args[0] == \$i) {
      @DB::args = (); # Don't let anyone see the address of $i
      @args = "** Incomplete caller override detected; \@DB::args were not set **";
    } else {
      @args = map {Carp::format_arg($_)} @DB::args;
    }
    if ($MaxArgNums and @args > $MaxArgNums) { # More than we want to show?
      $#args = $MaxArgNums;
      push @args, '...';
    }
    # Push the args onto the subroutine
    $sub_name .= '(' . join (', ', @args) . ')';
  }
  $call_info{sub_name} = $sub_name;
  return wantarray() ? %call_info : \%call_info;
}

# Transform an argument to a function into a string.
sub format_arg {
  my $arg = shift;
  if (ref($arg)) {
      $arg = defined($overload::VERSION) ? overload::StrVal($arg) : "$arg";
  }
  if (defined($arg)) {
      $arg =~ s/'/\\'/g;
      $arg = str_len_trim($arg, $MaxArgLen);
  
      # Quote it?
      $arg = "'$arg'" unless $arg =~ /^-?[\d.]+\z/;
  } else {
      $arg = 'undef';
  }

  # The following handling of "control chars" is direct from
  # the original code - it is broken on Unicode though.
  # Suggestions?
  utf8::is_utf8($arg)
    or $arg =~ s/([[:cntrl:]]|[[:^ascii:]])/sprintf("\\x{%x}",ord($1))/eg;
  return $arg;
}

# Takes an inheritance cache and a package and returns
# an anon hash of known inheritances and anon array of
# inheritances which consequences have not been figured
# for.
sub get_status {
    my $cache = shift;
    my $pkg = shift;
    $cache->{$pkg} ||= [{$pkg => $pkg}, [trusts_directly($pkg)]];
    return @{$cache->{$pkg}};
}

# Takes the info from caller() and figures out the name of
# the sub/require/eval
sub get_subname {
  my $info = shift;
  if (defined($info->{evaltext})) {
    my $eval = $info->{evaltext};
    if ($info->{is_require}) {
      return "require $eval";
    }
    else {
      $eval =~ s/([\\\'])/\\$1/g;
      return "eval '" . str_len_trim($eval, $MaxEvalLen) . "'";
    }
  }

  return ($info->{sub} eq '(eval)') ? 'eval {...}' : $info->{sub};
}

# Figures out what call (from the point of view of the caller)
# the long error backtrace should start at.
sub long_error_loc {
  my $i;
  my $lvl = $CarpLevel;
  {
    ++$i;
    my $pkg = defined &{"CORE::GLOBAL::caller"} ? &{"CORE::GLOBAL::caller"}($i) : caller($i);
    unless(defined($pkg)) {
      # This *shouldn't* happen.
      if (%Internal) {
        local %Internal;
        $i = long_error_loc();
        last;
      }
      else {
        # OK, now I am irritated.
        return 2;
      }
    }
    redo if $CarpInternal{$pkg};
    redo unless 0 > --$lvl;
    redo if $Internal{$pkg};
  }
  return $i - 1;
}


sub longmess_heavy {
  return @_ if ref($_[0]); # don't break references as exceptions
  my $i = long_error_loc();
  return ret_backtrace($i, @_);
}

# Returns a full stack backtrace starting from where it is
# told.
sub ret_backtrace {
  my ($i, @error) = @_;
  my $mess;
  my $err = join '', @error;
  $i++;

  my $tid_msg = '';
  if (defined &threads::tid) {
    my $tid = threads->tid;
    $tid_msg = " thread $tid" if $tid;
  }

  my %i = caller_info($i);
  $mess = "$err at $i{file} line $i{line}$tid_msg\n";

  while (my %i = caller_info(++$i)) {
      $mess .= "\t$i{sub_name} called at $i{file} line $i{line}$tid_msg\n";
  }
  
  return $mess;
}

sub ret_summary {
  my ($i, @error) = @_;
  my $err = join '', @error;
  $i++;

  my $tid_msg = '';
  if (defined &threads::tid) {
    my $tid = threads->tid;
    $tid_msg = " thread $tid" if $tid;
  }

  my %i = caller_info($i);
  return "$err at $i{file} line $i{line}$tid_msg\n";
}


sub short_error_loc {
  # You have to create your (hash)ref out here, rather than defaulting it
  # inside trusts *on a lexical*, as you want it to persist across calls.
  # (You can default it on $_[2], but that gets messy)
  my $cache = {};
  my $i = 1;
  my $lvl = $CarpLevel;
  {

    my $called = defined &{"CORE::GLOBAL::caller"} ? &{"CORE::GLOBAL::caller"}($i) : caller($i);
    $i++;
    my $caller = defined &{"CORE::GLOBAL::caller"} ? &{"CORE::GLOBAL::caller"}($i) : caller($i);

    return 0 unless defined($caller); # What happened?
    redo if $Internal{$caller};
    redo if $CarpInternal{$caller};
    redo if $CarpInternal{$called};
    redo if trusts($called, $caller, $cache);
    redo if trusts($caller, $called, $cache);
    redo unless 0 > --$lvl;
  }
  return $i - 1;
}


sub shortmess_heavy {
  return longmess_heavy(@_) if $Verbose;
  return @_ if ref($_[0]); # don't break references as exceptions
  my $i = short_error_loc();
  if ($i) {
    ret_summary($i, @_);
  }
  else {
    longmess_heavy(@_);
  }
}

# If a string is too long, trims it with ...
sub str_len_trim {
  my $str = shift;
  my $max = shift || 0;
  if (2 < $max and $max < length($str)) {
    substr($str, $max - 3) = '...';
  }
  return $str;
}

# Takes two packages and an optional cache.  Says whether the
# first inherits from the second.
#
# Recursive versions of this have to work to avoid certain
# possible endless loops, and when following long chains of
# inheritance are less efficient.
sub trusts {
    my $child = shift;
    my $parent = shift;
    my $cache = shift;
    my ($known, $partial) = get_status($cache, $child);
    # Figure out consequences until we have an answer
    while (@$partial and not exists $known->{$parent}) {
        my $anc = shift @$partial;
        next if exists $known->{$anc};
        $known->{$anc}++;
        my ($anc_knows, $anc_partial) = get_status($cache, $anc);
        my @found = keys %$anc_knows;
        @$known{@found} = ();
        push @$partial, @$anc_partial;
    }
    return exists $known->{$parent};
}

# Takes a package and gives a list of those trusted directly
sub trusts_directly {
    my $class = shift;
    no strict 'refs';
    no warnings 'once'; 
    return @{"$class\::CARP_NOT"}
      ? @{"$class\::CARP_NOT"}
      : @{"$class\::ISA"};
}

1;

__END__

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
they act like die() or warn(), but with a message which is more
likely to be useful to a user of your module.  In the case of
cluck, confess, and longmess that context is a summary of every
call in the call-stack.  For a shorter message you can use C<carp>
or C<croak> which report the error as being from where your module
was called.  There is no guarantee that that is where the error
was, but it is a good educated guess.

You can also alter the way the output and logic of C<Carp> works, by
changing some global variables in the C<Carp> namespace. See the
section on C<GLOBAL VARIABLES> below.

Here is a more complete description of how C<carp> and C<croak> work.
What they do is search the call-stack for a function call stack where
they have not been told that there shouldn't be an error.  If every
call is marked safe, they give up and give a full stack backtrace
instead.  In other words they presume that the first likely looking
potential suspect is guilty.  Their rules for telling whether
a call shouldn't generate errors work as follows:

=over 4

=item 1.

Any call from a package to itself is safe.

=item 2.

Packages claim that there won't be errors on calls to or from
packages explicitly marked as safe by inclusion in C<@CARP_NOT>, or
(if that array is empty) C<@ISA>.  The ability to override what
@ISA says is new in 5.8.

=item 3.

The trust in item 2 is transitive.  If A trusts B, and B
trusts C, then A trusts C.  So if you do not override C<@ISA>
with C<@CARP_NOT>, then this trust relationship is identical to,
"inherits from".

=item 4.

Any call from an internal Perl module is safe.  (Nothing keeps
user modules from marking themselves as internal to Perl, but
this practice is discouraged.)

=item 5.

Any call to Perl's warning system (eg Carp itself) is safe.
(This rule is what keeps it from reporting the error at the
point where you call C<carp> or C<croak>.)

=item 6.

C<$Carp::CarpLevel> can be set to skip a fixed number of additional
call levels.  Using this is not recommended because it is very
difficult to get it to behave correctly.

=back

=head2 Forcing a Stack Trace

As a debugging aid, you can force Carp to treat a croak as a confess
and a carp as a cluck across I<all> modules. In other words, force a
detailed stack trace to be given.  This can be very helpful when trying
to understand why, or from where, a warning or error is being generated.

This feature is enabled by 'importing' the non-existent symbol
'verbose'. You would typically enable it by saying

    perl -MCarp=verbose script.pl

or by including the string C<-MCarp=verbose> in the PERL5OPT
environment variable.

Alternately, you can set the global variable C<$Carp::Verbose> to true.
See the C<GLOBAL VARIABLES> section below.

=head1 GLOBAL VARIABLES

=head2 $Carp::MaxEvalLen

This variable determines how many characters of a string-eval are to
be shown in the output. Use a value of C<0> to show all text.

Defaults to C<0>.

=head2 $Carp::MaxArgLen

This variable determines how many characters of each argument to a
function to print. Use a value of C<0> to show the full length of the
argument.

Defaults to C<64>.

=head2 $Carp::MaxArgNums

This variable determines how many arguments to each function to show.
Use a value of C<0> to show all arguments to a function call.

Defaults to C<8>.

=head2 $Carp::Verbose

This variable makes C<carp> and C<croak> generate stack backtraces
just like C<cluck> and C<confess>.  This is how C<use Carp 'verbose'>
is implemented internally.

Defaults to C<0>.

=head2 @CARP_NOT

This variable, I<in your package>, says which packages are I<not> to be
considered as the location of an error. The C<carp()> and C<cluck()>
functions will skip over callers when reporting where an error occurred.

NB: This variable must be in the package's symbol table, thus:

    # These work
    our @CARP_NOT; # file scope
    use vars qw(@CARP_NOT); # package scope
    @My::Package::CARP_NOT = ... ; # explicit package variable

    # These don't work
    sub xyz { ... @CARP_NOT = ... } # w/o declarations above
    my @CARP_NOT; # even at top-level

Example of use:

    package My::Carping::Package;
    use Carp;
    our @CARP_NOT;
    sub bar     { .... or _error('Wrong input') }
    sub _error  {
        # temporary control of where'ness, __PACKAGE__ is implicit
        local @CARP_NOT = qw(My::Friendly::Caller);
        carp(@_)
    }

This would make C<Carp> report the error as coming from a caller not
in C<My::Carping::Package>, nor from C<My::Friendly::Caller>.

Also read the L</DESCRIPTION> section above, about how C<Carp> decides
where the error is reported from.

Use C<@CARP_NOT>, instead of C<$Carp::CarpLevel>.

Overrides C<Carp>'s use of C<@ISA>.

=head2 %Carp::Internal

This says what packages are internal to Perl.  C<Carp> will never
report an error as being from a line in a package that is internal to
Perl.  For example:

    $Carp::Internal{ (__PACKAGE__) }++;
    # time passes...
    sub foo { ... or confess("whatever") };

would give a full stack backtrace starting from the first caller
outside of __PACKAGE__.  (Unless that package was also internal to
Perl.)

=head2 %Carp::CarpInternal

This says which packages are internal to Perl's warning system.  For
generating a full stack backtrace this is the same as being internal
to Perl, the stack backtrace will not start inside packages that are
listed in C<%Carp::CarpInternal>.  But it is slightly different for
the summary message generated by C<carp> or C<croak>.  There errors
will not be reported on any lines that are calling packages in
C<%Carp::CarpInternal>.

For example C<Carp> itself is listed in C<%Carp::CarpInternal>.
Therefore the full stack backtrace from C<confess> will not start
inside of C<Carp>, and the short message from calling C<croak> is
not placed on the line where C<croak> was called.

=head2 $Carp::CarpLevel

This variable determines how many additional call frames are to be
skipped that would not otherwise be when reporting where an error
occurred on a call to one of C<Carp>'s functions.  It is fairly easy
to count these call frames on calls that generate a full stack
backtrace.  However it is much harder to do this accounting for calls
that generate a short message.  Usually people skip too many call
frames.  If they are lucky they skip enough that C<Carp> goes all of
the way through the call stack, realizes that something is wrong, and
then generates a full stack backtrace.  If they are unlucky then the
error is reported from somewhere misleading very high in the call
stack.

Therefore it is best to avoid C<$Carp::CarpLevel>.  Instead use
C<@CARP_NOT>, C<%Carp::Internal> and C<%Carp::CarpInternal>.

Defaults to C<0>.

=head1 BUGS

The Carp routines don't handle exception objects currently.
If called with a first argument that is a reference, they simply
call die() or warn(), as appropriate.

