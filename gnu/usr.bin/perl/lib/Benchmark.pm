package Benchmark;

=head1 NAME

Benchmark - benchmark running times of code

timethis - run a chunk of code several times

timethese - run several chunks of code several times

timeit - run a chunk of code and see how long it goes

=head1 SYNOPSIS

    timethis ($count, "code");

    timethese($count, {
	'Name1' => '...code1...',
	'Name2' => '...code2...',
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
    print "the code took:",timestr($dt),"\n";

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

Arguments: COUNT is the number of time to run the loop, and 
the second is the code to run.  CODE may be a string containing the code,
a reference to the function to run, or a reference to a hash containing 
keys which are names and values which are more CODE specs.

Side-effects: prints out noise to standard out.

Returns: a Benchmark object.  

=item timethis

=item timethese

=item timediff

=item timestr

=back

=head2 Optional Exports

The following routines will be exported into your namespace
if you specifically ask that they be imported:

=over 10

clearcache

clearallcache

disablecache

enablecache

=back

=head1 NOTES

The data is stored as a list of values from the time and times
functions: 

      ($real, $user, $system, $children_user, $children_system)

in seconds for the whole loop (not divided by the number of rounds).

The timing is done using time(3) and times(3).

Code is executed in the caller's package.

Enable debugging by:  

    $Benchmark::debug = 1;

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

The real time timing is done using time(2) and
the granularity is therefore only one second.

Short tests may produce negative figures because perl
can appear to take longer to execute the empty loop 
than a short test; try: 

    timethis(100,'1');

The system time of the null loop might be slightly
more than the system time of the loop with the actual
code and therefore the difference might end up being < 0.

More documentation is needed :-( especially for styles and formats.

=head1 AUTHORS

Jarkko Hietaniemi <Jarkko.Hietaniemi@hut.fi>,
Tim Bunce <Tim.Bunce@ig.co.uk>

=head1 MODIFICATION HISTORY

September 8th, 1994; by Tim Bunce.

=cut

# Purpose: benchmark running times of code.
#
#
# Usage - to time code snippets and print results:
#
#	timethis($count, '...code...');
#		
# prints:
#	timethis 100:  2 secs ( 0.23 usr  0.10 sys =  0.33 cpu)
#
#
#	timethese($count, {
#		Name1 => '...code1...',
#		Name2 => '...code2...',
#		... });
# prints:
#	Benchmark: timing 100 iterations of Name1, Name2...
#	     Name1:  2 secs ( 0.50 usr  0.00 sys =  0.50 cpu)
#	     Name2:  1 secs ( 0.48 usr  0.00 sys =  0.48 cpu)
#
# The default display style will automatically add child process
# values if non-zero.
#
#
# Usage - to time sections of your own code:
#
#	use Benchmark;
#	$t0 = new Benchmark;
#	... your code here ...
#	$t1 = new Benchmark;
#	$td = &timediff($t1, $t0);
#	print "the code took:",timestr($td),"\n";
#
#	$t = &timeit($count, '...other code...')
#	print "$count loops of other code took:",timestr($t),"\n";
# 
#
# Data format:
#       The data is stored as a list of values from the time and times
#       functions: ($real, $user, $system, $children_user, $children_system)
#	in seconds for the whole loop (not divided by the number of rounds).
#		
# Internals:
#	The timing is done using time(3) and times(3).
#		
#	Code is executed in the callers package
#
#	Enable debugging by:  $Benchmark::debug = 1;
#
#	The time of the null loop (a loop with the same
#	number of rounds but empty loop body) is substracted
#	from the time of the real loop.
#
#	The null loop times are cached, the key being the
#	number of rounds. The caching can be controlled using
#	&clearcache($key); &clearallcache;
#	&disablecache; &enablecache;
#
# Caveats:
#
#	The real time timing is done using time(2) and
#	the granularity is therefore only one second.
#
#	Short tests may produce negative figures because perl
#	can appear to take longer to execute the empty loop 
#	than a short test: try timethis(100,'1');
#
#	The system time of the null loop might be slightly
#	more than the system time of the loop with the actual
#	code and therefore the difference might end up being < 0
#
#	More documentation is needed :-(
#	Especially for styles and formats.
#
# Authors:	Jarkko Hietaniemi <Jarkko.Hietaniemi@hut.fi>
# 		Tim Bunce <Tim.Bunce@ig.co.uk>
#
#
# Last updated:	Sept 8th 94 by Tim Bunce
#

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

sub clearcache    { delete $cache{$_[0]}; }
sub clearallcache { %cache = (); }
sub enablecache   { $cache = 1; }
sub disablecache  { $cache = 0; }


# --- Functions to process the 'time' data type

sub new { my(@t)=(time, times); print "new=@t\n" if $debug; bless \@t; }

sub cpu_p { my($r,$pu,$ps,$cu,$cs) = @{$_[0]}; $pu+$ps         ; }
sub cpu_c { my($r,$pu,$ps,$cu,$cs) = @{$_[0]};         $cu+$cs ; }
sub cpu_a { my($r,$pu,$ps,$cu,$cs) = @{$_[0]}; $pu+$ps+$cu+$cs ; }
sub real  { my($r,$pu,$ps,$cu,$cs) = @{$_[0]}; $r              ; }

sub timediff{
    my($a, $b) = @_;
    my(@r);
    for($i=0; $i < @$a; ++$i){
	push(@r, $a->[$i] - $b->[$i]);
    }
    bless \@r;
}

sub timestr{
    my($tr, $style, $f) = @_;
    my(@t) = @$tr;
    warn "bad time value" unless @t==5;
    my($r, $pu, $ps, $cu, $cs) = @t;
    my($pt, $ct, $t) = ($tr->cpu_p, $tr->cpu_c, $tr->cpu_a);
    $f = $defaultfmt unless $f;
    # format a time in the required style, other formats may be added here
    $style = $defaultstyle unless $style;
    $style = ($ct>0) ? 'all' : 'noc' if $style=~/^auto$/;
    my($s) = "@t $style"; # default for unknown style
    $s=sprintf("%2d secs (%$f usr %$f sys + %$f cusr %$f csys = %$f cpu)",
			    @t,$t) if $style =~ /^all$/;
    $s=sprintf("%2d secs (%$f usr %$f sys = %$f cpu)",
			    $r,$pu,$ps,$pt) if $style =~ /^noc$/;
    $s=sprintf("%2d secs (%$f cusr %$f csys = %$f cpu)",
			    $r,$cu,$cs,$ct) if $style =~ /^nop$/;
    $s;
}
sub timedebug{
    my($msg, $t) = @_;
    print STDERR "$msg",timestr($t),"\n" if ($debug);
}


# --- Functions implementing low-level support for timing loops

sub runloop {
    my($n, $c) = @_;

    $n+=0; # force numeric now, so garbage won't creep into the eval
    croak "negativ loopcount $n" if $n<0;
    confess "Usage: runloop(number, string)" unless defined $c;
    my($t0, $t1, $td); # before, after, difference

    # find package of caller so we can execute code there
    my ($curpack) = caller(0);
    my ($i, $pack)= 0;
    while (($pack) = caller(++$i)) {
	last if $pack ne $curpack;
    }

    my $subcode = "sub { package $pack; my(\$_i)=$n; while (\$_i--){$c;} }";
    my $subref  = eval $subcode;
    croak "runloop unable to compile '$c': $@\ncode: $subcode\n" if $@;
    print STDERR "runloop $n '$subcode'\n" if ($debug);

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

    if ($cache && exists $cache{$n}){
	$wn = $cache{$n};
    }else{
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
    my($t) = timeit($n, $code);
    local($|) = 1;
    $title = "timethis $n" unless $title;
    $style = "" unless $style;
    printf("%10s: ", $title);
    print timestr($t, $style),"\n";
    # A conservative warning to spot very silly tests.
    # Don't assume that your benchmark is ok simply because
    # you don't get this warning!
    print "            (warning: too few iterations for a reliable count)\n"
	if (   $n < $min_count
	    || ($t->real < 1 && $n < 1000)
	    || $t->cpu_a < $min_cpu);
    $t;
}


sub timethese{
    my($n, $alt, $style) = @_;
    die "usage: timethese(count, { 'Name1'=>'code1', ... }\n"
		unless ref $alt eq HASH;
    my(@all);
    my(@names) = sort keys %$alt;
    $style = "" unless $style;
    print "Benchmark: timing $n iterations of ",join(', ',@names),"...\n";
    foreach(@names){
	$t = timethis($n, $alt->{$_}, $_, $style);
	push(@all, $t);
    }
    # we could produce a summary from @all here
    # sum, min, max, avg etc etc
    @all;
}


1;
