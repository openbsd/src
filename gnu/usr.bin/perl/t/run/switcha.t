#!./perl -na

BEGIN {
    print "1..2\n";
    *ARGV = *DATA;
    $i = 0;
}
print "$F[1] ",++$i,"\n";

__DATA__
not ok
not ok 3
