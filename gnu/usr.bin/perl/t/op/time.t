#!./perl

# $RCSfile: time.t,v $$Revision: 4.1 $$Date: 92/08/07 18:28:32 $

if ( $does_gmtime = gmtime(time) ) { 
    print "1..7\n" 
}
else { 
    print "1..4\n" 
}


my $test = 1;
sub ok ($$) {
    my($ok, $name) = @_;

    # You have to do it this way or VMS will get confused.
    print $ok ? "ok $test - $name\n" : "not ok $test - $name\n";

    printf "# Failed test at line %d\n", (caller)[2] unless $ok;

    $test++;
    return $ok;
}


($beguser,$begsys) = times;

$beg = time;

while (($now = time) == $beg) { sleep 1 }

ok($now > $beg && $now - $beg < 10,             'very basic time test');

for ($i = 0; $i < 100000; $i++) {
    ($nowuser, $nowsys) = times;
    $i = 200000 if $nowuser > $beguser && ( $nowsys >= $begsys || 
                                            (!$nowsys && !$begsys));
    last if time - $beg > 20;
}

ok($i >= 200000,                                'very basic times test');

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

exit 0 unless $does_gmtime;

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
