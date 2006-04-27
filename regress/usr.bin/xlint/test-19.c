 /*	$OpenBSD: test-19.c,v 1.3 2006/04/27 20:55:08 otto Exp $	*/

/*
 * Placed in the public domain by Chad Loder <cloder@openbsd.org>.
 *
 * Test lint dealing with type conversions.
 */
#include <limits.h>

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

	/* test with variables */
	c1(C);
	c1(UC);
	c1(S);
	c1(US);
	c1(I);
	c1(UI);
	c1(L);
	c1(UL);
	c1(LL);
	c1(ULL);
	c1(F);
	c1(D);
	c1(LD);

	uc1(C);
	uc1(UC);
	uc1(S);
	uc1(US);
	uc1(I);
	uc1(UI);
	uc1(L);
	uc1(UL);
	uc1(LL);
	uc1(ULL);
	uc1(F);
	uc1(D);
	uc1(LD);

	s1(C);
	s1(UC);
	s1(S);
	s1(US);
	s1(I);
	s1(UI);
	s1(L);
	s1(UL);
	s1(LL);
	s1(ULL);
	s1(F);
	s1(D);
	s1(LD);

	us1(C);
	us1(UC);
	us1(S);
	us1(US);
	us1(I);
	us1(UI);
	us1(L);
	us1(UL);
	us1(LL);
	us1(ULL);
	us1(F);
	us1(D);
	us1(LD);

	i1(C);
	i1(UC);
	i1(S);
	i1(US);
	i1(I);
	i1(UI);
	i1(L);
	i1(UL);
	i1(LL);
	i1(ULL);
	i1(F);
	i1(D);
	i1(LD);

	ui1(C);
	ui1(UC);
	ui1(S);
	ui1(US);
	ui1(I);
	ui1(UI);
	ui1(L);
	ui1(UL);
	ui1(LL);
	ui1(ULL);
	ui1(F);
	ui1(D);
	ui1(LD);

	f1(C);
	f1(UC);
	f1(S);
	f1(US);
	f1(I);
	f1(UI);
	f1(L);
	f1(UL);
	f1(LL);
	f1(ULL);
	f1(F);
	f1(D);
	f1(LD);

	l1(C);
	l1(UC);
	l1(S);
	l1(US);
	l1(I);
	l1(UI);
	l1(L);
	l1(UL);
	l1(LL);
	l1(ULL);
	l1(F);
	l1(D);
	l1(LD);

	ul1(C);
	ul1(UC);
	ul1(S);
	ul1(US);
	ul1(I);
	ul1(UI);
	ul1(L);
	ul1(UL);
	ul1(LL);
	ul1(ULL);
	ul1(F);
	ul1(D);
	ul1(LD);

	d1(C);
	d1(UC);
	d1(S);
	d1(US);
	d1(I);
	d1(UI);
	d1(L);
	d1(UL);
	d1(LL);
	d1(ULL);
	d1(F);
	d1(D);
	d1(LD);

	ll1(C);
	ll1(UC);
	ll1(S);
	ll1(US);
	ll1(I);
	ll1(UI);
	ll1(L);
	ll1(UL);
	ll1(LL);
	ll1(ULL);
	ll1(F);
	ll1(D);
	ll1(LD);

	ull1(C);
	ull1(UC);
	ull1(S);
	ull1(US);
	ull1(I);
	ull1(UI);
	ull1(L);
	ull1(UL);
	ull1(LL);
	ull1(ULL);
	ull1(F);
	ull1(D);
	ull1(LD);

	ld1(C);
	ld1(UC);
	ld1(S);
	ld1(US);
	ld1(I);
	ld1(UI);
	ld1(L);
	ld1(UL);
	ld1(LL);
	ld1(ULL);
	ld1(F);
	ld1(D);
	ld1(LD);


	c1(-1);
	c1(0);
	c1(1);

	uc1(-1);
	uc1(0);
	uc1(1);

	s1(-1);
	s1(0);
	s1(1);

	us1(-1);
	us1(0);
	us1(1);

	i1(-1);
	i1(0);
	i1(1);

	ui1(-1);
	ui1(0);
	ui1(1);

	f1(-1);
	f1(0);
	f1(1);

	l1(-1);
	l1(0);
	l1(1);

	ul1(-1);
	ul1(0);
	ul1(1);

	d1(-1);
	d1(0);
	d1(1);

	ll1(-1);
	ll1(0);
	ll1(1);

	ull1(-1);
	ull1(0);
	ull1(1);

	ld1(-1);
	ld1(0);
	ld1(1);

	/* now test with long constants */
	c1(-1L);
	c1(0L);
	c1(1L);

	uc1(-1L);
	uc1(0L);
	uc1(1L);

	s1(-1L);
	s1(0L);
	s1(1L);

	us1(-1L);
	us1(0L);
	us1(1L);

	i1(-1L);
	i1(0L);
	i1(1L);

	ui1(-1L);
	ui1(0L);
	ui1(1L);

	f1(-1L);
	f1(0L);
	f1(1L);

	l1(-1L);
	l1(0L);
	l1(1L);

	ul1(-1L);
	ul1(0L);
	ul1(1L);

	d1(-1L);
	d1(0L);
	d1(1L);

	ll1(-1L);
	ll1(0L);
	ll1(1L);

	ull1(-1L);
	ull1(0L);
	ull1(1L);

	ld1(-1L);
	ld1(0L);
	ld1(1L);

	/* now test with float constants */
	c1(-1.0f);
	c1(0.0f);
	c1(1.0f);

	uc1(-1.0f);
	uc1(0.0f);
	uc1(1.0f);

	s1(-1.0f);
	s1(0.0f);
	s1(1.0f);

	us1(-1.0f);
	us1(0.0f);
	us1(1.0f);

	i1(-1.0f);
	i1(0.0f);
	i1(1.0f);

	ui1(-1.0f);
	ui1(0.0f);
	ui1(1.0f);

	f1(-1.0f);
	f1(0.0f);
	f1(1.0f);

	l1(-1.0f);
	l1(0.0f);
	l1(1.0f);

	ul1(-1.0f);
	ul1(0.0f);
	ul1(1.0f);

	d1(-1.0f);
	d1(0.0f);
	d1(1.0f);

	ll1(-1.0f);
	ll1(0.0f);
	ll1(1.0f);

	ull1(-1.0f);
	ull1(0.0f);
	ull1(1.0f);

	ld1(-1.0f);
	ld1(0.0f);
	ld1(1.0f);

	/* now test with double constants */
	c1(-1.0);
	c1(0.0);
	c1(1.0);

	uc1(-1.0);
	uc1(0.0);
	uc1(1.0);

	s1(-1.0);
	s1(0.0);
	s1(1.0);

	us1(-1.0);
	us1(0.0);
	us1(1.0);

	i1(-1.0);
	i1(0.0);
	i1(1.0);

	ui1(-1.0);
	ui1(0.0);
	ui1(1.0);

	f1(-1.0);
	f1(0.0);
	f1(1.0);

	l1(-1.0);
	l1(0.0);
	l1(1.0);

	ul1(-1.0);
	ul1(0.0);
	ul1(1.0);

	d1(-1.0);
	d1(0.0);
	d1(1.0);

	ll1(-1.0);
	ll1(0.0);
	ll1(1.0);

	ull1(-1.0);
	ull1(0.0);
	ull1(1.0);

	ld1(-1.0);
	ld1(0.0);
	ld1(1.0);

	/* now test with long double constants */
	c1(-1.0L);
	c1(0.0L);
	c1(1.0L);

	uc1(-1.0L);
	uc1(0.0L);
	uc1(1.0L);

	s1(-1.0L);
	s1(0.0L);
	s1(1.0L);

	us1(-1.0L);
	us1(0.0L);
	us1(1.0L);

	i1(-1.0L);
	i1(0.0L);
	i1(1.0L);

	ui1(-1.0L);
	ui1(0.0L);
	ui1(1.0L);

	f1(-1.0L);
	f1(0.0L);
	f1(1.0L);

	l1(-1.0L);
	l1(0.0L);
	l1(1.0L);

	ul1(-1.0L);
	ul1(0.0L);
	ul1(1.0L);

	d1(-1.0L);
	d1(0.0L);
	d1(1.0L);

	ll1(-1.0L);
	ll1(0.0L);
	ll1(1.0L);

	ull1(-1.0L);
	ull1(0.0L);
	ull1(1.0L);

	ld1(-1.0L);
	ld1(0.0L);
	ld1(1.0L);

	ul1(4 * I);

	return 0;
}





