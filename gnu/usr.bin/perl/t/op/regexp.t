#!./perl

# The tests are in a separate file 't/op/re_tests'.
# Each line in that file is a separate test.
# There are five columns, separated by tabs.
#
# Column 1 contains the pattern, optionally enclosed in C<''>.
# Modifiers can be put after the closing C<'>.
#
# Column 2 contains the string to be matched.
#
# Column 3 contains the expected result:
# 	y	expect a match
# 	n	expect no match
# 	c	expect an error
#
# Columns 4 and 5 are used only if column 3 contains C<y> or C<c>.
#
# Column 4 contains a string, usually C<$&>.
#
# Column 5 contains the expected result of double-quote
# interpolating that string after the match.

open(TESTS,'op/re_tests') || open(TESTS,'t/op/re_tests')
    || die "Can't open re_tests";

while (<TESTS>) { }
$numtests = $.;
seek(TESTS,0,0);
$. = 0;

$| = 1;
print "1..$numtests\n";
TEST:
while (<TESTS>) {
    ($pat, $subject, $result, $repl, $expect) = split(/[\t\n]/,$_);
    $input = join(':',$pat,$subject,$result,$repl,$expect);
    $pat = "'$pat'" unless $pat =~ /^[:']/;
    for $study ("", "study \$subject") {
	eval "$study; \$match = (\$subject =~ m$pat); \$got = \"$repl\";";
	if ($result eq 'c') {
	    if ($@ !~ m!^\Q$expect!) { print "not ok $.\n"; next TEST }
	    last;  # no need to study a syntax error
	}
	elsif ($result eq 'n') {
	    if ($match) { print "not ok $. $input => $got\n"; next TEST }
	}
	else {
	    if (!$match || $got ne $expect) {
		print "not ok $. $input => $got\n";
		next TEST;
	    }
	}
    }
    print "ok $.\n";
}

close(TESTS);
