package Time::Local;
require 5.000;
require Exporter;
use Carp;

@ISA = qw(Exporter);
@EXPORT = qw(timegm timelocal);

=head1 NAME

Time::Local - efficiently compute tome from local and GMT time

=head1 SYNOPSIS

    $time = timelocal($sec,$min,$hours,$mday,$mon,$year);
    $time = timegm($sec,$min,$hours,$mday,$mon,$year);

=head1 DESCRIPTION

These routines are quite efficient and yet are always guaranteed to agree
with localtime() and gmtime().  We manage this by caching the start times
of any months we've seen before.  If we know the start time of the month,
we can always calculate any time within the month.  The start times
themselves are guessed by successive approximation starting at the
current time, since most dates seen in practice are close to the
current date.  Unlike algorithms that do a binary search (calling gmtime
once for each bit of the time value, resulting in 32 calls), this algorithm
calls it at most 6 times, and usually only once or twice.  If you hit
the month cache, of course, it doesn't call it at all.

timelocal is implemented using the same cache.  We just assume that we're
translating a GMT time, and then fudge it when we're done for the timezone
and daylight savings arguments.  The timezone is determined by examining
the result of localtime(0) when the package is initialized.  The daylight
savings offset is currently assumed to be one hour.

Both routines return -1 if the integer limit is hit. I.e. for dates
after the 1st of January, 2038 on most machines.

=cut

@epoch = localtime(0);
$tzmin = $epoch[2] * 60 + $epoch[1];	# minutes east of GMT
if ($tzmin > 0) {
    $tzmin = 24 * 60 - $tzmin;		# minutes west of GMT
    $tzmin -= 24 * 60 if $epoch[5] == 70;	# account for the date line
}

$SEC = 1;
$MIN = 60 * $SEC;
$HR = 60 * $MIN;
$DAYS = 24 * $HR;
$YearFix = ((gmtime(946684800))[5] == 100) ? 100 : 0;

sub timegm {
    $ym = pack(C2, @_[5,4]);
    $cheat = $cheat{$ym} || &cheat;
    return -1 if $cheat<0;
    $cheat + $_[0] * $SEC + $_[1] * $MIN + $_[2] * $HR + ($_[3]-1) * $DAYS;
}

sub timelocal {
    $time = &timegm + $tzmin*$MIN;
    return -1 if $cheat<0;
    @test = localtime($time);
    $time -= $HR if $test[2] != $_[2];
    $time;
}

sub cheat {
    $year = $_[5];
    $month = $_[4];
    croak "Month out of range 0..11 in timelocal.pl" 
	if $month > 11 || $month < 0;
    croak "Day out of range 1..31 in timelocal.pl" 
	if $_[3] > 31 || $_[3] < 1;
    croak "Hour out of range 0..23 in timelocal.pl"
	if $_[2] > 23 || $_[2] < 0;
    croak "Minute out of range 0..59 in timelocal.pl"
	if $_[1] > 59 || $_[1] < 0;
    croak "Second out of range 0..59 in timelocal.pl"
	if $_[0] > 59 || $_[0] < 0;
    $guess = $^T;
    @g = gmtime($guess);
    $year += $YearFix if $year < $epoch[5];
    $lastguess = "";
    while ($diff = $year - $g[5]) {
	$guess += $diff * (363 * $DAYS);
	@g = gmtime($guess);
	if (($thisguess = "@g") eq $lastguess){
	    return -1; #date beyond this machine's integer limit
	}
	$lastguess = $thisguess;
    }
    while ($diff = $month - $g[4]) {
	$guess += $diff * (27 * $DAYS);
	@g = gmtime($guess);
	if (($thisguess = "@g") eq $lastguess){
	    return -1; #date beyond this machine's integer limit
	}
	$lastguess = $thisguess;
    }
    @gfake = gmtime($guess-1); #still being sceptic
    if ("@gfake" eq $lastguess){
	return -1; #date beyond this machine's integer limit
    }
    $g[3]--;
    $guess -= $g[0] * $SEC + $g[1] * $MIN + $g[2] * $HR + $g[3] * $DAYS;
    $cheat{$ym} = $guess;
}

1;
