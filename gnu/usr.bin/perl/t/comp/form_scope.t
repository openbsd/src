#!./perl
#
# Tests bug #22977.  Test case from Dave Mitchell.

print "1..2\n";

sub f ($);
sub f ($) {
my $test = $_[0];
write;
format STDOUT =
ok @<<<<<<<
$test
.
}

f(1);
f(2);
