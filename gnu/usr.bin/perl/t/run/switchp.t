#!./perl -p

BEGIN {
    print "1..3\n";
    *ARGV = *DATA;
}

END {
    print "ok 3\n";
}

s/^not //;

__DATA__
not ok 1
not ok 2
