#!./perl

BEGIN {
    print "1..3\n";
    *ARGV = *DATA;
}
print "ok 1\n";
print <>;
print "ok 3\n";

__DATA__
ok 2 - read from aliased DATA filehandle
