BEGIN {
	j = 1
	substr("", 1, ++j)	# does j get incremented?
	if (j != 2) {
		print "FAIL: substr arg list not evaluated"
		exit 1
	}
}
