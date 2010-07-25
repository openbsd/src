/*      $OpenBSD: test-25.c,v 1.1 2010/07/25 23:00:05 guenther Exp $ */

/*
 * Placed in the public domain by Philip Guenther <guenther@openbsd.org>.
 *
 * Test _Complex handling, based on test-19.c
 */

#include <limits.h>
#include <complex.h>

int
f(void)
{
	float f1;
	double d1;
	long double l1;

	float _Complex fc1;
	_Complex float fc2;

	double _Complex dc1;
	_Complex double dc2;

	long double _Complex lc1;
	double long _Complex lc2;
	double _Complex long lc3;
	long _Complex double lc4;
	_Complex long double lc5;
	_Complex double long lc6;

	/* test type compatibility by mixing pointers */
	if (&fc1 == &fc2)
		return 0;
	if (&dc1 == &dc2)
		return 0;
	if (&fc1 == &dc1 || &dc1 == &lc1 || &lc1 == &fc1)
		return 1;
	if (&__real__ fc1 == &f1 || &__imag__ fc1 == &f1 ||
	    &__real__ dc1 == &d1 || &__imag__ dc1 == &d1 ||
	    &__real__ lc1 == &l1 || &__imag__ lc1 == &l1)
		return 1;
	return (&lc1 != &lc2 && &lc1 != &lc3 && &lc1 != &lc4 &&
	    &lc1 != &lc5 && &lc1 != &lc6);
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
void fc1	(float _Complex f) { f++; }
void dc1	(double _Complex d) { d++; }
void ldc1	(long double _Complex ld) { ld++; }

/* ARGSUSED */
int
main(int argc, char* argv[])
{
	_Bool B = 1;
	signed char C = 1;
	unsigned char UC = 1;
	short S = 1;
	unsigned short US = 1;
	int II = 1;
	unsigned int UI = 1;
	long L = 1;
	unsigned long UL = 1;
	long long LL = 1;
	unsigned long long ULL = 1;
	float F = 1.0f;
	double D = 1.0;
	long double LD = 1.0L;
	float _Complex FC = 1.0f + I;
	double _Complex DC = 1.0 + I;
	long double _Complex LDC = 1.0L + I;

	f();

	/* test with variables */
	b1(FC);
	b1(DC);
	b1(LDC);

	c1(FC);
	c1(DC);
	c1(LDC);

	uc1(FC);
	uc1(DC);
	uc1(LDC);

	s1(FC);
	s1(DC);
	s1(LDC);

	us1(FC);
	us1(DC);
	us1(LDC);

	i1(FC);
	i1(DC);
	i1(LDC);

	ui1(FC);
	ui1(DC);
	ui1(LDC);

	f1(FC);
	f1(DC);
	f1(LDC);

	l1(FC);
	l1(DC);
	l1(LDC);

	ul1(FC);
	ul1(DC);
	ul1(LDC);

	d1(FC);
	d1(DC);
	d1(LDC);

	ll1(FC);
	ll1(DC);
	ll1(LDC);

	ull1(FC);
	ull1(DC);
	ull1(LDC);

	ld1(FC);
	ld1(DC);
	ld1(LDC);

	fc1(B);
	fc1(C);
	fc1(UC);
	fc1(S);
	fc1(US);
	fc1(II);
	fc1(UI);
	fc1(L);
	fc1(UL);
	fc1(LL);
	fc1(ULL);
	fc1(F);
	fc1(D);
	fc1(LD);
	fc1(FC);
	fc1(DC);
	fc1(LDC);

	dc1(B);
	dc1(C);
	dc1(UC);
	dc1(S);
	dc1(US);
	dc1(II);
	dc1(UI);
	dc1(L);
	dc1(UL);
	dc1(LL);
	dc1(ULL);
	dc1(F);
	dc1(D);
	dc1(LD);
	dc1(FC);
	dc1(DC);
	dc1(LDC);

	ldc1(B);
	ldc1(C);
	ldc1(UC);
	ldc1(S);
	ldc1(US);
	ldc1(II);
	ldc1(UI);
	ldc1(L);
	ldc1(UL);
	ldc1(LL);
	ldc1(ULL);
	ldc1(F);
	ldc1(D);
	ldc1(LD);
	ldc1(FC);
	ldc1(DC);
	ldc1(LDC);

	/* now test with int constants */
	fc1(-1);
	fc1(0);
	fc1(1);

	dc1(-1);
	dc1(0);
	dc1(1);

	ldc1(-1);
	ldc1(0);
	ldc1(1);

	/* now test with long constants */
	fc1(-1L);
	fc1(0L);
	fc1(1L);

	dc1(-1L);
	dc1(0L);
	dc1(1L);

	ldc1(-1L);
	ldc1(0L);
	ldc1(1L);

	/* now test with float constants */
	fc1(-1.0f);
	fc1(0.0f);
	fc1(1.0f);

	dc1(-1.0f);
	dc1(0.0f);
	dc1(1.0f);

	ldc1(-1.0f);
	ldc1(0.0f);
	ldc1(1.0f);

	/* now test with double constants */
	fc1(-1.0);
	fc1(0.0);
	fc1(1.0);

	dc1(-1.0);
	dc1(0.0);
	dc1(1.0);

	ldc1(-1.0);
	ldc1(0.0);
	ldc1(1.0);

	/* now test with long double constants */
	fc1(-1.0L);
	fc1(0.0L);
	fc1(1.0L);

	dc1(-1.0L);
	dc1(0.0L);
	dc1(1.0L);

	ldc1(-1.0L);
	ldc1(0.0L);
	ldc1(1.0L);

	return 0;
}
