BEGIN {
    if ($ENV{PERL_CORE}) {
        chdir('t') if -d 't';
	@INC = qw(lib/Filter/Simple ../lib);
    }
}

use FilterOnlyTest qr/ok/ => "not ok", "bad" => "ok";
print "1..6\n";

print "bad 1\n";
print "bad 2\n";
print "bad 3\n";
print  <DATA>;

__DATA__
ok 4
ok 5
ok 6
