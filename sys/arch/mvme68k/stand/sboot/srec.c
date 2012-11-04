/*	$OpenBSD: srec.c,v 1.6 2012/11/04 13:34:51 miod Exp $ */

/*
 * Public domain, believed to be by Mike Price.
 *
 * convert binary file to Srecord format
 */
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <ctype.h>

size_t	get32(char *, size_t);
void	put32(size_t, uint32_t, const char *, uint32_t, uint32_t);
void	sput(const char *);
void	put(int);
int	checksum(uint32_t, const char *, size_t, uint32_t);

__dead void
usage(const char *progname)
{
	fprintf(stderr, "usage: %s {size} {hex_addr} {name}\n", progname);
	fprintf(stderr, "Size = 2, 3, or 4 byte address\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	char buf[32], *name;
	uint32_t base, addr, mask, size;
	size_t cc;

	if (argc != 4)
		usage(argv[0]);
	sscanf(argv[1], "%x", &size);
	sscanf(argv[2], "%x", &base);
	name = argv[3];

	switch (size) {
	case 2:
		printf("S0%02lX%04X", 2 + strlen(name) + 1, 0);
		mask = 0x0000ffff;
		break;
	case 3:
		printf("S0%02lX%06X", 3 + strlen(name) + 1, 0);
		mask = 0x00ffffff;
		break;
	case 4:
		printf("S0%02lX%08X", 4 + strlen(name) + 1, 0);
		mask = 0xffffffff;
		break;
	default:
		usage(argv[0]);
		/* NOTREACHED */
	}
	sput(name);
	printf("%02X\n", checksum(0, name, strlen(name), size));

	addr = base;
	for (;;) {
		cc = get32(buf, sizeof buf);
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
	default:
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
	printf("%02X\n", checksum(base, (char *) 0, 0, size) + (size - 2));
	exit(0);
}

size_t
get32(char *buf, size_t sz)
{
	char *cp = buf;
	size_t i;
	int c;

	for (i = 0; i < sz; i++) {
		if ((c = getchar()) != EOF)
			*cp++ = c;
		else
			break;
	}
	return (cp - buf);
}

void
put32(size_t len, uint32_t addr, const char *buf, uint32_t size, uint32_t mask)
{
	const char *cp = buf;
	size_t i;

	switch (size) {
	case 2:
		printf("S1%02lX%04X", 2 + len + 1, addr & mask);
		break;
	case 3:
		printf("S2%02lX%06X", 3 + len + 1, addr & mask);
		break;
	case 4:
		printf("S3%02lX%08X", 4 + len + 1, addr & mask);
		break;
	}
	for (i = 0; i < len; ++i)
		put(*cp++);
	printf("%02X\n", checksum(addr, buf, len, size));
}

void
sput(const char *s)
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
checksum(uint32_t addr, const char *buf, size_t len, uint32_t size)
{
	const char *cp = buf;
	int sum = 0xff - 1 - size - (len & 0xff);
	size_t i;

	switch (size) {
	case 4:
		sum -= (addr >> 24) & 0xff;
		/* FALLTHROUGH */
	case 3:
		sum -= (addr >> 16) & 0xff;
		/* FALLTHROUGH */
	case 2:
		sum -= (addr >> 8) & 0xff;
		sum -= addr & 0xff;
		break;
	}
	for (i = 0; i < len; ++i) {
		sum -= *cp++ & 0xff;
	}
	return (sum & 0xff);
}
