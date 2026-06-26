BEGIN {
	for (i = 1; i < ARGC; i++) {
		printf "%s", ARGV[i]
		if (i < ARGC-1)
			printf " "
	}
	printf "\n"
	exit
}
