/*	$OpenBSD: cvt.c,v 1.2 1996/06/26 05:39:39 deraadt Exp $	*/

/*
 * Quick hack to convert old binary sup when.collection files into 
 * the new ascii format.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

int
main(argc, argv)
	int argc;
	char *argv[];
{
	long b;
	FILE *fp;
	int fd;

	if (argc != 2) {
		(void) fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
		return 1;
	}

	if ((fd = open(argv[1], O_RDWR)) == -1) {
		perror("open");
		return 1;
	}

	if (read(fd, &b, sizeof(b)) != sizeof(b)) {
		perror("read");
		return 1;
	}

	if (lseek(fd, 0, SEEK_SET) == -1) {
		perror("lseek");
		return 1;
	}

	(void) close(fd);

	if ((fp = fopen(argv[1], "w")) == NULL) {
		perror("fopen");
		return 1;
	}

	if (fprintf(fp, "%ld\n", b) < 0) {
		perror("fprintf");
		return 1;
	}
	if (fclose(fp) != 0) {
		perror("fclose");
		return 1;
	}

	return 0;
}
