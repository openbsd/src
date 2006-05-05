/*      $OpenBSD: test-23.c,v 1.1 2006/05/05 20:02:11 otto Exp $ */

/*
 * Placed in the public domain by Otto Moerbeek <otto@drijf.net>.
 *
 * Test pointer casts
 */

struct foo {
	int a;
};

void
f(void)
{
	void *vp = 0;
	char *cp = 0;
	signed char *scp = 0;
	unsigned char *ucp = 0;
	short *sp = 0;
	struct foo *fp = 0;

	vp = (void *)vp;
	vp = (void *)cp;
	vp = (void *)scp;
	vp = (void *)ucp;
	vp = (void *)sp;
	vp = (void *)fp;

	cp = (char *)vp;
	cp = (char *)cp;
	cp = (char *)ucp;
	cp = (char *)scp;
	cp = (char *)sp;
	cp = (char *)fp;

	scp = (signed char *)vp;
	scp = (signed char *)cp;
	scp = (signed char *)ucp;
	scp = (signed char *)scp;
	scp = (signed char *)sp;
	scp = (signed char *)fp;

	ucp = (unsigned char *)vp;
	ucp = (unsigned char *)cp;
	ucp = (unsigned char *)ucp;
	ucp = (unsigned char *)scp;
	ucp = (unsigned char *)sp;
	ucp = (unsigned char *)fp;

	sp = (short *)vp;
	sp = (short *)cp;
	sp = (short *)ucp;
	sp = (short *)scp;
	sp = (short *)sp;
	sp = (short *)fp;

	fp = (struct foo *)vp;
	fp = (struct foo *)cp;
	fp = (struct foo *)ucp;
	fp = (struct foo *)scp;
	fp = (struct foo *)sp;
	fp = (struct foo *)fp;

}
