# -*- Mode: cperl; cperl-indent-level: 4 -*-
# $Id: Straps.pm,v 1.13 2002/06/19 21:01:04 schwern Exp $

package Test::Harness::Straps;

use strict;
use vars qw($VERSION);
use Config;
$VERSION = '0.14';

use Test::Harness::Assert;
use Test::Harness::Iterator;

# Flags used as return values from our methods.  Just for internal 
# clarification.
my $TRUE  = (1==1);
my $FALSE = !$TRUE;
my $YES   = $TRUE;
my $NO    = $FALSE;


=head1 NAME

Test::Harness::Straps - detailed analysis of test results

=head1 SYNOPSIS

  use Test::Harness::Straps;

  my $strap = Test::Harness::Straps->new;

  # Various ways to interpret a test
  my %results = $strap->analyze($name, \@test_output);
  my %results = $strap->analyze_fh($name, $test_filehandle);
  my %results = $strap->analyze_file($test_file);

  # UNIMPLEMENTED
  my %total = $strap->total_results;

  # Altering the behavior of the strap  UNIMPLEMENTED
  my $verbose_output = $strap->dump_verbose();
  $strap->dump_verbose_fh($output_filehandle);


=head1 DESCRIPTION

B<THIS IS ALPHA SOFTWARE> in that the interface is subject to change
in incompatible ways.  It is otherwise stable.

Test::Harness is limited to printing out its results.  This makes
analysis of the test results difficult for anything but a human.  To
make it easier for programs to work with test results, we provide
Test::Harness::Straps.  Instead of printing the results, straps
provide them as raw data.  You can also configure how the tests are to
be run.

The interface is currently incomplete.  I<Please> contact the author
if you'd like a feature added or something change or just have
comments.

=head2 Construction

=over 4

=item B<new>

  my $strap = Test::Harness::Straps->new;

Initialize a new strap.

=cut

sub new {
    my($proto) = shift;
    my($class) = ref $proto || $proto;

    my $self = bless {}, $class;
    $self->_init;

    return $self;
}

=begin _private

=item B<_init>

  $strap->_init;

Initialize the internal state of a strap to make it ready for parsing.

=cut

sub _init {
    my($self) = shift;

    $self->{_is_vms} = $^O eq 'VMS';
}

=end _private

=back

=head2 Analysis

=over 4

=item B<analyze>

  my %results = $strap->analyze($name, \@test_output);

Analyzes the output of a single test, assigning it the given $name for
use in the total report.  Returns the %results of the test.  See
L<Results>.

@test_output should be the raw output from the test, including newlines.

=cut

sub analyze {
    my($self, $name, $test_output) = @_;

    my $it = Test::Harness::Iterator->new($test_output);
    return $self->_analyze_iterator($name, $it);
}


sub _analyze_iterator {
    my($self, $name, $it) = @_;

    $self->_reset_file_state;
    $self->{file} = $name;
    my %totals  = (
                   max      => 0,
                   seen     => 0,

                   ok       => 0,
                   todo     => 0,
                   skip     => 0,
                   bonus    => 0,

                   details  => []
                  );

    # Set them up here so callbacks can have them.
    $self->{totals}{$name}         = \%totals;
    while( defined(my $line = $it->next) ) {
        $self->_analyze_line($line, \%totals);
        last if $self->{saw_bailout};
    }

    $totals{skip_all} = $self->{skip_all} if defined $self->{skip_all};

    my $passed = !$totals{max} ||
                  ($totals{max} && $totals{seen} &&
                   $totals{max} == $totals{seen} && 
                   $totals{max} == $totals{ok});
    $totals{passing} = $passed ? 1 : 0;

    return %totals;
}


sub _analyze_line {
    my($self, $line, $totals) = @_;

    my %result = ();

    $self->{line}++;

    my $type;
    if( $self->_is_header($line) ) {
        $type = 'header';

        $self->{saw_header}++;

        $totals->{max} += $self->{max};
    }
    elsif( $self->_is_test($line, \%result) ) {
        $type = 'test';

        $totals->{seen}++;
        $result{number} = $self->{'next'} unless $result{number};

        # sometimes the 'not ' and the 'ok' are on different lines,
        # happens often on VMS if you do:
        #   print "not " unless $test;
        #   print "ok $num\n";
        if( $self->{saw_lone_not} && 
            ($self->{lone_not_line} == $self->{line} - 1) ) 
        {
            $result{ok} = 0;
        }

        my $pass = $result{ok};
        $result{type} = 'todo' if $self->{todo}{$result{number}};

        if( $result{type} eq 'todo' ) {
            $totals->{todo}++;
            $pass = 1;
            $totals->{bonus}++ if $result{ok}
        }
        elsif( $result{type} eq 'skip' ) {
            $totals->{skip}++;
            $pass = 1;
        }

        $totals->{ok}++ if $pass;

        if( $result{number} > 100000 ) {
            warn "Enormous test number seen [test $result{number}]\n";
            warn "Can't detailize, too big.\n";
        }
        else {
            $totals->{details}[$result{number} - 1] = 
                               {$self->_detailize($pass, \%result)};
        }

        # XXX handle counter mismatch
    }
    elsif ( $self->_is_bail_out($line, \$self->{bailout_reason}) ) {
        $type = 'bailout';
        $self->{saw_bailout} = 1;
    }
    else {
        $type = 'other';
    }

    $self->{callback}->($self, $line, $type, $totals) if $self->{callback};

    $self->{'next'} = $result{number} + 1 if $type eq 'test';
}

=item B<analyze_fh>

  my %results = $strap->analyze_fh($name, $test_filehandle);

Like C<analyze>, but it reads from the given filehandle.

=cut

sub analyze_fh {
    my($self, $name, $fh) = @_;

    my $it = Test::Harness::Iterator->new($fh);
    $self->_analyze_iterator($name, $it);
}

=item B<analyze_file>

  my %results = $strap->analyze_file($test_file);

Like C<analyze>, but it runs the given $test_file and parses it's
results.  It will also use that name for the total report.

=cut

sub analyze_file {
    my($self, $file) = @_;

    unless( -e $file ) {
        $self->{error} = "$file does not exist";
        return;
    }

    unless( -r $file ) {
        $self->{error} = "$file is not readable";
        return;
    }

    local $ENV{PERL5LIB} = $self->_INC2PERL5LIB;

    # Is this necessary anymore?
    my $cmd = $self->{_is_vms} ? "MCR $^X" : $^X;

    my $switches = $self->_switches($file);

    # *sigh* this breaks under taint, but open -| is unportable.
    unless( open(FILE, "$cmd $switches $file|") ) {
        print "can't run $file. $!\n";
        return;
    }

    my %results = $self->analyze_fh($file, \*FILE);
    my $exit = close FILE;
    $results{'wait'} = $?;
    if( $? && $self->{_is_vms} ) {
        eval q{use vmsish "status"; $results{'exit'} = $?};
    }
    else {
        $results{'exit'} = _wait2exit($?);
    }
    $results{passing} = 0 unless $? == 0;

    $self->_restore_PERL5LIB();

    return %results;
}


eval { require POSIX; &POSIX::WEXITSTATUS(0) };
if( $@ ) {
    *_wait2exit = sub { $_[0] >> 8 };
}
else {
    *_wait2exit = sub { POSIX::WEXITSTATUS($_[0]) }
}


=begin _private

=item B<_switches>

  my $switches = $self->_switches($file);

Formats and returns the switches necessary to run the test.

=cut

sub _switches {
    my($self, $file) = @_;

    local *TEST;
    open(TEST, $file) or print "can't open $file. $!\n";
    my $first = <TEST>;
    my $s = '';
    $s .= " $ENV{'HARNESS_PERL_SWITCHES'}"
      if exists $ENV{'HARNESS_PERL_SWITCHES'};

    if ($first =~ /^#!.*\bperl.*\s-\w*([Tt]+)/) {
        # When taint mode is on, PERL5LIB is ignored.  So we need to put
        # all that on the command line as -Is.
        $s .= join " ", qq[ "-$1"], map {qq["-I$_"]} $self->_filtered_INC;
    }
    elsif ($^O eq 'MacOS') {
        # MacPerl's putenv is broken, so it will not see PERL5LIB.
        $s .= join " ", map {qq["-I$_"]} $self->_filtered_INC;
    }

    close(TEST) or print "can't close $file. $!\n";

    return $s;
}


=item B<_INC2PERL5LIB>

  local $ENV{PERL5LIB} = $self->_INC2PERL5LIB;

Takes the current value of @INC and turns it into something suitable
for putting onto PERL5LIB.

=cut

sub _INC2PERL5LIB {
    my($self) = shift;

    $self->{_old5lib} = $ENV{PERL5LIB};

    return join $Config{path_sep}, $self->_filtered_INC;
}

=item B<_filtered_INC>

  my @filtered_inc = $self->_filtered_INC;

Shortens @INC by removing redundant and unnecessary entries.
Necessary for OS's with limited command line lengths, like VMS.

=cut

sub _filtered_INC {
    my($self, @inc) = @_;
    @inc = @INC unless @inc;

    # VMS has a 255-byte limit on the length of %ENV entries, so
    # toss the ones that involve perl_root, the install location
    # for VMS
    if( $self->{_is_vms} ) {
        @inc = grep !/perl_root/i, @inc;
    }

    return @inc;
}


=item B<_restore_PERL5LIB>

  $self->_restore_PERL5LIB;

This restores the original value of the PERL5LIB environment variable.
Necessary on VMS, otherwise a no-op.

=cut

sub _restore_PERL5LIB {
    my($self) = shift;

    return unless $self->{_is_vms};

    if (defined $self->{_old5lib}) {
        $ENV{PERL5LIB} = $self->{_old5lib};
    }
}


=end _private

=back


=begin _private

=head2 Parsing

Methods for identifying what sort of line you're looking at.

=over 4

=item B<_is_comment>

  my $is_comment = $strap->_is_comment($line, \$comment);

Checks if the given line is a comment.  If so, it will place it into
$comment (sans #).

=cut

sub _is_comment {
    my($self, $line, $comment) = @_;

    if( $line =~ /^\s*\#(.*)/ ) {
        $$comment = $1;
        return $YES;
    }
    else {
        return $NO;
    }
}

=item B<_is_header>

  my $is_header = $strap->_is_header($line);

Checks if the given line is a header (1..M) line.  If so, it places
how many tests there will be in $strap->{max}, a list of which tests
are todo in $strap->{todo} and if the whole test was skipped
$strap->{skip_all} contains the reason.

=cut

# Regex for parsing a header.  Will be run with /x
my $Extra_Header_Re = <<'REGEX';
                       ^
                        (?: \s+ todo \s+ ([\d \t]+) )?      # optional todo set
                        (?: \s* \# \s* ([\w:]+\s?) (.*) )?     # optional skip with optional reason
REGEX

sub _is_header {
    my($self, $line) = @_;

    if( my($max, $extra) = $line =~ /^1\.\.(\d+)(.*)/ ) {
        $self->{max}  = $max;
        assert( $self->{max} >= 0,  'Max # of tests looks right' );

        if( defined $extra ) {
            my($todo, $skip, $reason) = $extra =~ /$Extra_Header_Re/xo;

            $self->{todo} = { map { $_ => 1 } split /\s+/, $todo } if $todo;

            $self->{skip_all} = $reason if defined $skip and $skip =~ /^Skip/i;
        }

        return $YES;
    }
    else {
        return $NO;
    }
}

=item B<_is_test>

  my $is_test = $strap->_is_test($line, \%test);

Checks if the $line is a test report (ie. 'ok/not ok').  Reports the
result back in %test which will contain:

  ok            did it succeed?  This is the literal 'ok' or 'not ok'.
  name          name of the test (if any)
  number        test number (if any)

  type          'todo' or 'skip' (if any)
  reason        why is it todo or skip? (if any)

If will also catch lone 'not' lines, note it saw them 
$strap->{saw_lone_not} and the line in $strap->{lone_not_line}.

=cut

my $Report_Re = <<'REGEX';
                 ^
                  (not\ )?               # failure?
                  ok\b
                  (?:\s+(\d+))?         # optional test number
                  \s*
                  (.*)                  # and the rest
REGEX

my $Extra_Re = <<'REGEX';
                 ^
                  (.*?) (?:(?:[^\\]|^)# (.*))?
                 $
REGEX

sub _is_test {
    my($self, $line, $test) = @_;

    # We pulverize the line down into pieces in three parts.
    if( my($not, $num, $extra)    = $line  =~ /$Report_Re/ox ) {
        my($name, $control) = split /(?:[^\\]|^)#/, $extra if $extra;
        my($type, $reason)  = $control =~ /^\s*(\S+)(?:\s+(.*))?$/ if $control;

        $test->{number} = $num;
        $test->{ok}     = $not ? 0 : 1;
        $test->{name}   = $name;

        if( defined $type ) {
            $test->{type}   = $type =~ /^TODO$/i ? 'todo' :
                              $type =~ /^Skip/i  ? 'skip' : 0;
        }
        else {
            $test->{type} = '';
        }
        $test->{reason} = $reason;

        return $YES;
    }
    else{
        # Sometimes the "not " and "ok" will be on seperate lines on VMS.
        # We catch this and remember we saw it.
        if( $line =~ /^not\s+$/ ) {
            $self->{saw_lone_not} = 1;
            $self->{lone_not_line} = $self->{line};
        }

        return $NO;
    }
}

=item B<_is_bail_out>

  my $is_bail_out = $strap->_is_bail_out($line, \$reason);

Checks if the line is a "Bail out!".  Places the reason for bailing
(if any) in $reason.

=cut

sub _is_bail_out {
    my($self, $line, $reason) = @_;

    if( $line =~ /^Bail out!\s*(.*)/i ) {
        $$reason = $1 if $1;
        return $YES;
    }
    else {
        return $NO;
    }
}

=item B<_reset_file_state>

  $strap->_reset_file_state;

Resets things like $strap->{max}, $strap->{skip_all}, etc... so its
ready to parse the next file.

=cut

sub _reset_file_state {
    my($self) = shift;

    delete @{$self}{qw(max skip_all todo)};
    $self->{line}       = 0;
    $self->{saw_header} = 0;
    $self->{saw_bailout}= 0;
    $self->{saw_lone_not} = 0;
    $self->{lone_not_line} = 0;
    $self->{bailout_reason} = '';
    $self->{'next'}       = 1;
}

=back

=end _private


=head2 Results

The %results returned from analyze() contain the following information:

  passing           true if the whole test is considered a pass 
                    (or skipped), false if its a failure

  exit              the exit code of the test run, if from a file
  wait              the wait code of the test run, if from a file

  max               total tests which should have been run
  seen              total tests actually seen
  skip_all          if the whole test was skipped, this will 
                      contain the reason.

  ok                number of tests which passed 
                      (including todo and skips)

  todo              number of todo tests seen
  bonus             number of todo tests which 
                      unexpectedly passed

  skip              number of tests skipped

So a successful test should have max == seen == ok.


There is one final item, the details.

  details           an array ref reporting the result of 
                    each test looks like this:

    $results{details}[$test_num - 1] = 
            { ok        => is the test considered ok?
              actual_ok => did it literally say 'ok'?
              name      => name of the test (if any)
              type      => 'skip' or 'todo' (if any)
              reason    => reason for the above (if any)
            };

Element 0 of the details is test #1.  I tried it with element 1 being
#1 and 0 being empty, this is less awkward.

=begin _private

=over 4

=item B<_detailize>

  my %details = $strap->_detailize($pass, \%test);

Generates the details based on the last test line seen.  $pass is true
if it was considered to be a passed test.  %test is the results of the
test you're summarizing.

=cut

sub _detailize {
    my($self, $pass, $test) = @_;

    my %details = ( ok         => $pass,
                    actual_ok  => $test->{ok}
                  );

    assert( !(grep !defined $details{$_}, keys %details),
            'test contains the ok and actual_ok info' );

    # We don't want these to be undef because they are often
    # checked and don't want the checker to have to deal with
    # uninitialized vars.
    foreach my $piece (qw(name type reason)) {
        $details{$piece} = defined $test->{$piece} ? $test->{$piece} : '';
    }

    return %details;
}

=back

=end _private

=head1 EXAMPLES

See F<examples/mini_harness.plx> for an example of use.

=head1 AUTHOR

Michael G Schwern E<lt>schwern@pobox.comE<gt>

=head1 SEE ALSO

L<Test::Harness>

=cut


1;
