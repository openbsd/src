#!./perl -n

BEGIN {
    print "1..2\n";
    *ARGV = *DATA;
}
print;

__DATA__
ok 1
ok 2
