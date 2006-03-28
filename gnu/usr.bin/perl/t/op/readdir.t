#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

eval 'opendir(NOSUCH, "no/such/directory");';
if ($@) { print "1..0\n"; exit; }

print "1..11\n";

for $i (1..2000) {
    local *OP;
    opendir(OP, "op") or die "can't opendir: $!";
    # should auto-closedir() here
}

if (opendir(OP, "op")) { print "ok 1\n"; } else { print "not ok 1\n"; }
@D = grep(/^[^\.].*\.t$/i, readdir(OP));
closedir(OP);

##
## This range will have to adjust as the number of tests expands,
## as it's counting the number of .t files in src/t
##
my ($min, $max) = (125, 145);
if (@D > $min && @D < $max) { print "ok 2\n"; }
else {
    printf "not ok 2 # counting op/*.t, expect $min < %d < $max files\n",
      scalar @D;
}

@R = sort @D;
@G = sort <op/*.t>;
@G = sort <:op:*.t> if $^O eq 'MacOS';
if ($G[0] =~ m#.*\](\w+\.t)#i) {
    # grep is to convert filespecs returned from glob under VMS to format
    # identical to that returned by readdir
    @G = grep(s#.*\](\w+\.t).*#op/$1#i,<op/*.t>);
}
while (@R && @G && $G[0] eq ($^O eq 'MacOS' ? ':op:' : 'op/').$R[0]) {
	shift(@R);
	shift(@G);
}
if (@R == 0 && @G == 0) { print "ok 3\n"; } else { print "not ok 3\n"; }

if (opendir($fh, "op")) { print "ok 4\n"; } else { print "not ok 4\n"; }
if (ref($fh) eq 'GLOB') { print "ok 5\n"; } else { print "not ok 5\n"; }
if (opendir($fh[0], "op")) { print "ok 6\n"; } else { print "not ok 6\n"; }
if (ref($fh[0]) eq 'GLOB') { print "ok 7\n"; } else { print "not ok 7\n"; }
if (opendir($fh{abc}, "op")) { print "ok 8\n"; } else { print "not ok 8\n"; }
if (ref($fh{abc}) eq 'GLOB') { print "ok 9\n"; } else { print "not ok 9\n"; }
if ("$fh" ne "$fh[0]") { print "ok 10\n"; } else { print "not ok 10\n"; }
if ("$fh" ne "$fh{abc}") { print "ok 11\n"; } else { print "not ok 11\n"; }
