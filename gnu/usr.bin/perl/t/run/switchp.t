#!./perl -p

BEGIN {
    print "1..2\n";
    *ARGV = *DATA;
}

__DATA__
ok 1
ok 2
