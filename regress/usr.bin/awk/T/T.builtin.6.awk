BEGIN {
	j = 1
	sprintf("%d", 99, ++j)	# does j get incremented?
	if (j != 2) {
		print "FAIL: printf arg list not evaluated"
		exit 1
	}
}
