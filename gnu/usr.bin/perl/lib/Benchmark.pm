package Benchmark;

=head1 NAME

Benchmark - benchmark running times of code

timethis - run a chunk of code several times

timethese - run several chunks of code several times

timeit - run a chunk of code and see how long it goes

=head1 SYNOPSIS

    timethis ($count, "code");

    # Use Perl code in strings...
    timethese($count, {
	'Name1' => '...code1...',
	'Name2' => '...code2...',
    });

    # ... or use subroutine references.
    timethese($count, {
	'Name1' => sub { ...code1... },
	'Name2' => sub { ...code2... },
    });

    $t = timeit($count, '...other code...')
    print "$count loops of other code took:",timestr($t),"\n";

=head1 DESCRIPTION

The Benchmark module encapsulates a number of routines to help you
figure out how long it takes to execute some code.

=head2 Methods

=over 10

=item new

Returns the current time.   Example:

    use Benchmark;
    $t0 = new Benchmark;
    # ... your code here ...
    $t1 = new Benchmark;
    $td = timediff($t1, $t0);
    print "the code took:",timestr($td),"\n";

=item debug

Enables or disable debugging by setting the C<$Benchmark::Debug> flag:

    debug Benchmark 1;
    $t = timeit(10, ' 5 ** $Global ');
    debug Benchmark 0;

=back

=head2 Standard Exports

The following routines will be exported into your namespace
if you use the Benchmark module:

=over 10

=item timeit(COUNT, CODE)

Arguments: COUNT is the number of times to run the loop, and CODE is
the code to run.  CODE may be either a code reference or a string to
be eval'd; either way it will be run in the caller's package.

Returns: a Benchmark object.

=item timethis ( COUNT, CODE, [ TITLE, [ STYLE ]] )

Time COUNT iterations of CODE. CODE may be a string to eval or a
code reference; either way the CODE will run in the caller's package.
Results will be printed to STDOUT as TITLE followed by the times.
TITLE defaults to "timethis COUNT" if none is provided. STYLE
determines the format of the output, as described for timestr() below.

=item timethese ( COUNT, CODEHASHREF, [ STYLE ] )

The CODEHASHREF is a reference to a hash containing names as keys
and either a string to eval or a code reference for each value.
For each (KEY, VALUE) pair in the CODEHASHREF, this routine will
call

	timethis(COUNT, VALUE, KEY, STYLE)

=item timediff ( T1, T2 )

Returns the difference between two Benchmark times as a Benchmark
object suitable for passing to timestr().

=item timestr ( TIMEDIFF, [ STYLE, [ FORMAT ]] )

Returns a string that formats the times in the TIMEDIFF object in
the requested STYLE. TIMEDIFF is expected to be a Benchmark object
similar to that returned by timediff().

STYLE can be any of 'all', 'noc', 'nop' or 'auto'. 'all' shows each
of the 5 times available ('wallclock' time, user time, system time,
user time of children, and system time of children). 'noc' shows all
except the two children times. 'nop' shows only wallclock and the
two children times. 'auto' (the default) will act as 'all' unless
the children times are both zero, in which case it acts as 'noc'.

FORMAT is the L<printf(3)>-style format specifier (without the
leading '%') to use to print the times. It defaults to '5.2f'.

=back

=head2 Optional Exports

The following routines will be exported into your namespace
if you specifically ask that they be imported:

=over 10

=item clearcache ( COUNT )

Clear the cached time for COUNT rounds of the null loop.

=item clearallcache ( )

Clear all cached times.

=item disablecache ( )

Disable caching of timings for the null loop. This will force Benchmark
to recalculate these timings for each new piece of code timed.

=item enablecache ( )

Enable caching of timings for the null loop. The time taken for COUNT
rounds of the null loop will be calculated only once for each
different COUNT used.

=back

=head1 NOTES

The data is stored as a list of values from the time and times
functions:

      ($real, $user, $system, $children_user, $children_system)

in seconds for the whole loop (not divided by the number of rounds).

The timing is done using time(3) and times(3).

Code is executed in the caller's package.

The time of the null loop (a loop with the same
number of rounds but empty loop body) is subtracted
from the time of the real loop.

The null loop times are cached, the key being the
number of rounds. The caching can be controlled using
calls like these:

    clearcache($key);
    clearallcache();

    disablecache();
    enablecache();

=head1 INHERITANCE

Benchmark inherits from no other class, except of course
for Exporter.

=head1 CAVEATS

Comparing eval'd strings with code references will give you
inaccurate results: a code reference will show a slower
execution time than the equivalent eval'd string.

The real time timing is done using time(2) and
the granularity is therefore only one second.

Short tests may produce negative figures because perl
can appear to take longer to execute the empty loop
than a short test; try:

    timethis(100,'1');

The system time of the null loop might be slightly
more than the system time of the loop with the actual
code and therefore the difference might end up being E<lt> 0.

=head1 AUTHORS

Jarkko Hietaniemi <F<jhi@iki.fi>>, Tim Bunce <F<Tim.Bunce@ig.co.uk>>

=head1 MODIFICATION HISTORY

September 8th, 1994; by Tim Bunce.

March 28th, 1997; by Hugo van der Sanden: added support for code
references and the already documented 'debug' method; revamped
documentation.

=cut

use Carp;
use Exporter;
@ISA=(Exporter);
@EXPORT=qw(timeit timethis timethese timediff timestr);
@EXPORT_OK=qw(clearcache clearallcache disablecache enablecache);

&init;

sub init {
    $debug = 0;
    $min_count = 4;
    $min_cpu   = 0.4;
    $defaultfmt = '5.2f';
    $defaultstyle = 'auto';
    # The cache can cause a slight loss of sys time accuracy. If a
    # user does many tests (>10) with *very* large counts (>10000)
    # or works on a very slow machine the cache may be useful.
    &disablecache;
    &clearallcache;
}

sub debug { $debug = ($_[1] != 0); }

sub clearcache    { delete $cache{$_[0]}; }
sub clearallcache { %cache = (); }
sub enablecache   { $cache = 1; }
sub disablecache  { $cache = 0; }

# --- Functions to process the 'time' data type

sub new { my @t = (time, times); print "new=@t\n" if $debug; bless \@t; }

sub cpu_p { my($r,$pu,$ps,$cu,$cs) = @{$_[0]}; $pu+$ps         ; }
sub cpu_c { my($r,$pu,$ps,$cu,$cs) = @{$_[0]};         $cu+$cs ; }
sub cpu_a { my($r,$pu,$ps,$cu,$cs) = @{$_[0]}; $pu+$ps+$cu+$cs ; }
sub real  { my($r,$pu,$ps,$cu,$cs) = @{$_[0]}; $r              ; }

sub timediff {
    my($a, $b) = @_;
    my @r;
    for ($i=0; $i < @$a; ++$i) {
	push(@r, $a->[$i] - $b->[$i]);
    }
    bless \@r;
}

sub timestr {
    my($tr, $style, $f) = @_;
    my @t = @$tr;
    warn "bad time value" unless @t==5;
    my($r, $pu, $ps, $cu, $cs) = @t;
    my($pt, $ct, $t) = ($tr->cpu_p, $tr->cpu_c, $tr->cpu_a);
    $f = $defaultfmt unless defined $f;
    # format a time in the required style, other formats may be added here
    $style ||= $defaultstyle;
    $style = ($ct>0) ? 'all' : 'noc' if $style eq 'auto';
    my $s = "@t $style"; # default for unknown style
    $s=sprintf("%2d secs (%$f usr %$f sys + %$f cusr %$f csys = %$f cpu)",
			    @t,$t) if $style eq 'all';
    $s=sprintf("%2d secs (%$f usr %$f sys = %$f cpu)",
			    $r,$pu,$ps,$pt) if $style eq 'noc';
    $s=sprintf("%2d secs (%$f cusr %$f csys = %$f cpu)",
			    $r,$cu,$cs,$ct) if $style eq 'nop';
    $s;
}

sub timedebug {
    my($msg, $t) = @_;
    print STDERR "$msg",timestr($t),"\n" if $debug;
}

# --- Functions implementing low-level support for timing loops

sub runloop {
    my($n, $c) = @_;

    $n+=0; # force numeric now, so garbage won't creep into the eval
    croak "negative loopcount $n" if $n<0;
    confess "Usage: runloop(number, [string | coderef])" unless defined $c;
    my($t0, $t1, $td); # before, after, difference

    # find package of caller so we can execute code there
    my($curpack) = caller(0);
    my($i, $pack)= 0;
    while (($pack) = caller(++$i)) {
	last if $pack ne $curpack;
    }

    my $subcode = (ref $c eq 'CODE')
	? "sub { package $pack; my(\$_i)=$n; while (\$_i--){&\$c;} }"
	: "sub { package $pack; my(\$_i)=$n; while (\$_i--){$c;} }";
    my $subref  = eval $subcode;
    croak "runloop unable to compile '$c': $@\ncode: $subcode\n" if $@;
    print STDERR "runloop $n '$subcode'\n" if $debug;

    $t0 = &new;
    &$subref;
    $t1 = &new;
    $td = &timediff($t1, $t0);

    timedebug("runloop:",$td);
    $td;
}


sub timeit {
    my($n, $code) = @_;
    my($wn, $wc, $wd);

    printf STDERR "timeit $n $code\n" if $debug;

    if ($cache && exists $cache{$n}) {
	$wn = $cache{$n};
    } else {
	$wn = &runloop($n, '');
	$cache{$n} = $wn;
    }

    $wc = &runloop($n, $code);

    $wd = timediff($wc, $wn);

    timedebug("timeit: ",$wc);
    timedebug("      - ",$wn);
    timedebug("      = ",$wd);

    $wd;
}

# --- Functions implementing high-level time-then-print utilities

sub timethis{
    my($n, $code, $title, $style) = @_;
    my $t = timeit($n, $code);
    local $| = 1;
    $title = "timethis $n" unless defined $title;
    $style = "" unless defined $style;
    printf("%10s: ", $title);
    print timestr($t, $style),"\n";

    # A conservative warning to spot very silly tests.
    # Don't assume that your benchmark is ok simply because
    # you don't get this warning!
    print "            (warning: too few iterations for a reliable count)\n"
	if     $n < $min_count
	    || ($t->real < 1 && $n < 1000)
	    || $t->cpu_a < $min_cpu;
    $t;
}

sub timethese{
    my($n, $alt, $style) = @_;
    die "usage: timethese(count, { 'Name1'=>'code1', ... }\n"
		unless ref $alt eq HASH;
    my @names = sort keys %$alt;
    $style = "" unless defined $style;
    print "Benchmark: timing $n iterations of ",join(', ',@names),"...\n";

    # we could save the results in an array and produce a summary here
    # sum, min, max, avg etc etc
    map timethis($n, $alt->{$_}, $_, $style), @names;
}

1;
