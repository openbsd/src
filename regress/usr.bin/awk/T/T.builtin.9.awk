BEGIN {
	j = 1
	length("zzzz", ++j, ++j)	# does j get incremented?
	if (j != 3) {
		print "FAIL: excess length() args not evaluated"
		exit 1
	}
}
