#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

print "1..14\n";

# compile time evaluation

if (int(1.234) == 1) {print "ok 1\n";} else {print "not ok 1\n";}

if (int(-1.234) == -1) {print "ok 2\n";} else {print "not ok 2\n";}

# run time evaluation

$x = 1.234;
if (int($x) == 1) {print "ok 3\n";} else {print "not ok 3\n";}
if (int(-$x) == -1) {print "ok 4\n";} else {print "not ok 4\n";}

$x = length("abc") % -10;
print $x == -7 ? "ok 5\n" : "# expected -7, got $x\nnot ok 5\n";

{
    use integer;
    $x = length("abc") % -10;
    $y = (3/-10)*-10;
    print $x+$y == 3 && abs($x) < 10 ? "ok 6\n" : "not ok 6\n";
}

# check bad strings still get converted

@x = ( 6, 8, 10);
print "not " if $x["1foo"] != 8;
print "ok 7\n";

# check values > 32 bits work.

$x = 4294967303.15;
$y = int ($x);

if ($y eq "4294967303") {
  print "ok 8\n"
} else {
  print "not ok 8 # int($x) is $y, not 4294967303\n"
}

$y = int (-$x);

if ($y eq "-4294967303") {
  print "ok 9\n"
} else {
  print "not ok 9 # int($x) is $y, not -4294967303\n"
}

$x = 4294967294.2;
$y = int ($x);

if ($y eq "4294967294") {
  print "ok 10\n"
} else {
  print "not ok 10 # int($x) is $y, not 4294967294\n"
}

$x = 4294967295.7;
$y = int ($x);

if ($y eq "4294967295") {
  print "ok 11\n"
} else {
  print "not ok 11 # int($x) is $y, not 4294967295\n"
}

$x = 4294967296.11312;
$y = int ($x);

if ($y eq "4294967296") {
  print "ok 12\n"
} else {
  print "not ok 12 # int($x) is $y, not 4294967296\n"
}

$y = int(279964589018079/59);
if ($y == 4745162525730) {
  print "ok 13\n"
} else {
  print "not ok 13 # int(279964589018079/59) is $y, not 4745162525730\n"
}

$y = 279964589018079;
$y = int($y/59);
if ($y == 4745162525730) {
  print "ok 14\n"
} else {
  print "not ok 14 # int(279964589018079/59) is $y, not 4745162525730\n"
}

