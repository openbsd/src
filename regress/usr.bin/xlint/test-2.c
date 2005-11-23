/*      $OpenBSD: test-2.c,v 1.1 2005/11/23 00:13:56 cloder Exp $ */

/*
 * Placed in the public domain by Chad Loder <cloder@openbsd.org>.
 *
 * Test detection of right shift by too many bits.
 */

/* ARGSUSED */
int
main(int argc, char* argv[])
{
	unsigned char c, d;
	
	c = 'a';
	d = c << 7;     /* ok */
	d = c >> 7;     /* ok */
	c <<= 7;        /* ok */
	c >>= 7;        /* ok */

	d = c << 8;     /* ok */
	d = c >> 8;     /* right-shifting an 8-bit quantity by 8 bits */
	c <<= 8;        /* ok */
	c >>= 8;        /* right-shifting an 8-bit quantity by 8 bits */

	d = c << 9;     /* ok */
	d = c >> 9;     /* right-shifting an 8-bit quantity by 9 bits */
	c <<= 9;        /* ok */
	c >>= 9;        /* right-shifting/assign an 8-bit quantity by 9 bits */

	d = c << 10;    /* ok */
	d = c >> 10;    /* right-shifting an 8-bit quantity by 10 bits */
	c <<= 10;       /* ok */
	c >>= 10;       /* right-shifting/assign an 8-bit quantity by 10 bits */

        d++;
        return 0;
}
