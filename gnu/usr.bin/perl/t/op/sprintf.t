#!./perl

# $RCSfile: sprintf.t,v $$Revision: 4.1 $$Date: 92/08/07 18:28:27 $

print "1..1\n";

$x = sprintf("%3s %-4s%%foo %5d%c%3.1f","hi",123,456,65,3.0999);
if ($x eq ' hi 123 %foo   456A3.1') {print "ok 1\n";} else {print "not ok 1 '$x'\n";}
