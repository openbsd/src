/*	$OpenBSD: srec.c,v 1.5 2003/08/19 10:22:30 deraadt Exp $ */

/*
 * Public domain, believed to be by Mike Price.
 *
 * convert binary file to Srecord format
 */
#include <stdio.h>
#include <ctype.h>

int get32();
void put32();
void sput();
void put();
int checksum();

int mask;
int size;

main(int argc, char *argv[])
{
	char buf[32], *name;
	int cc, base, addr;

	if (argc != 4) {
		fprintf(stderr, "usage: %s {size} {hex_addr} {name}\n", argv[0]);
		fprintf(stderr, "Size = 2, 3, or 4 byte address\n");
		exit(1);
	}
	sscanf(argv[1], "%x", &size);
	mask = (1 << (size * 8)) - 1;
	if (!mask)
		mask = (-1);
	sscanf(argv[2], "%x", &base);
	name = argv[3];

	if (size == 2)
		printf("S0%02X%04X", 2 + strlen(name) + 1, 0);
	if (size == 3)
		printf("S0%02X%06X", 3 + strlen(name) + 1, 0);
	if (size == 4)
		printf("S0%02X%08X", 4 + strlen(name) + 1, 0);
	sput(name);
	printf("%02X\n", checksum(0, name, strlen(name), size));

	addr = base;
	for (;;) {
		cc = get32(buf);
		if (cc > 0) {
			put32(cc, addr, buf, size, mask);
			addr += cc;
		} else
			break;
	}

	buf[0] = base >> 8;
	buf[1] = base;
	printf("S%d%02X", 11 - size, 2 + 1);
	switch (size) {
	case 2:
		printf("%04X", base & mask);
		break;
	case 3:
		printf("%06X", base & mask);
		break;
	case 4:
		printf("%08X", base & mask);
		break;
	}

	/*
	 * kludge -> some sizes need an extra count (1 if size == 3, 2 if
	 * size == 4).  Don't ask why.
	 */
	printf("%02X\n", checksum(base, (char *) 0, 0, size) +
	    (size - 2));
	exit (0);
}

int
get32(char buf[])
{
	char *cp = buf;
	int i, c;

	for (i = 0; i < 32; ++i) {
		if ((c = getchar()) != EOF)
			*cp++ = c;
		else
			break;
	}
	return (cp - buf);
}

void
put32(int len, int addr, char buf[], int size, int mask)
{
	char *cp = buf;
	int i;

	if (size == 2)
		printf("S1%02X%04X", 2 + len + 1, addr & mask);
	if (size == 3)
		printf("S2%02X%06X", 3 + len + 1, addr & mask);
	if (size == 4)
		printf("S3%02X%08X", 4 + len + 1, addr & mask);
	for (i = 0; i < len; ++i)
		put(*cp++);
	printf("%02X\n", checksum(addr, buf, len, size));
}

void
sput(char *s)
{
	while (*s != '\0')
		put(*s++);
}

void
put(int c)
{
	printf("%02X", c & 0xff);
}

int
checksum(int addr, char buf[], int len, int size)
{
	char *cp = buf;
	int sum = 0xff - 1 - size - (len & 0xff);
	int i;

	if (size == 4)
		sum -= (addr >> 24) & 0xff;
	if (size >= 3)
		sum -= (addr >> 16) & 0xff;
	sum -= (addr >> 8) & 0xff;
	sum -= addr & 0xff;
	for (i = 0; i < len; ++i) {
		sum -= *cp++ & 0xff;
	}
	return (sum & 0xff);
}
