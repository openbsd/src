
require 5.004;
package Test;
# Time-stamp: "2003-04-18 21:48:01 AHDT"

use strict;

use Carp;
use vars (qw($VERSION @ISA @EXPORT @EXPORT_OK $ntest $TestLevel), #public-ish
          qw($TESTOUT $TESTERR %Program_Lines
             $ONFAIL %todo %history $planned @FAILDETAIL) #private-ish
         );

# In case a test is run in a persistent environment.
sub _reset_globals {
    %todo       = ();
    %history    = ();
    @FAILDETAIL = ();
    $ntest      = 1;
    $TestLevel  = 0;		# how many extra stack frames to skip
    $planned    = 0;
}

$VERSION = '1.24';
require Exporter;
@ISA=('Exporter');

@EXPORT    = qw(&plan &ok &skip);
@EXPORT_OK = qw($ntest $TESTOUT $TESTERR);

$|=1;
$TESTOUT = *STDOUT{IO};
$TESTERR = *STDERR{IO};

# Use of this variable is strongly discouraged.  It is set mainly to
# help test coverage analyzers know which test is running.
$ENV{REGRESSION_TEST} = $0;


=head1 NAME

Test - provides a simple framework for writing test scripts

=head1 SYNOPSIS

  use strict;
  use Test;

  # use a BEGIN block so we print our plan before MyModule is loaded
  BEGIN { plan tests => 14, todo => [3,4] }

  # load your module...
  use MyModule;

  # Helpful notes.  All note-lines must start with a "#".
  print "# I'm testing MyModule version $MyModule::VERSION\n";

  ok(0); # failure
  ok(1); # success

  ok(0); # ok, expected failure (see todo list, above)
  ok(1); # surprise success!

  ok(0,1);             # failure: '0' ne '1'
  ok('broke','fixed'); # failure: 'broke' ne 'fixed'
  ok('fixed','fixed'); # success: 'fixed' eq 'fixed'
  ok('fixed',qr/x/);   # success: 'fixed' =~ qr/x/

  ok(sub { 1+1 }, 2);  # success: '2' eq '2'
  ok(sub { 1+1 }, 3);  # failure: '2' ne '3'

  my @list = (0,0);
  ok @list, 3, "\@list=".join(',',@list);      #extra notes
  ok 'segmentation fault', '/(?i)success/';    #regex match

  skip(
    $^O eq 'MSWin' ? "Skip unless MSWin" : 0,  # whether to skip
    $foo, $bar  # arguments just like for ok(...)
  );
  skip(
    $^O eq 'MSWin' ? 0 : "Skip if MSWin",  # whether to skip
    $foo, $bar  # arguments just like for ok(...)
  );

=head1 DESCRIPTION

This module simplifies the task of writing test files for Perl modules,
such that their output is in the format that
L<Test::Harness|Test::Harness> expects to see.

=head1 QUICK START GUIDE

To write a test for your new (and probably not even done) module, create
a new file called F<t/test.t> (in a new F<t> directory). If you have
multiple test files, to test the "foo", "bar", and "baz" feature sets,
then feel free to call your files F<t/foo.t>, F<t/bar.t>, and
F<t/baz.t>

=head2 Functions

This module defines three public functions, C<plan(...)>, C<ok(...)>,
and C<skip(...)>.  By default, all three are exported by
the C<use Test;> statement.

=over 4

=item C<plan(...)>

     BEGIN { plan %theplan; }

This should be the first thing you call in your test script.  It
declares your testing plan, how many there will be, if any of them
should be allowed to fail, and so on.

Typical usage is just:

     use Test;
     BEGIN { plan tests => 23 }

These are the things that you can put in the parameters to plan:

=over

=item C<tests =E<gt> I<number>>

The number of tests in your script.
This means all ok() and skip() calls.

=item C<todo =E<gt> [I<1,5,14>]>

A reference to a list of tests which are allowed to fail.
See L</TODO TESTS>.

=item C<onfail =E<gt> sub { ... }>

=item C<onfail =E<gt> \&some_sub>

A subroutine reference to be run at the end of the test script, if
any of the tests fail.  See L</ONFAIL>.

=back

You must call C<plan(...)> once and only once.  You should call it
in a C<BEGIN {...}> block, like so:

     BEGIN { plan tests => 23 }

=cut

sub plan {
    croak "Test::plan(%args): odd number of arguments" if @_ & 1;
    croak "Test::plan(): should not be called more than once" if $planned;

    local($\, $,);   # guard against -l and other things that screw with
                     # print

    _reset_globals();

    _read_program( (caller)[1] );

    my $max=0;
    for (my $x=0; $x < @_; $x+=2) {
	my ($k,$v) = @_[$x,$x+1];
	if ($k =~ /^test(s)?$/) { $max = $v; }
	elsif ($k eq 'todo' or 
	       $k eq 'failok') { for (@$v) { $todo{$_}=1; }; }
	elsif ($k eq 'onfail') { 
	    ref $v eq 'CODE' or croak "Test::plan(onfail => $v): must be CODE";
	    $ONFAIL = $v; 
	}
	else { carp "Test::plan(): skipping unrecognized directive '$k'" }
    }
    my @todo = sort { $a <=> $b } keys %todo;
    if (@todo) {
	print $TESTOUT "1..$max todo ".join(' ', @todo).";\n";
    } else {
	print $TESTOUT "1..$max\n";
    }
    ++$planned;
    print $TESTOUT "# Running under perl version $] for $^O",
      (chr(65) eq 'A') ? "\n" : " in a non-ASCII world\n";

    print $TESTOUT "# Win32::BuildNumber ", &Win32::BuildNumber(), "\n"
      if defined(&Win32::BuildNumber) and defined &Win32::BuildNumber();

    print $TESTOUT "# MacPerl version $MacPerl::Version\n"
      if defined $MacPerl::Version;

    printf $TESTOUT
      "# Current time local: %s\n# Current time GMT:   %s\n",
      scalar(localtime($^T)), scalar(gmtime($^T));
      
    print $TESTOUT "# Using Test.pm version $VERSION\n";

    # Retval never used:
    return undef;
}

sub _read_program {
  my($file) = shift;
  return unless defined $file and length $file
    and -e $file and -f _ and -r _;
  open(SOURCEFILE, "<$file") || return;
  $Program_Lines{$file} = [<SOURCEFILE>];
  close(SOURCEFILE);
  
  foreach my $x (@{$Program_Lines{$file}})
   { $x =~ tr/\cm\cj\n\r//d }
  
  unshift @{$Program_Lines{$file}}, '';
  return 1;
}

=begin _private

=item B<_to_value>

  my $value = _to_value($input);

Converts an C<ok> parameter to its value.  Typically this just means
running it, if it's a code reference.  You should run all inputted 
values through this.

=cut

sub _to_value {
    my ($v) = @_;
    return (ref $v or '') eq 'CODE' ? $v->() : $v;
}

=end _private

=item C<ok(...)>

  ok(1 + 1 == 2);
  ok($have, $expect);
  ok($have, $expect, $diagnostics);

This function is the reason for C<Test>'s existence.  It's
the basic function that
handles printing "C<ok>" or "C<not ok>", along with the
current test number.  (That's what C<Test::Harness> wants to see.)

In its most basic usage, C<ok(...)> simply takes a single scalar
expression.  If its value is true, the test passes; if false,
the test fails.  Examples:

    # Examples of ok(scalar)

    ok( 1 + 1 == 2 );           # ok if 1 + 1 == 2
    ok( $foo =~ /bar/ );        # ok if $foo contains 'bar'
    ok( baz($x + $y) eq 'Armondo' );    # ok if baz($x + $y) returns
                                        # 'Armondo'
    ok( @a == @b );             # ok if @a and @b are the same length

The expression is evaluated in scalar context.  So the following will
work:

    ok( @stuff );                       # ok if @stuff has any elements
    ok( !grep !defined $_, @stuff );    # ok if everything in @stuff is
                                        # defined.

A special case is if the expression is a subroutine reference (in either
C<sub {...}> syntax or C<\&foo> syntax).  In
that case, it is executed and its value (true or false) determines if
the test passes or fails.  For example,

    ok( sub {   # See whether sleep works at least passably
      my $start_time = time;
      sleep 5;
      time() - $start_time  >= 4
    });

In its two-argument form, C<ok(I<arg1>,I<arg2>)> compares the two scalar
values to see if they equal.  (The equality is checked with C<eq>).

    # Example of ok(scalar, scalar)

    ok( "this", "that" );               # not ok, 'this' ne 'that'

If either (or both!) is a subroutine reference, it is run and used
as the value for comparing.  For example:

    ok 4, sub {
        open(OUT, ">x.dat") || die $!;
        print OUT "\x{e000}";
        close OUT;
        my $bytecount = -s 'x.dat';
        unlink 'x.dat' or warn "Can't unlink : $!";
        return $bytecount;
      },
    ;

The above test passes two values to C<ok(arg1, arg2)> -- the first is
the number 4, and the second is a coderef. Before C<ok> compares them,
it calls the coderef, and uses its return value as the real value of
this parameter. Assuming that C<$bytecount> returns 4, C<ok> ends up
testing C<4 eq 4>. Since that's true, this test passes.

If C<arg2> is either a regex object (i.e., C<qr/.../>) or a string
that I<looks like> a regex (e.g., C<'/foo/'>), then
C<ok(I<arg1>,I<arg2>)> will perform a pattern
match against it, instead of using C<eq>.

    ok( 'JaffO', '/Jaff/' );    # ok, 'JaffO' =~ /Jaff/
    ok( 'JaffO', qr/Jaff/ );    # ok, 'JaffO' =~ qr/Jaff/;
    ok( 'JaffO', '/(?i)jaff/ ); # ok, 'JaffO' =~ /jaff/i;

Finally, you can append an optional third argument, in 
C<ok(I<arg1>,I<arg2>, I<note>)>, where I<note> is a string value that
will be printed if the test fails.  This should be some useful
information about the test, pertaining to why it failed, and/or
a description of the test.  For example:

    ok( grep($_ eq 'something unique', @stuff), 1,
        "Something that should be unique isn't!\n".
        '@stuff = '.join ', ', @stuff
      );

Unfortunately, a note cannot be used with the single argument
style of C<ok()>.  That is, if you try C<ok(I<arg1>, I<note>)>, then
C<Test> will interpret this as C<ok(I<arg1>, I<arg2>)>, and probably
end up testing C<I<arg1> eq I<arg2>> -- and that's not what you want!

All of the above special cases can occasionally cause some
problems.  See L</BUGS and CAVEATS>.

=cut

# A past maintainer of this module said:
# <<ok(...)'s special handling of subroutine references is an unfortunate
#   "feature" that can't be removed due to compatibility.>>
#

sub ok ($;$$) {
    croak "ok: plan before you test!" if !$planned;

    local($\,$,);   # guard against -l and other things that screw with
                    # print

    my ($pkg,$file,$line) = caller($TestLevel);
    my $repetition = ++$history{"$file:$line"};
    my $context = ("$file at line $line".
		   ($repetition > 1 ? " fail \#$repetition" : ''));

    # Are we comparing two values?
    my $compare = 0;

    my $ok=0;
    my $result = _to_value(shift);
    my ($expected,$diag,$isregex,$regex);
    if (@_ == 0) {
	$ok = $result;
    } else {
        $compare = 1;
	$expected = _to_value(shift);
	if (!defined $expected) {
	    $ok = !defined $result;
	} elsif (!defined $result) {
	    $ok = 0;
	} elsif ((ref($expected)||'') eq 'Regexp') {
	    $ok = $result =~ /$expected/;
            $regex = $expected;
	} elsif (($regex) = ($expected =~ m,^ / (.+) / $,sx) or
	    (undef, $regex) = ($expected =~ m,^ m([^\w\s]) (.+) \1 $,sx)) {
	    $ok = $result =~ /$regex/;
	} else {
	    $ok = $result eq $expected;
	}
    }
    my $todo = $todo{$ntest};
    if ($todo and $ok) {
	$context .= ' TODO?!' if $todo;
	print $TESTOUT "ok $ntest # ($context)\n";
    } else {
        # Issuing two seperate prints() causes problems on VMS.
        if (!$ok) {
            print $TESTOUT "not ok $ntest\n";
        }
	else {
            print $TESTOUT "ok $ntest\n";
        }
	
	if (!$ok) {
	    my $detail = { 'repetition' => $repetition, 'package' => $pkg,
			   'result' => $result, 'todo' => $todo };
	    $$detail{expected} = $expected if defined $expected;

            # Get the user's diagnostic, protecting against multi-line
            # diagnostics.
	    $diag = $$detail{diagnostic} = _to_value(shift) if @_;
            $diag =~ s/\n/\n#/g if defined $diag;

	    $context .= ' *TODO*' if $todo;
	    if (!$compare) {
		if (!$diag) {
		    print $TESTERR "# Failed test $ntest in $context\n";
		} else {
		    print $TESTERR "# Failed test $ntest in $context: $diag\n";
		}
	    } else {
		my $prefix = "Test $ntest";
		print $TESTERR "# $prefix got: ".
		    (defined $result? "'$result'":'<UNDEF>')." ($context)\n";
		$prefix = ' ' x (length($prefix) - 5);
		if (defined $regex) {
		    $expected = 'qr{'.$regex.'}';
		}
                elsif (defined $expected) {
		    $expected = "'$expected'";
		}
                else {
                    $expected = '<UNDEF>';
                }
		if (!$diag) {
		    print $TESTERR "# $prefix Expected: $expected\n";
		} else {
		    print $TESTERR "# $prefix Expected: $expected ($diag)\n";
		}
	    }

            if(defined $Program_Lines{$file}[$line]) {
                print $TESTERR
                  "#  $file line $line is: $Program_Lines{$file}[$line]\n"
                 if
                  $Program_Lines{$file}[$line] =~ m/[^\s\#\(\)\{\}\[\]\;]/
                   # Otherwise it's a pretty uninteresting line!
                ;
                
                undef $Program_Lines{$file}[$line];
                 # So we won't repeat it.
            }

	    push @FAILDETAIL, $detail;
	}
    }
    ++ $ntest;
    $ok;
}

=item C<skip(I<skip_if_true>, I<args...>)>

This is used for tests that under some conditions can be skipped.  It's
basically equivalent to:

  if( $skip_if_true ) {
    ok(1);
  } else {
    ok( args... );
  }

...except that the C<ok(1)> emits not just "C<ok I<testnum>>" but
actually "C<ok I<testnum> # I<skip_if_true_value>>".

The arguments after the I<skip_if_true> are what is fed to C<ok(...)> if
this test isn't skipped.

Example usage:

  my $if_MSWin =
    $^O eq 'MSWin' ? 'Skip if under MSWin' : '';

  # A test to be run EXCEPT under MSWin:
  skip($if_MSWin, thing($foo), thing($bar) );

Or, going the other way:  

  my $unless_MSWin =
    $^O eq 'MSWin' ? 'Skip unless under MSWin' : '';

  # A test to be run EXCEPT under MSWin:
  skip($unless_MSWin, thing($foo), thing($bar) );

The tricky thing to remember is that the first parameter is true if
you want to I<skip> the test, not I<run> it; and it also doubles as a
note about why it's being skipped. So in the first codeblock above, read
the code as "skip if MSWin -- (otherwise) test whether C<thing($foo)> is
C<thing($bar)>" or for the second case, "skip unless MSWin...".

Also, when your I<skip_if_reason> string is true, it really should (for
backwards compatibility with older Test.pm versions) start with the
string "Skip", as shown in the above examples.

Note that in the above cases, C<thing($foo)> and C<thing($bar)>
I<are> evaluated -- but as long as the C<skip_if_true> is true,
then we C<skip(...)> just tosses out their value (i.e., not
bothering to treat them like values to C<ok(...)>.  But if
you need to I<not> eval the arguments when skipping the
test, use
this format:

  skip( $unless_MSWin,
    sub {
      # This code returns true if the test passes.
      # (But it doesn't even get called if the test is skipped.)
      thing($foo) eq thing($bar)
    }
  );

or even this, which is basically equivalent:

  skip( $unless_MSWin,
    sub { thing($foo) }, sub { thing($bar) }
  );

That is, both are like this:

  if( $unless_MSWin ) {
    ok(1);  # but it actually appends "# $unless_MSWin"
            #  so that Test::Harness can tell it's a skip
  } else {
    # Not skipping, so actually call and evaluate...
    ok( sub { thing($foo) }, sub { thing($bar) } );
  }

=cut

sub skip ($;$$$) {
    local($\, $,);   # guard against -l and other things that screw with
                     # print

    my $whyskip = _to_value(shift);
    if (!@_ or $whyskip) {
	$whyskip = '' if $whyskip =~ m/^\d+$/;
        $whyskip =~ s/^[Ss]kip(?:\s+|$)//;  # backwards compatibility, old
                                            # versions required the reason
                                            # to start with 'skip'
        # We print in one shot for VMSy reasons.
        my $ok = "ok $ntest # skip";
        $ok .= " $whyskip" if length $whyskip;
        $ok .= "\n";
        print $TESTOUT $ok;
        ++ $ntest;
        return 1;
    } else {
        # backwards compatiblity (I think).  skip() used to be
        # called like ok(), which is weird.  I haven't decided what to do with
        # this yet.
#        warn <<WARN if $^W;
#This looks like a skip() using the very old interface.  Please upgrade to
#the documented interface as this has been deprecated.
#WARN

	local($TestLevel) = $TestLevel+1;  #to ignore this stack frame
        return &ok(@_);
    }
}

=back

=cut

END {
    $ONFAIL->(\@FAILDETAIL) if @FAILDETAIL && $ONFAIL;
}

1;
__END__

=head1 TEST TYPES

=over 4

=item * NORMAL TESTS

These tests are expected to succeed.  Usually, most or all of your tests
are in this category.  If a normal test doesn't succeed, then that
means that something is I<wrong>.  

=item * SKIPPED TESTS

The C<skip(...)> function is for tests that might or might not be
possible to run, depending
on the availability of platform-specific features.  The first argument
should evaluate to true (think "yes, please skip") if the required
feature is I<not> available.  After the first argument, C<skip(...)> works
exactly the same way as C<ok(...)> does.

=item * TODO TESTS

TODO tests are designed for maintaining an B<executable TODO list>.
These tests are I<expected to fail.>  If a TODO test does succeed,
then the feature in question shouldn't be on the TODO list, now
should it?

Packages should NOT be released with succeeding TODO tests.  As soon
as a TODO test starts working, it should be promoted to a normal test,
and the newly working feature should be documented in the release
notes or in the change log.

=back

=head1 ONFAIL

  BEGIN { plan test => 4, onfail => sub { warn "CALL 911!" } }

Although test failures should be enough, extra diagnostics can be
triggered at the end of a test run.  C<onfail> is passed an array ref
of hash refs that describe each test failure.  Each hash will contain
at least the following fields: C<package>, C<repetition>, and
C<result>.  (The file, line, and test number are not included because
their correspondence to a particular test is tenuous.)  If the test
had an expected value or a diagnostic (or "note") string, these will also be
included.

The I<optional> C<onfail> hook might be used simply to print out the
version of your package and/or how to report problems.  It might also
be used to generate extremely sophisticated diagnostics for a
particularly bizarre test failure.  However it's not a panacea.  Core
dumps or other unrecoverable errors prevent the C<onfail> hook from
running.  (It is run inside an C<END> block.)  Besides, C<onfail> is
probably over-kill in most cases.  (Your test code should be simpler
than the code it is testing, yes?)


=head1 BUGS and CAVEATS

=over

=item *

C<ok(...)>'s special handing of strings which look like they might be
regexes can also cause unexpected behavior.  An innocent:

    ok( $fileglob, '/path/to/some/*stuff/' );

will fail, since Test.pm considers the second argument to be a regex!
The best bet is to use the one-argument form:

    ok( $fileglob eq '/path/to/some/*stuff/' );

=item *

C<ok(...)>'s use of string C<eq> can sometimes cause odd problems
when comparing
numbers, especially if you're casting a string to a number:

    $foo = "1.0";
    ok( $foo, 1 );      # not ok, "1.0" ne 1

Your best bet is to use the single argument form:

    ok( $foo == 1 );    # ok "1.0" == 1

=item *

As you may have inferred from the above documentation and examples,
C<ok>'s prototype is C<($;$$)> (and, incidentally, C<skip>'s is
C<($;$$$)>). This means, for example, that you can do C<ok @foo, @bar>
to compare the I<size> of the two arrays. But don't be fooled into
thinking that C<ok @foo, @bar> means a comparison of the contents of two
arrays -- you're comparing I<just> the number of elements of each. It's
so easy to make that mistake in reading C<ok @foo, @bar> that you might
want to be very explicit about it, and instead write C<ok scalar(@foo),
scalar(@bar)>.

=item *

This almost definitely doesn't do what you expect:

     ok $thingy->can('some_method');

Why?  Because C<can> returns a coderef to mean "yes it can (and the
method is this...)", and then C<ok> sees a coderef and thinks you're
passing a function that you want it to call and consider the truth of
the result of!  I.e., just like:

     ok $thingy->can('some_method')->();

What you probably want instead is this:

     ok $thingy->can('some_method') && 1;

If the C<can> returns false, then that is passed to C<ok>.  If it
returns true, then the larger expression S<< C<<
$thingy->can('some_method') && 1 >> >> returns 1, which C<ok> sees as
a simple signal of success, as you would expect.


=item *

The syntax for C<skip> is about the only way it can be, but it's still
quite confusing.  Just start with the above examples and you'll
be okay.

Moreover, users may expect this:

  skip $unless_mswin, foo($bar), baz($quux);

to not evaluate C<foo($bar)> and C<baz($quux)> when the test is being
skipped.  But in reality, they I<are> evaluated, but C<skip> just won't
bother comparing them if C<$unless_mswin> is true.

You could do this:

  skip $unless_mswin, sub{foo($bar)}, sub{baz($quux)};

But that's not terribly pretty.  You may find it simpler or clearer in
the long run to just do things like this:

  if( $^O =~ m/MSWin/ ) {
    print "# Yay, we're under $^O\n";
    ok foo($bar), baz($quux);
    ok thing($whatever), baz($stuff);
    ok blorp($quux, $whatever);
    ok foo($barzbarz), thang($quux);
  } else {
    print "# Feh, we're under $^O.  Watch me skip some tests...\n";
    for(1 .. 4) { skip "Skip unless under MSWin" }
  }

But be quite sure that C<ok> is called exactly as many times in the
first block as C<skip> is called in the second block.

=back

=head1 NOTE

A past developer of this module once said that it was no longer being
actively developed.  However, rumors of its demise were greatly
exaggerated.  Feedback and suggestions are quite welcome.

Be aware that the main value of this module is its simplicity.  Note
that there are already more ambitious modules out there, such as
L<Test::More> and L<Test::Unit>.


=head1 SEE ALSO

L<Test::Harness>

L<Test::Simple>, L<Test::More>, L<Devel::Cover>

L<Test::Builder> for building your own testing library.

L<Test::Unit> is an interesting XUnit-style testing library.

L<Test::Inline> and L<SelfTest> let you embed tests in code.


=head1 AUTHOR

Copyright (c) 1998-2000 Joshua Nathaniel Pritikin.  All rights reserved.

Copyright (c) 2001-2002 Michael G. Schwern.

Copyright (c) 2002-2003 Sean M. Burke.

Current maintainer: Sean M. Burke. E<lt>sburke@cpan.orgE<gt>

This package is free software and is provided "as is" without express
or implied warranty.  It may be used, redistributed and/or modified
under the same terms as Perl itself.

=cut

# "Your mistake was a hidden intention."
#  -- /Oblique Strategies/,  Brian Eno and Peter Schmidt
