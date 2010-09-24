BEGIN {
    unshift @INC, 't/lib/';
}

use Filter::Simple::FilterOnlyTest qr/not ok/ => "ok", 
                                   "bad" => "ok", fail => "die";
print "1..9\n";

sub fail { print "ok ", $_[0], "\n" }
sub ok { print "ok ", $_[0], "\n" }

print "not ok 1\n";
print "bad 2\n";

fail(3);
&fail(4);

print "not " unless "whatnot okapi" eq "whatokapi";
print "ok 5\n";

ok 7 unless not ok 6;

no Filter::Simple::FilterOnlyTest; # THE FUN STOPS HERE

print "not " unless "not ok" =~ /^not /;
print "ok 8\n";

print "not " unless "bad" =~ /bad/;
print "ok 9\n";
