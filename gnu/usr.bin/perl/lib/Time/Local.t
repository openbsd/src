#!./perl

BEGIN {
  if ($ENV{PERL_CORE}){
    chdir('t') if -d 't';
    @INC = ('.', '../lib');
  }
}

use Time::Local;

# Set up time values to test
@time =
  (
   #year,mon,day,hour,min,sec 
   [1970,  1,  2, 00, 00, 00],
   [1980,  2, 28, 12, 00, 00],
   [1980,  2, 29, 12, 00, 00],
   [1999, 12, 31, 23, 59, 59],
   [2000,  1,  1, 00, 00, 00],
   [2010, 10, 12, 14, 13, 12],
   [2020,  2, 29, 12, 59, 59],
   [2030,  7,  4, 17, 07, 06],
# The following test fails on a surprising number of systems
# so it is commented out. The end of the Epoch for a 32-bit signed
# implementation of time_t should be Jan 19, 2038  03:14:07 UTC.
#  [2038,  1, 17, 23, 59, 59],     # last full day in any tz
  );

# use vmsish 'time' makes for oddness around the Unix epoch
if ($^O eq 'VMS') { $time[0][2]++ }

my $tests = @time * 2 + 4;
$tests += 2 if $ENV{PERL_CORE};

print "1..$tests\n";

$count = 1;
for (@time) {
    my($year, $mon, $mday, $hour, $min, $sec) = @$_;
    $year -= 1900;
    $mon --;
    if ($^O eq 'vos' && $count == 1) {
     print "ok $count -- skipping 1970 test on VOS.\n";
    } else {
     my $time = timelocal($sec,$min,$hour,$mday,$mon,$year);
     # print scalar(localtime($time)), "\n";
     my($s,$m,$h,$D,$M,$Y) = localtime($time);

     if ($s == $sec &&
	 $m == $min &&
	 $h == $hour &&
	 $D == $mday &&
	 $M == $mon &&
	 $Y == $year
        ) {
	 print "ok $count\n";
     } else {
      print "not ok $count\n";
     }
    }
    $count++;

    # Test gmtime function
    if ($^O eq 'vos' && $count == 2) {
        print "ok $count -- skipping 1970 test on VOS.\n";
    } else {
     $time = timegm($sec,$min,$hour,$mday,$mon,$year);
     ($s,$m,$h,$D,$M,$Y) = gmtime($time);

     if ($s == $sec &&
	 $m == $min &&
	 $h == $hour &&
	 $D == $mday &&
	 $M == $mon &&
	 $Y == $year
        ) {
	 print "ok $count\n";
     } else {
      print "not ok $count\n";
     }
    }
    $count++;
}

#print "Testing that the differences between a few dates makes sense...\n";

timelocal(0,0,1,1,0,90) - timelocal(0,0,0,1,0,90) == 3600
  or print "not ";
print "ok ", $count++, "\n";

timelocal(1,2,3,1,0,100) - timelocal(1,2,3,31,11,99) == 24 * 3600 
  or print "not ";
print "ok ", $count++, "\n";

# Diff beween Jan 1, 1980 and Mar 1, 1980 = (31 + 29 = 60 days)
timegm(0,0,0, 1, 2, 80) - timegm(0,0,0, 1, 0, 80) == 60 * 24 * 3600
  or print "not ";
print "ok ", $count++, "\n";

# bugid #19393
# At a DST transition, the clock skips forward, eg from 01:59:59 to
# 03:00:00. In this case, 02:00:00 is an invalid time, and should be
# treated like 03:00:00 rather than 01:00:00 - negative zone offsets used
# to do the latter
{
    my $hour = (localtime(timelocal(0, 0, 2, 7, 3, 102)))[2];
    # testers in US/Pacific should get 3,
    # other testers should get 2
    print "not " unless $hour == 2 || $hour == 3;
    print "ok ", $main::count++, "\n";
}

if ($ENV{PERL_CORE}) {
  #print "Testing timelocal.pl module too...\n";
  package test;
  require 'timelocal.pl';
  timegm(0,0,0,1,0,80) == main::timegm(0,0,0,1,0,80) or print "not ";
  print "ok ", $main::count++, "\n";

  timelocal(1,2,3,4,5,88) == main::timelocal(1,2,3,4,5,88) or print "not ";
  print "ok ", $main::count++, "\n";
}
