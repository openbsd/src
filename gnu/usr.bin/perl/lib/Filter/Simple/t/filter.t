BEGIN {
    if ($ENV{PERL_CORE}) {
        chdir('t') if -d 't';
	@INC = qw(lib/Filter/Simple ../lib);
    }
}

use FilterTest qr/not ok/ => "ok", fail => "ok";

print "1..6\n";

sub fail { print "fail ", $_[0], "\n" }

print "not ok 1\n";
print "fail 2\n";

fail(3);
&fail(4);

print "not " unless "whatnot okapi" eq "whatokapi";
print "ok 5\n";

no FilterTest;

print "not " unless "not ok" =~ /^not /;
print "ok 6\n";

