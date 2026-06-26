BEGIN {
	j = 1
	sub(/1/, ++j, z)	# does j get incremented?
	if (j != 2) {
		print "FAIL: sub() arg list not evaluated"
		exit 1
	}
}
