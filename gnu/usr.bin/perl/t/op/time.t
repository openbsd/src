#!./perl

# $RCSfile: time.t,v $$Revision: 1.3 $$Date: 1999/04/29 22:52:39 $

if ($does_gmtime = gmtime(time)) { print "1..5\n" }
else { print "1..3\n" }

($beguser,$begsys) = times;

$beg = time;

while (($now = time) == $beg) { sleep 1 }

if ($now > $beg && $now - $beg < 10){print "ok 1\n";} else {print "not ok 1\n";}

for ($i = 0; $i < 10000000; $i++) {
    ($nowuser, $nowsys) = times;
    $i = 20000000 if $nowuser > $beguser && ( $nowsys > $begsys || 
                                            (!$nowsys && !$begsys));
    last if time - $beg > 20;
}

if ($i >= 20000000) {print "ok 2\n";} else {print "not ok 2\n";}

($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime($beg);
($xsec,$foo) = localtime($now);
$localyday = $yday;

if ($sec != $xsec && $mday && $year)
    {print "ok 3\n";}
else
    {print "not ok 3\n";}

exit 0 unless $does_gmtime;

($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = gmtime($beg);
($xsec,$foo) = localtime($now);

if ($sec != $xsec && $mday && $year)
    {print "ok 4\n";}
else
    {print "not ok 4\n";}

if (index(" :0:1:-1:364:365:-364:-365:",':' . ($localyday - $yday) . ':') > 0)
    {print "ok 5\n";}
else
    {print "not ok 5\n";}
