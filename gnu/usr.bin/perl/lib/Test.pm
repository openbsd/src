package Test;

require 5.004;

use strict;

use Carp;
use vars (qw($VERSION @ISA @EXPORT @EXPORT_OK $ntest $TestLevel), #public-ish
          qw($TESTOUT $TESTERR
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

$VERSION = '1.20';
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
  ok(0, int(rand(2));  # (just kidding :-)

  my @list = (0,0);
  ok @list, 3, "\@list=".join(',',@list);      #extra diagnostics
  ok 'segmentation fault', '/(?i)success/';    #regex match

  skip($feature_is_missing, ...);    #do platform specific test

=head1 DESCRIPTION

B<STOP!> If you are writing a new test, we I<highly suggest> you use
the new Test::Simple and Test::More modules instead.

L<Test::Harness|Test::Harness> expects to see particular output when it
executes tests.  This module aims to make writing proper test scripts just
a little bit easier (and less error prone :-).


=head2 Functions

All the following are exported by Test by default.

=over 4

=item B<plan>

     BEGIN { plan %theplan; }

This should be the first thing you call in your test script.  It
declares your testing plan, how many there will be, if any of them
should be allowed to fail, etc...

Typical usage is just:

     use Test;
     BEGIN { plan tests => 23 }

Things you can put in the plan:

     tests          The number of tests in your script.
                    This means all ok() and skip() calls.
     todo           A reference to a list of tests which are allowed
                    to fail.  See L</TODO TESTS>.
     onfail         A subroutine reference to be run at the end of
                    the test script should any of the tests fail.
                    See L</ONFAIL>.

You must call plan() once and only once.

=cut

sub plan {
    croak "Test::plan(%args): odd number of arguments" if @_ & 1;
    croak "Test::plan(): should not be called more than once" if $planned;

    local($\, $,);   # guard against -l and other things that screw with
                     # print

    _reset_globals();

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

    # Never used.
    return undef;
}


=begin _private

=item B<_to_value>

  my $value = _to_value($input);

Converts an ok parameter to its value.  Typically this just means
running it if its a code reference.  You should run all inputed 
values through this.

=cut

sub _to_value {
    my ($v) = @_;
    return (ref $v or '') eq 'CODE' ? $v->() : $v;
}

=end _private

=item B<ok>

  ok(1 + 1 == 2);
  ok($have, $expect);
  ok($have, $expect, $diagnostics);

This is the reason for Test's existance.  Its the basic function that
handles printing "ok" or "not ok" along with the current test number.

In its most basic usage, it simply takes an expression.  If its true,
the test passes, if false, the test fails.  Simp.

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

A special case is if the expression is a subroutine reference.  In
that case, it is executed and its value (true or false) determines if
the test passes or fails.

In its two argument form it compares the two values to see if they
equal (with C<eq>).

    ok( "this", "that" );               # not ok, 'this' ne 'that'

If either is a subroutine reference, that is run and used as a
comparison.

Should $expect either be a regex reference (ie. qr//) or a string that
looks like a regex (ie. '/foo/') ok() will perform a pattern match
against it rather than using eq.

    ok( 'JaffO', '/Jaff/' );    # ok, 'JaffO' =~ /Jaff/
    ok( 'JaffO', qr/Jaff/ );    # ok, 'JaffO' =~ qr/Jaff/;
    ok( 'JaffO', '/(?i)jaff/ ); # ok, 'JaffO' =~ /jaff/i;

Finally, an optional set of $diagnostics will be printed should the
test fail.  This should usually be some useful information about the
test pertaining to why it failed or perhaps a description of the test.
Or both.

    ok( grep($_ eq 'something unique', @stuff), 1,
        "Something that should be unique isn't!\n".
        '@stuff = '.join ', ', @stuff
      );

Unfortunately, a diagnostic cannot be used with the single argument
style of ok().

All these special cases can cause some problems.  See L</BUGS and CAVEATS>.

=cut

sub ok ($;$$) {
    croak "ok: plan before you test!" if !$planned;

    local($\,$,);   # guard against -l and other things that screw with
                    # print

    my ($pkg,$file,$line) = caller($TestLevel);
    my $repetition = ++$history{"$file:$line"};
    my $context = ("$file at line $line".
		   ($repetition > 1 ? " fail \#$repetition" : ''));
    my $ok=0;
    my $result = _to_value(shift);
    my ($expected,$diag,$isregex,$regex);
    if (@_ == 0) {
	$ok = $result;
    } else {
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
	    if (!defined $expected) {
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
                else {
		    $expected = "'$expected'";
		}
		if (!$diag) {
		    print $TESTERR "# $prefix Expected: $expected\n";
		} else {
		    print $TESTERR "# $prefix Expected: $expected ($diag)\n";
		}
	    }
	    push @FAILDETAIL, $detail;
	}
    }
    ++ $ntest;
    $ok;
}

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

	local($TestLevel) = $TestLevel+1;  #ignore this stack frame
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

These tests are expected to succeed.  If they don't something's
screwed up!

=item * SKIPPED TESTS

Skip is for tests that might or might not be possible to run depending
on the availability of platform specific features.  The first argument
should evaluate to true (think "yes, please skip") if the required
feature is not available.  After the first argument, skip works
exactly the same way as do normal tests.

=item * TODO TESTS

TODO tests are designed for maintaining an B<executable TODO list>.
These tests are expected NOT to succeed.  If a TODO test does succeed,
the feature in question should not be on the TODO list, now should it?

Packages should NOT be released with succeeding TODO tests.  As soon
as a TODO test starts working, it should be promoted to a normal test
and the newly working feature should be documented in the release
notes or change log.

=back

=head1 ONFAIL

  BEGIN { plan test => 4, onfail => sub { warn "CALL 911!" } }

While test failures should be enough, extra diagnostics can be
triggered at the end of a test run.  C<onfail> is passed an array ref
of hash refs that describe each test failure.  Each hash will contain
at least the following fields: C<package>, C<repetition>, and
C<result>.  (The file, line, and test number are not included because
their correspondence to a particular test is tenuous.)  If the test
had an expected value or a diagnostic string, these will also be
included.

The B<optional> C<onfail> hook might be used simply to print out the
version of your package and/or how to report problems.  It might also
be used to generate extremely sophisticated diagnostics for a
particularly bizarre test failure.  However it's not a panacea.  Core
dumps or other unrecoverable errors prevent the C<onfail> hook from
running.  (It is run inside an C<END> block.)  Besides, C<onfail> is
probably over-kill in most cases.  (Your test code should be simpler
than the code it is testing, yes?)


=head1 BUGS and CAVEATS

ok()'s special handling of subroutine references is an unfortunate
"feature" that can't be removed due to compatibility.

ok()'s use of string eq can sometimes cause odd problems when comparing
numbers, especially if you're casting a string to a number:

    $foo = "1.0";
    ok( $foo, 1 );      # not ok, "1.0" ne 1

Your best bet is to use the single argument form:

    ok( $foo == 1 );    # ok "1.0" == 1

ok()'s special handing of strings which look like they might be
regexes can also cause unexpected behavior.  An innocent:

    ok( $fileglob, '/path/to/some/*stuff/' );

will fail since Test.pm considers the second argument to a regex.
Again, best bet is to use the single argument form:

    ok( $fileglob eq '/path/to/some/*stuff/' );


=head1 NOTE

This module is no longer actively being developed, only bug fixes and
small tweaks (I'll still accept patches).  If you desire additional
functionality, consider L<Test::More> or L<Test::Unit>.


=head1 SEE ALSO

L<Test::Simple>, L<Test::More>, L<Test::Harness>, L<Devel::Cover>

L<Test::Builder> for building your own testing library.

L<Test::Unit> is an interesting XUnit-style testing library.

L<Test::Inline> and L<SelfTest> let you embed tests in code.


=head1 AUTHOR

Copyright (c) 1998-2000 Joshua Nathaniel Pritikin.  All rights reserved.
Copyright (c) 2001-2002 Michael G Schwern.

Current maintainer, Michael G Schwern <schwern@pobox.com>

This package is free software and is provided "as is" without express
or implied warranty.  It may be used, redistributed and/or modified
under the same terms as Perl itself.

=cut
