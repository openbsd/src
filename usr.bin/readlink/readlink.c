#include <sys/syslimits.h>
#include <stdio.h>
#include <unistd.h>
#include <err.h>

int
main(argc, argv)
int argc;
char **argv;
{
	char buf[PATH_MAX];

	if (argc != 2)
		errx(1, "usage: readlink symlink");

	if (readlink(argv[1], buf, PATH_MAX) < 0)
		exit(1);
	else
		printf("%s", buf);
	exit(0);
}
