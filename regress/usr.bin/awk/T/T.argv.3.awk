BEGIN {
	print ARGC
	ARGV[ARGC-1] = ""
	for (i=0; i < ARGC; i++)
		print ARGV[i]
	exit
}
