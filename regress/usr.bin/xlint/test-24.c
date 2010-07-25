/*      $OpenBSD: test-24.c,v 1.1 2010/07/25 23:00:05 guenther Exp $ */

/*
 * Placed in the public domain by Philip Guenther <guenther@openbsd.org>.
 *
 * Test _Bool handling.
 * Based in part on test-19.c, by Chad Loder <cloder@openbsd.org>. 
 */

void
f(void)
{
	_Bool b1;
	const _Bool b2 = 1;
	_Bool const b3 = 0;
	float fl = 4.3f;

	_Bool *bp = &b1;

	*bp = 3;
	if (b1 > 1 ||
	    b2 < 0 ||
	    *bp > 1 ||
	    *bp < 0)
	{
		*bp = 0;
	}

	b1 = fl;
}

void b1		(_Bool b){ b++; }
void c1		(signed char c){ c++; }
void uc1	(unsigned char uc) { uc++; }
void s1		(short s) { s++; }
void us1	(unsigned short us) { us++; }
void i1		(int i) { i++; }
void ui1	(unsigned int ui) { ui++; }
void f1		(float f) { f++; }
void l1		(long l) { l++; }
void ul1	(unsigned long ul) { ul++; }
void d1		(double d) { d++; }
void ll1	(long long ll) { ll++; }
void ull1	(unsigned long long ull) { ull++; }
void ld1	(long double ld) { ld++; }

/* ARGSUSED */
int
main(int argc, char* argv[])
{
	_Bool B = 1;
	signed char C = 1;
	unsigned char UC = 1;
	short S = 1;
	unsigned short US = 1;
	int I = 1;
	unsigned int UI = 1;
	long L = 1;
	unsigned long UL = 1;
	long long LL = 1;
	unsigned long long ULL = 1;
	float F = 1.0f;
	double D = 1.0;
	long double LD = 1.0L;

	f();

	/* test with variables */
	b1(B);
	b1(C);
	b1(UC);
	b1(S);
	b1(US);
	b1(I);
	b1(UI);
	b1(L);
	b1(UL);
	b1(LL);
	b1(ULL);
	b1(F);
	b1(D);
	b1(LD);

	c1(B);
	uc1(B);
	s1(B);
	us1(B);
	i1(B);
	ui1(B);
	f1(B);
	l1(B);
	ul1(B);
	d1(B);
	ll1(B);
	ull1(B);
	ld1(B);

	/* now test with int constants */
	b1(-1);
	b1(0);
	b1(1);

	/* now test with long constants */
	b1(-1L);
	b1(0L);
	b1(1L);

	/* now test with float constants */
	b1(-1.0f);
	b1(0.0f);
	b1(1.0f);

	/* now test with double constants */
	b1(-1.0);
	b1(0.0);
	b1(1.0);

	/* now test with long double constants */
	b1(-1.0L);
	b1(0.0L);
	b1(1.0L);

	return 0;
}
