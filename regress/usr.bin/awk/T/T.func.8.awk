function foo() {
	for (i = 1; i <= 2; i++)
		return 3
	print "should not see this"
}
BEGIN { foo(); exit }
