#!./perl -n

BEGIN {
    print "1..3\n";
    *ARGV = *DATA;
}

END {
    print "ok 3\n";
}

print;

s/^/not /;

__DATA__
ok 1
ok 2
