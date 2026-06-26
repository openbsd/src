BEGIN {
	delete ARGV
	ARGV[0] = "awk"
	ARGV[1] = "/dev/null"
	ARGC = 2
} {
	# this should not be executed
	print "FILENAME: " FILENAME
	fflush()
}
