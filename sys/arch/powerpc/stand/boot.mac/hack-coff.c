/*	$OpenBSD: hack-coff.c,v 1.2 2001/07/04 08:38:53 niklas Exp $	*/

#include <stdio.h>

char magic[2] = { 1, 0xb };
char snos[12] = { 0, 1, 0, 1, 0, 2, 0, 0, 0, 0, 0, 3 };

main(int ac, char **av)
{
    int fd;

    if (ac != 2) {
	fprintf(stderr, "Usage: hack-coff coff-file\n");
	exit(1);
    }
    if ((fd = open(av[1], 2)) == -1) {
	perror(av[2]);
	exit(1);
    }
    if (lseek(fd, (long) 0x14, 0) == -1
	|| write(fd, magic, sizeof(magic)) != sizeof(magic)
	|| lseek(fd, (long) 0x34, 0) == -1
	|| write(fd, snos, sizeof(snos)) != sizeof(snos)) {
	fprintf(stderr, "%s: write error\n", av[1]);
	exit(1);
    }
    close(fd);
    exit(0);
}
