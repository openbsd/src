#!./perl

$does_gmtime = gmtime(time);

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

plan tests => 8;

($beguser,$begsys) = times;

$beg = time;

while (($now = time) == $beg) { sleep 1 }

ok($now > $beg && $now - $beg < 10,             'very basic time test');

for ($i = 0; $i < 1_000_000; $i++) {
    for my $j (1..100) {}; # burn some user cycles
    ($nowuser, $nowsys) = times;
    $i = 2_000_000 if $nowuser > $beguser && ( $nowsys >= $begsys ||
                                            (!$nowsys && !$begsys));
    last if time - $beg > 20;
}

ok($i >= 2_000_000, 'very basic times test');

($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime($beg);
($xsec,$foo) = localtime($now);
$localyday = $yday;

ok($sec != $xsec && $mday && $year,             'localtime() list context');

ok(localtime() =~ /^(Sun|Mon|Tue|Wed|Thu|Fri|Sat)[ ]
                    (Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec)[ ]
                    ([ \d]\d)\ (\d\d):(\d\d):(\d\d)\ (\d{4})$
                  /x,
   'localtime(), scalar context'
  );

SKIP: {
    # This conditional of "No tzset()" is stolen from ext/POSIX/t/time.t
    skip "No tzset()", 1
        if $^O eq "MacOS" || $^O eq "VMS" || $^O eq "cygwin" ||
           $^O eq "djgpp" || $^O eq "MSWin32" || $^O eq "dos" ||
           $^O eq "interix";

# check that localtime respects changes to $ENV{TZ}
$ENV{TZ} = "GMT-5";
($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime($beg);
$ENV{TZ} = "GMT+5";
($sec,$min,$hour2,$mday,$mon,$year,$wday,$yday,$isdst) = localtime($beg);
ok($hour != $hour2,                             'changes to $ENV{TZ} respected');
}

SKIP: {
    skip "No gmtime()", 3 unless $does_gmtime;

($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = gmtime($beg);
($xsec,$foo) = localtime($now);

ok($sec != $xsec && $mday && $year,             'gmtime() list context');

my $day_diff = $localyday - $yday;
ok( grep({ $day_diff == $_ } (0, 1, -1, 364, 365, -364, -365)),
                     'gmtime() and localtime() agree what day of year');


# This could be stricter.
ok(gmtime() =~ /^(Sun|Mon|Tue|Wed|Thu|Fri|Sat)[ ]
                 (Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec)[ ]
                 ([ \d]\d)\ (\d\d):(\d\d):(\d\d)\ (\d{4})$
               /x,
   'gmtime(), scalar context'
  );
}
