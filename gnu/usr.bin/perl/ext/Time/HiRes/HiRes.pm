package Time::HiRes;

use strict;
use vars qw($VERSION $XS_VERSION @ISA @EXPORT @EXPORT_OK $AUTOLOAD);

require Exporter;
use XSLoader;

@ISA = qw(Exporter);

@EXPORT = qw( );
@EXPORT_OK = qw (usleep sleep ualarm alarm gettimeofday time tv_interval
		 getitimer setitimer ITIMER_REAL ITIMER_VIRTUAL ITIMER_PROF);

$VERSION = '1.20_00';
$XS_VERSION = $VERSION;
$VERSION = eval $VERSION;

sub AUTOLOAD {
    my $constname;
    ($constname= $AUTOLOAD) =~ s/.*:://;
    my $val = constant($constname, @_ ? $_[0] : 0);
    if ($!) {
	my ($pack,$file,$line) = caller;
	die "Your vendor has not defined Time::HiRes macro $constname, used at $file line $line.\n";
    }
    {
	no strict 'refs';
	*$AUTOLOAD = sub { $val };
    }
    goto &$AUTOLOAD;
}

XSLoader::load 'Time::HiRes', $XS_VERSION;

# Preloaded methods go here.

sub tv_interval {
    # probably could have been done in C
    my ($a, $b) = @_;
    $b = [gettimeofday()] unless defined($b);
    (${$b}[0] - ${$a}[0]) + ((${$b}[1] - ${$a}[1]) / 1_000_000);
}

# Autoload methods go after =cut, and are processed by the autosplit program.

1;
__END__

=head1 NAME

Time::HiRes - High resolution alarm, sleep, gettimeofday, interval timers

=head1 SYNOPSIS

  use Time::HiRes qw( usleep ualarm gettimeofday tv_interval );

  usleep ($microseconds);

  ualarm ($microseconds);
  ualarm ($microseconds, $interval_microseconds);

  $t0 = [gettimeofday];
  ($seconds, $microseconds) = gettimeofday;

  $elapsed = tv_interval ( $t0, [$seconds, $microseconds]);
  $elapsed = tv_interval ( $t0, [gettimeofday]);
  $elapsed = tv_interval ( $t0 );

  use Time::HiRes qw ( time alarm sleep );

  $now_fractions = time;
  sleep ($floating_seconds);
  alarm ($floating_seconds);
  alarm ($floating_seconds, $floating_interval);

  use Time::HiRes qw( setitimer getitimer
		      ITIMER_REAL ITIMER_VIRTUAL ITIMER_PROF );

  setitimer ($which, $floating_seconds, $floating_interval );
  getitimer ($which);

=head1 DESCRIPTION

The C<Time::HiRes> module implements a Perl interface to the usleep,
ualarm, gettimeofday, and setitimer/getitimer system calls. See the
EXAMPLES section below and the test scripts for usage; see your system
documentation for the description of the underlying usleep, ualarm,
gettimeofday, and setitimer/getitimer calls.

If your system lacks gettimeofday(2) or an emulation of it you don't
get gettimeofday() or the one-arg form of tv_interval().
If you don't have usleep(3) or select(2) you don't get usleep()
or sleep().  If your system don't have ualarm(3) or setitimer(2) you
don't get ualarm() or alarm().  If you try to import an unimplemented
function in the C<use> statement it will fail at compile time.

The following functions can be imported from this module.
No functions are exported by default.

=over 4

=item gettimeofday ()

In array context returns a 2 element array with the seconds and
microseconds since the epoch.  In scalar context returns floating
seconds like Time::HiRes::time() (see below).

=item usleep ( $useconds )

Sleeps for the number of microseconds specified.  Returns the number
of microseconds actually slept.  Can sleep for more than one second
unlike the usleep system call. See also Time::HiRes::sleep() below.

=item ualarm ( $useconds [, $interval_useconds ] )

Issues a ualarm call; interval_useconds is optional and will be 0 if 
unspecified, resulting in alarm-like behaviour.

=item tv_interval 

C<tv_interval ( $ref_to_gettimeofday [, $ref_to_later_gettimeofday] )>

Returns the floating seconds between the two times, which should have
been returned by gettimeofday(). If the second argument is omitted,
then the current time is used.

=item time ()

Returns a floating seconds since the epoch. This function can be
imported, resulting in a nice drop-in replacement for the C<time>
provided with core Perl, see the EXAMPLES below.

B<NOTE 1>: this higher resolution timer can return values either less or
more than the core time(), depending on whether your platforms rounds
the higher resolution timer values up, down, or to the nearest to get
the core time(), but naturally the difference should be never more than
half a second.

B<NOTE 2>: Since Sunday, September 9th, 2001 at 01:46:40 AM GMT
(when the time() seconds since epoch rolled over to 1_000_000_000),
the default floating point format of Perl and the seconds since epoch
have conspired to produce an apparent bug: if you print the value of
Time::HiRes::time() you seem to be getting only five decimals, not six
as promised (microseconds).  Not to worry, the microseconds are there
(assuming your platform supports such granularity).  What is going on
is that the default floating point format of Perl only outputs 15
digits.  In this case that means ten digits before the decimal
separator and five after.  To see the microseconds you can use either
printf/sprintf with C<%.6f>, or the gettimeofday() function in list
context, which will give you the seconds and microseconds as two
separate values.

=item sleep ( $floating_seconds )

Sleeps for the specified amount of seconds.  Returns the number of
seconds actually slept (a floating point value).  This function can be
imported, resulting in a nice drop-in replacement for the C<sleep>
provided with perl, see the EXAMPLES below.

=item alarm ( $floating_seconds [, $interval_floating_seconds ] )

The SIGALRM signal is sent after the specfified number of seconds.
Implemented using ualarm().  The $interval_floating_seconds argument
is optional and will be 0 if unspecified, resulting in alarm()-like
behaviour.  This function can be imported, resulting in a nice drop-in
replacement for the C<alarm> provided with perl, see the EXAMPLES below.

=item setitimer 

C<setitimer ( $which, $floating_seconds [, $interval_floating_seconds ] )>

Start up an interval timer: after a certain time, a signal arrives,
and more signals may keep arriving at certain intervals.  To disable
a timer, use time of zero.  If interval is set to zero (or unspecified),
the timer is disabled B<after> the next delivered signal.

Use of interval timers may interfere with alarm(), sleep(), and usleep().
In standard-speak the "interaction is unspecified", which means that
I<anything> may happen: it may work, it may not.

In scalar context, the remaining time in the timer is returned.

In list context, both the remaining time and the interval are returned.

There are three interval timers: the $which can be ITIMER_REAL,
ITIMER_VIRTUAL, or ITIMER_PROF.

ITIMER_REAL results in alarm()-like behavior.  Time is counted in
I<real time>, that is, wallclock time.  SIGALRM is delivered when
the timer expires.

ITIMER_VIRTUAL counts time in (process) I<virtual time>, that is, only
when the process is running.  In multiprocessor/user/CPU systems this
may be more or less than real or wallclock time.  (This time is also
known as the I<user time>.)  SIGVTALRM is delivered when the timer expires.

ITIMER_PROF counts time when either the process virtual time or when
the operating system is running on behalf of the process (such as
I/O).  (This time is also known as the I<system time>.)  (Collectively
these times are also known as the I<CPU time>.)  SIGPROF is delivered
when the timer expires.  SIGPROF can interrupt system calls.

The semantics of interval timers for multithreaded programs are
system-specific, and some systems may support additional interval
timers.  See your setitimer() documentation.

=item getitimer ( $which )

Return the remaining time in the interval timer specified by $which.

In scalar context, the remaining time is returned.

In list context, both the remaining time and the interval are returned.
The interval is always what you put in using setitimer().

=back

=head1 EXAMPLES

  use Time::HiRes qw(usleep ualarm gettimeofday tv_interval);

  $microseconds = 750_000;
  usleep $microseconds;

  # signal alarm in 2.5s & every .1s thereafter
  ualarm 2_500_000, 100_000;	

  # get seconds and microseconds since the epoch
  ($s, $usec) = gettimeofday;

  # measure elapsed time 
  # (could also do by subtracting 2 gettimeofday return values)
  $t0 = [gettimeofday];
  # do bunch of stuff here
  $t1 = [gettimeofday];
  # do more stuff here
  $t0_t1 = tv_interval $t0, $t1;
  
  $elapsed = tv_interval ($t0, [gettimeofday]);
  $elapsed = tv_interval ($t0);	# equivalent code

  #
  # replacements for time, alarm and sleep that know about
  # floating seconds
  #
  use Time::HiRes;
  $now_fractions = Time::HiRes::time;
  Time::HiRes::sleep (2.5);
  Time::HiRes::alarm (10.6666666);
 
  use Time::HiRes qw ( time alarm sleep );
  $now_fractions = time;
  sleep (2.5);
  alarm (10.6666666);

  # Arm an interval timer to go off first at 10 seconds and
  # after that every 2.5 seconds, in process virtual time

  use Time::HiRes qw ( setitimer ITIMER_VIRTUAL time );

  $SIG{VTLARM} = sub { print time, "\n" };
  setitimer(ITIMER_VIRTUAL, 10, 2.5);

=head1 C API

In addition to the perl API described above, a C API is available for
extension writers.  The following C functions are available in the
modglobal hash:

  name             C prototype
  ---------------  ----------------------
  Time::NVtime     double (*)()
  Time::U2time     void (*)(UV ret[2])

Both functions return equivalent information (like C<gettimeofday>)
but with different representations.  The names C<NVtime> and C<U2time>
were selected mainly because they are operating system independent.
(C<gettimeofday> is Un*x-centric.)

Here is an example of using NVtime from C:

  double (*myNVtime)();
  SV **svp = hv_fetch(PL_modglobal, "Time::NVtime", 12, 0);
  if (!svp)         croak("Time::HiRes is required");
  if (!SvIOK(*svp)) croak("Time::NVtime isn't a function pointer");
  myNVtime = INT2PTR(double(*)(), SvIV(*svp));
  printf("The current time is: %f\n", (*myNVtime)());

=head1 CAVEATS

Notice that the core time() maybe rounding rather than truncating.
What this means that the core time() may be giving time one second
later than gettimeofday(), also known as Time::HiRes::time().

=head1 AUTHORS

D. Wegscheid <wegscd@whirlpool.com>
R. Schertler <roderick@argon.org>
J. Hietaniemi <jhi@iki.fi>
G. Aas <gisle@aas.no>

=head1 REVISION

$Id: HiRes.pm,v 1.20 1999/03/16 02:26:13 wegscd Exp $

$Log: HiRes.pm,v $
Revision 1.20  1999/03/16 02:26:13  wegscd
Add documentation for NVTime and U2Time.

Revision 1.19  1998/09/30 02:34:42  wegscd
No changes, bump version.

Revision 1.18  1998/07/07 02:41:35  wegscd
No changes, bump version.

Revision 1.17  1998/07/02 01:45:13  wegscd
Bump version to 1.17

Revision 1.16  1997/11/13 02:06:36  wegscd
version bump to accomodate HiRes.xs fix.

Revision 1.15  1997/11/11 02:17:59  wegscd
POD editing, courtesy of Gisle Aas.

Revision 1.14  1997/11/06 03:14:35  wegscd
Update version # for Makefile.PL and HiRes.xs changes.

Revision 1.13  1997/11/05 05:36:25  wegscd
change version # for Makefile.pl and HiRes.xs changes.

Revision 1.12  1997/10/13 20:55:33  wegscd
Force a new version for Makefile.PL changes.

Revision 1.11  1997/09/05 19:59:33  wegscd
New version to bump version for README and Makefile.PL fixes.
Fix bad RCS log.

Revision 1.10  1997/05/23 01:11:38  wegscd
Conditional compilation; EXPORT_FAIL fixes.

Revision 1.2  1996/12/30 13:28:40  wegscd
Update documentation for what to do when missing ualarm() and friends.

Revision 1.1  1996/10/17 20:53:31  wegscd
Fix =head1 being next to __END__ so pod2man works

Revision 1.0  1996/09/03 18:25:15  wegscd
Initial revision

=head1 COPYRIGHT

Copyright (c) 1996-1997 Douglas E. Wegscheid.
All rights reserved. This program is free software; you can
redistribute it and/or modify it under the same terms as Perl itself.

=cut
