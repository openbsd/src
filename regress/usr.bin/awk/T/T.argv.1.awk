BEGIN {
	for (i = 1; i < ARGC-1; i++)
		printf "%s ", ARGV[i]
	if (ARGC > 1)
		printf "%s", ARGV[i]
	printf "\n"
	exit
}
