/*
 * turn text into hex in a flexible way
 *
 * Jim Rees, University of Michigan, July 2000
 */
static char *rcsid = "$Id: input.c,v 1.1 2001/05/22 15:35:58 rees Exp $";

#include <stdio.h>
#include <ctype.h>

#ifdef TEST
main(ac, av)
int ac;
char *av[];
{
    int i, n;
    unsigned char obuf[256];

    while (1) {
	n = get_input(stdin, obuf, 1, sizeof obuf);
	if (!n)
	    break;
	for (i = 0; i < n; i++)
	    printf("%02x ", obuf[i]);
	printf("\n");
    }
    exit(0);
}
#endif

#ifndef __palmos__
int
get_input(FILE *f, unsigned char *obuf, int omin, int olen)
{
    int n = 0;
    char ibuf[1024];

    while (n < omin && fgets(ibuf, sizeof ibuf, f) != NULL)
	n += parse_input(ibuf, obuf + n, olen - n);
    return n;
}
#endif

int
parse_input(char *ibuf, unsigned char *obuf, int olen)
{
    char *cp;
    unsigned char *up;
    int its_hex, nhex, ntext, n, ndig;

    if (!strncmp(ibuf, "0x", 2)) {
	/* If it starts with '0x' it's hex */
	ibuf += 2;
	its_hex = 1;
    } else if (ibuf[0] == '\"') {
	/* If it starts with " it's text */
	ibuf++;
	its_hex = 0;
    } else {
	/* Count hex and non-hex characters */
	nhex = ntext = 0;
	for (cp = ibuf; *cp; cp++) {
	    if (isxdigit(*cp))
		nhex++;
	    if (!isspace(*cp) && *cp != '.')
		ntext++;
	}

	/*
	 * 1. Two characters is always text (scfs file names, for example)
	 * 2. Any non-space, non-hexdigit chars means it's text
	 */
	if (ntext == 2 || ntext > nhex)
	    its_hex = 0;
	else
	    its_hex = 1;
    }

    if (its_hex) {
	for (cp = ibuf, up = obuf, n = ndig = 0; *cp && (up - obuf < olen); cp++) {
	    if (isxdigit(*cp)) {
		n <<= 4;
		n += isdigit(*cp) ? (*cp - '0') : ((*cp & 0x5f) - 'A' + 10);
	    }
	    if (ndig >= 1) {
		*up++ = n;
		n = 0;
		ndig = 0;
	    } else if (isxdigit(*cp))
		ndig++;
	}
    } else {
	/* It's text */
	for (cp = ibuf, up = obuf; *cp && (up - obuf < olen); cp++) {
	    if (isprint(*cp))
		*up++ = *cp;
	}
    }

    return (up - obuf);
}
