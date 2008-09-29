package threads::shared;

use 5.008;

use strict;
use warnings;

our $VERSION = '1.14';
my $XS_VERSION = $VERSION;
$VERSION = eval $VERSION;

# Declare that we have been loaded
$threads::shared::threads_shared = 1;

# Load the XS code, if applicable
if ($threads::threads) {
    require XSLoader;
    XSLoader::load('threads::shared', $XS_VERSION);

    *is_shared = \&_id;

} else {
    # String eval is generally evil, but we don't want these subs to
    # exist at all if 'threads' is not loaded successfully.
    # Vivifying them conditionally this way saves on average about 4K
    # of memory per thread.
    eval <<'_MARKER_';
        sub share          (\[$@%])         { return $_[0] }
        sub is_shared      (\[$@%])         { undef }
        sub cond_wait      (\[$@%];\[$@%])  { undef }
        sub cond_timedwait (\[$@%]$;\[$@%]) { undef }
        sub cond_signal    (\[$@%])         { undef }
        sub cond_broadcast (\[$@%])         { undef }
_MARKER_
}


### Export ###

sub import
{
    # Exported subroutines
    my @EXPORT = qw(share is_shared cond_wait cond_timedwait
                    cond_signal cond_broadcast);
    if ($threads::threads) {
        push(@EXPORT, 'bless');
    }

    # Export subroutine names
    my $caller = caller();
    foreach my $sym (@EXPORT) {
        no strict 'refs';
        *{$caller.'::'.$sym} = \&{$sym};
    }
}


### Methods, etc. ###

sub threads::shared::tie::SPLICE
{
    require Carp;
    Carp::croak('Splice not implemented for shared arrays');
}

1;

__END__

=head1 NAME

threads::shared - Perl extension for sharing data structures between threads

=head1 VERSION

This document describes threads::shared version 1.14

=head1 SYNOPSIS

  use threads;
  use threads::shared;

  my $var :shared;
  $var = $scalar_value;
  $var = $shared_ref_value;
  $var = share($simple_unshared_ref_value);

  my ($scalar, @array, %hash);
  share($scalar);
  share(@array);
  share(%hash);
  my $bar = &share([]);
  $hash{bar} = &share({});

  { lock(%hash); ...  }

  cond_wait($scalar);
  cond_timedwait($scalar, time() + 30);
  cond_broadcast(@array);
  cond_signal(%hash);

  my $lockvar :shared;
  # condition var != lock var
  cond_wait($var, $lockvar);
  cond_timedwait($var, time()+30, $lockvar);

=head1 DESCRIPTION

By default, variables are private to each thread, and each newly created
thread gets a private copy of each existing variable.  This module allows you
to share variables across different threads (and pseudo-forks on Win32).  It is
used together with the L<threads> module.

=head1 EXPORT

C<share>, C<cond_wait>, C<cond_timedwait>, C<cond_signal>, C<cond_broadcast>,
C<is_shared>

Note that if this module is imported when L<threads> has not yet been loaded,
then these functions all become no-ops.  This makes it possible to write
modules that will work in both threaded and non-threaded environments.

=head1 FUNCTIONS

=over 4

=item share VARIABLE

C<share> takes a value and marks it as shared. You can share a scalar, array,
hash, scalar ref, array ref, or hash ref.  C<share> will return the shared
rvalue, but always as a reference.

A variable can also be marked as shared at compile time by using the
C<:shared> attribute: C<my $var :shared;>.

Due to problems with Perl's prototyping, if you want to share a newly created
reference, you need to use the C<&share([])> and C<&share({})> syntax.

The only values that can be assigned to a shared scalar are other scalar
values, or shared refs:

  my $var :shared;
  $var = 1;              # ok
  $var = [];             # error
  $var = &share([]);     # ok

C<share> will traverse up references exactly I<one> level.  C<share(\$a)> is
equivalent to C<share($a)>, while C<share(\\$a)> is not.  This means that you
must create nested shared data structures by first creating individual shared
leaf nodes, and then adding them to a shared hash or array.

  my %hash :shared;
  $hash{'meaning'} = &share([]);
  $hash{'meaning'}[0] = &share({});
  $hash{'meaning'}[0]{'life'} = 42;

=item is_shared VARIABLE

C<is_shared> checks if the specified variable is shared or not.  If shared,
returns the variable's internal ID (similar to
L<refaddr()|Scalar::Util/"refaddr EXPR">).  Otherwise, returns C<undef>.

  if (is_shared($var)) {
      print("\$var is shared\n");
  } else {
      print("\$var is not shared\n");
  }

=item lock VARIABLE

C<lock> places a lock on a variable until the lock goes out of scope.  If the
variable is locked by another thread, the C<lock> call will block until it's
available.  Multiple calls to C<lock> by the same thread from within
dynamically nested scopes are safe -- the variable will remain locked until
the outermost lock on the variable goes out of scope.

Locking a container object, such as a hash or array, doesn't lock the elements
of that container. For example, if a thread does a C<lock(@a)>, any other
thread doing a C<lock($a[12])> won't block.

C<lock()> follows references exactly I<one> level.  C<lock(\$a)> is equivalent
to C<lock($a)>, while C<lock(\\$a)> is not.

Note that you cannot explicitly unlock a variable; you can only wait for the
lock to go out of scope.  This is most easily accomplished by locking the
variable inside a block.

  my $var :shared;
  {
      lock($var);
      # $var is locked from here to the end of the block
      ...
  }
  # $var is now unlocked

If you need more fine-grained control over shared variable access, see
L<Thread::Semaphore>.

=item cond_wait VARIABLE

=item cond_wait CONDVAR, LOCKVAR

The C<cond_wait> function takes a B<locked> variable as a parameter, unlocks
the variable, and blocks until another thread does a C<cond_signal> or
C<cond_broadcast> for that same locked variable.  The variable that
C<cond_wait> blocked on is relocked after the C<cond_wait> is satisfied.  If
there are multiple threads C<cond_wait>ing on the same variable, all but one
will re-block waiting to reacquire the lock on the variable. (So if you're only
using C<cond_wait> for synchronisation, give up the lock as soon as possible).
The two actions of unlocking the variable and entering the blocked wait state
are atomic, the two actions of exiting from the blocked wait state and
re-locking the variable are not.

In its second form, C<cond_wait> takes a shared, B<unlocked> variable followed
by a shared, B<locked> variable.  The second variable is unlocked and thread
execution suspended until another thread signals the first variable.

It is important to note that the variable can be notified even if no thread
C<cond_signal> or C<cond_broadcast> on the variable.  It is therefore
important to check the value of the variable and go back to waiting if the
requirement is not fulfilled.  For example, to pause until a shared counter
drops to zero:

  { lock($counter); cond_wait($count) until $counter == 0; }

=item cond_timedwait VARIABLE, ABS_TIMEOUT

=item cond_timedwait CONDVAR, ABS_TIMEOUT, LOCKVAR

In its two-argument form, C<cond_timedwait> takes a B<locked> variable and an
absolute timeout as parameters, unlocks the variable, and blocks until the
timeout is reached or another thread signals the variable.  A false value is
returned if the timeout is reached, and a true value otherwise.  In either
case, the variable is re-locked upon return.

Like C<cond_wait>, this function may take a shared, B<locked> variable as an
additional parameter; in this case the first parameter is an B<unlocked>
condition variable protected by a distinct lock variable.

Again like C<cond_wait>, waking up and reacquiring the lock are not atomic,
and you should always check your desired condition after this function
returns.  Since the timeout is an absolute value, however, it does not have to
be recalculated with each pass:

  lock($var);
  my $abs = time() + 15;
  until ($ok = desired_condition($var)) {
      last if !cond_timedwait($var, $abs);
  }
  # we got it if $ok, otherwise we timed out!

=item cond_signal VARIABLE

The C<cond_signal> function takes a B<locked> variable as a parameter and
unblocks one thread that's C<cond_wait>ing on that variable. If more than one
thread is blocked in a C<cond_wait> on that variable, only one (and which one
is indeterminate) will be unblocked.

If there are no threads blocked in a C<cond_wait> on the variable, the signal
is discarded. By always locking before signaling, you can (with care), avoid
signaling before another thread has entered cond_wait().

C<cond_signal> will normally generate a warning if you attempt to use it on an
unlocked variable. On the rare occasions where doing this may be sensible, you
can suppress the warning with:

  { no warnings 'threads'; cond_signal($foo); }

=item cond_broadcast VARIABLE

The C<cond_broadcast> function works similarly to C<cond_signal>.
C<cond_broadcast>, though, will unblock B<all> the threads that are blocked in
a C<cond_wait> on the locked variable, rather than only one.

=back

=head1 OBJECTS

L<threads::shared> exports a version of L<bless()|perlfunc/"bless REF"> that
works on shared objects such that I<blessings> propagate across threads.

  # Create a shared 'foo' object
  my $foo;
  share($foo);
  $foo = &share({});
  bless($foo, 'foo');

  # Create a shared 'bar' object
  my $bar;
  share($bar);
  $bar = &share({});
  bless($bar, 'bar');

  # Put 'bar' inside 'foo'
  $foo->{'bar'} = $bar;

  # Rebless the objects via a thread
  threads->create(sub {
      # Rebless the outer object
      bless($foo, 'yin');

      # Cannot directly rebless the inner object
      #bless($foo->{'bar'}, 'yang');

      # Retrieve and rebless the inner object
      my $obj = $foo->{'bar'};
      bless($obj, 'yang');
      $foo->{'bar'} = $obj;

  })->join();

  print(ref($foo),          "\n");    # Prints 'yin'
  print(ref($foo->{'bar'}), "\n");    # Prints 'yang'
  print(ref($bar),          "\n");    # Also prints 'yang'

=head1 NOTES

threads::shared is designed to disable itself silently if threads are not
available. If you want access to threads, you must C<use threads> before you
C<use threads::shared>.  L<threads> will emit a warning if you use it after
L<threads::shared>.

=head1 BUGS AND LIMITATIONS

When C<share> is used on arrays, hashes, array refs or hash refs, any data
they contain will be lost.

  my @arr = qw(foo bar baz);
  share(@arr);
  # @arr is now empty (i.e., == ());

  # Create a 'foo' object
  my $foo = { 'data' => 99 };
  bless($foo, 'foo');

  # Share the object
  share($foo);        # Contents are now wiped out
  print("ERROR: \$foo is empty\n")
      if (! exists($foo->{'data'}));

Therefore, populate such variables B<after> declaring them as shared.  (Scalar
and scalar refs are not affected by this problem.)

It is often not wise to share an object unless the class itself has been
written to support sharing.  For example, an object's destructor may get
called multiple times, once for each thread's scope exit.  Another danger is
that the contents of hash-based objects will be lost due to the above
mentioned limitation.  See F<examples/class.pl> (in the CPAN distribution of
this module) for how to create a class that supports object sharing.

Does not support C<splice> on arrays!

Taking references to the elements of shared arrays and hashes does not
autovivify the elements, and neither does slicing a shared array/hash over
non-existent indices/keys autovivify the elements.

C<share()> allows you to C<< share($hashref->{key}) >> without giving any
error message.  But the C<< $hashref->{key} >> is B<not> shared, causing the
error "locking can only be used on shared values" to occur when you attempt to
C<< lock($hasref->{key}) >>.

View existing bug reports at, and submit any new bugs, problems, patches, etc.
to: L<http://rt.cpan.org/NoAuth/Bugs.html?Dist=threads-shared>

=head1 SEE ALSO

L<threads::shared> Discussion Forum on CPAN:
L<http://www.cpanforum.com/dist/threads-shared>

Annotated POD for L<threads::shared>:
L<http://annocpan.org/~JDHEDDEN/threads-shared-1.14/shared.pm>

Source repository:
L<http://code.google.com/p/threads-shared/>

L<threads>, L<perlthrtut>

L<http://www.perl.com/pub/a/2002/06/11/threads.html> and
L<http://www.perl.com/pub/a/2002/09/04/threads.html>

Perl threads mailing list:
L<http://lists.cpan.org/showlist.cgi?name=iThreads>

=head1 AUTHOR

Artur Bergman E<lt>sky AT crucially DOT netE<gt>

threads::shared is released under the same license as Perl.

Documentation borrowed from the old Thread.pm.

CPAN version produced by Jerry D. Hedden E<lt>jdhedden AT cpan DOT orgE<gt>.

=cut
