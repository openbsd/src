=head1 NAME

Term::ReadLine - Perl interface to various C<readline> packages. If
no real package is found, substitutes stubs instead of basic functions.

=head1 SYNOPSIS

  use Term::ReadLine;
  $term = new Term::ReadLine 'Simple Perl calc';
  $prompt = "Enter your arithmetic expression: ";
  $OUT = $term->OUT || STDOUT;
  while ( defined ($_ = $term->readline($prompt)) ) {
    $res = eval($_), "\n";
    warn $@ if $@;
    print $OUT $res, "\n" unless $@;
    $term->addhistory($_) if /\S/;
  }

=head1 DESCRIPTION

This package is just a front end to some other packages. At the moment
this description is written, the only such package is Term-ReadLine,
available on CPAN near you. The real target of this stub package is to
set up a common interface to whatever Readline emerges with time.

=head1 Minimal set of supported functions

All the supported functions should be called as methods, i.e., either as 

  $term = new Term::ReadLine 'name';

or as 

  $term->addhistory('row');

where $term is a return value of Term::ReadLine->Init.

=over 12

=item C<ReadLine>

returns the actual package that executes the commands. Among possible
values are C<Term::ReadLine::Gnu>, C<Term::ReadLine::Perl>,
C<Term::ReadLine::Stub Exporter>.

=item C<new>

returns the handle for subsequent calls to following
functions. Argument is the name of the application. Optionally can be
followed by two arguments for C<IN> and C<OUT> filehandles. These
arguments should be globs.

=item C<readline>

gets an input line, I<possibly> with actual C<readline>
support. Trailing newline is removed. Returns C<undef> on C<EOF>.

=item C<addhistory>

adds the line to the history of input, from where it can be used if
the actual C<readline> is present.

=item C<IN>, $C<OUT>

return the filehandles for input and output or C<undef> if C<readline>
input and output cannot be used for Perl.

=item C<MinLine>

If argument is specified, it is an advice on minimal size of line to
be included into history.  C<undef> means do not include anything into
history. Returns the old value.

=item C<findConsole>

returns an array with two strings that give most appropriate names for
files for input and output using conventions C<"<$in">, C<"E<gt>out">.

=item C<Features>

Returns a reference to a hash with keys being features present in
current implementation. Several optional features are used in the
minimal interface: C<appname> should be present if the first argument
to C<new> is recognized, and C<minline> should be present if
C<MinLine> method is not dummy.  C<autohistory> should be present if
lines are put into history automatically (maybe subject to
C<MinLine>), and C<addhistory> if C<addhistory> method is not dummy.

=back

Actually C<Term::ReadLine> can use some other package, that will
support reacher set of commands.

=head1 EXPORTS

None

=cut

package Term::ReadLine::Stub;

$DB::emacs = $DB::emacs;	# To peacify -w

sub ReadLine {'Term::ReadLine::Stub'}
sub readline {
  my ($in,$out,$str) = @{shift()};
  print $out shift; 
  $str = scalar <$in>;
  # bug in 5.000: chomping empty string creats length -1:
  chomp $str if defined $str;
  $str;
}
sub addhistory {}

sub findConsole {
    my $console;

    if (-e "/dev/tty") {
	$console = "/dev/tty";
    } elsif (-e "con") {
	$console = "con";
    } else {
	$console = "sys\$command";
    }

    if (defined $ENV{'OS2_SHELL'}) { # In OS/2
      if ($DB::emacs) {
	$console = undef;
      } else {
	$console = "/dev/con";
      }
    }

    $consoleOUT = $console;
    $console = "&STDIN" unless defined $console;
    if (!defined $consoleOUT) {
      $consoleOUT = defined fileno(STDERR) ? "&STDERR" : "&STDOUT";
    }
    ($console,$consoleOUT);
}

sub new {
  die "method new called with wrong number of arguments" 
    unless @_==2 or @_==4;
  #local (*FIN, *FOUT);
  my ($FIN, $FOUT);
  if (@_==2) {
    ($console, $consoleOUT) = findConsole;

    open(FIN, "<$console"); 
    open(FOUT,">$consoleOUT");
    #OUT->autoflush(1);		# Conflicts with debugger?
    $sel = select(FOUT);
    $| = 1;				# for DB::OUT
    select($sel);
    bless [\*FIN, \*FOUT];
  } else {			# Filehandles supplied
    $FIN = $_[2]; $FOUT = $_[3];
    #OUT->autoflush(1);		# Conflicts with debugger?
    $sel = select($FOUT);
    $| = 1;				# for DB::OUT
    select($sel);
    bless [$FIN, $FOUT];
  }
}
sub IN { shift->[0] }
sub OUT { shift->[1] }
sub MinLine { undef }
sub Features { {} }

package Term::ReadLine;		# So late to allow the above code be defined?
eval "use Term::ReadLine::Gnu;" or eval "use Term::ReadLine::Perl;";

#require FileHandle;

# To make possible switch off RL in debugger: (Not needed, work done
# in debugger).

if (defined &Term::ReadLine::Gnu::readline) {
  @ISA = qw(Term::ReadLine::Gnu Term::ReadLine::Stub);
} elsif (defined &Term::ReadLine::Perl::readline) {
  @ISA = qw(Term::ReadLine::Perl Term::ReadLine::Stub);
} else {
  @ISA = qw(Term::ReadLine::Stub);
}


1;

