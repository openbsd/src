#!./perl

#
# test the bit operators '&', '|', '^', '~', '<<', and '>>'
#

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

print "1..18\n";

# numerics
print ((0xdead & 0xbeef) == 0x9ead ? "ok 1\n" : "not ok 1\n");
print ((0xdead | 0xbeef) == 0xfeef ? "ok 2\n" : "not ok 2\n");
print ((0xdead ^ 0xbeef) == 0x6042 ? "ok 3\n" : "not ok 3\n");
print ((~0xdead & 0xbeef) == 0x2042 ? "ok 4\n" : "not ok 4\n");

# shifts
print ((257 << 7) == 32896 ? "ok 5\n" : "not ok 5\n");
print ((33023 >> 7) == 257 ? "ok 6\n" : "not ok 6\n");

# signed vs. unsigned
print ((~0 > 0 && do { use integer; ~0 } == -1)
       ? "ok 7\n" : "not ok 7\n");

my $bits = 0;
for (my $i = ~0; $i; $i >>= 1) { ++$bits; }
my $cusp = 1 << ($bits - 1);

print ((($cusp & -1) > 0 && do { use integer; $cusp & -1 } < 0)
       ? "ok 8\n" : "not ok 8\n");
print ((($cusp | 1) > 0 && do { use integer; $cusp | 1 } < 0)
       ? "ok 9\n" : "not ok 9\n");
print ((($cusp ^ 1) > 0 && do { use integer; $cusp ^ 1 } < 0)
       ? "ok 10\n" : "not ok 10\n");
print (((1 << ($bits - 1)) == $cusp &&
	do { use integer; 1 << ($bits - 1) } == -$cusp)
       ? "ok 11\n" : "not ok 11\n");
print ((($cusp >> 1) == ($cusp / 2) &&
	do { use integer; $cusp >> 1 } == -($cusp / 2))
       ? "ok 12\n" : "not ok 12\n");

# short strings
print (("AAAAA" & "zzzzz") eq '@@@@@' ? "ok 13\n" : "not ok 13\n");
print (("AAAAA" | "zzzzz") eq '{{{{{' ? "ok 14\n" : "not ok 14\n");
print (("AAAAA" ^ "zzzzz") eq ';;;;;' ? "ok 15\n" : "not ok 15\n");

# long strings
$foo = "A" x 150;
$bar = "z" x 75;
print (($foo & $bar) eq ('@'x75 ) ? "ok 16\n" : "not ok 16\n");
print (($foo | $bar) eq ('{'x75 . 'A'x75) ? "ok 17\n" : "not ok 17\n");
print (($foo ^ $bar) eq (';'x75 . 'A'x75) ? "ok 18\n" : "not ok 18\n");
