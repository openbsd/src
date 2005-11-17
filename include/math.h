/*	$OpenBSD: math.h,v 1.10 2005/11/17 20:07:40 otto Exp $	*/
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice 
 * is preserved.
 * ====================================================
 */

/*
 * from: @(#)fdlibm.h 5.1 93/09/24
 */

#ifndef _MATH_H_
#define _MATH_H_

/*
 * ANSI/POSIX
 */
extern char __infinity[];
#define HUGE_VAL	(*(double *) __infinity)

/* 
 * C99
 */

/* XXX just appease the committee for now, needs proper defs... */

typedef float float_t;
typedef double double_t;
#define FLT_EVAL_METHOD (-1)

/*
 * XOPEN/SVID
 */
#if !defined(_ANSI_SOURCE) && !defined(_POSIX_SOURCE)
#define	M_E		2.7182818284590452354	/* e */
#define	M_LOG2E		1.4426950408889634074	/* log 2e */
#define	M_LOG10E	0.43429448190325182765	/* log 10e */
#define	M_LN2		0.69314718055994530942	/* log e2 */
#define	M_LN10		2.30258509299404568402	/* log e10 */
#define	M_PI		3.14159265358979323846	/* pi */
#define	M_PI_2		1.57079632679489661923	/* pi/2 */
#define	M_PI_4		0.78539816339744830962	/* pi/4 */
#define	M_1_PI		0.31830988618379067154	/* 1/pi */
#define	M_2_PI		0.63661977236758134308	/* 2/pi */
#define	M_2_SQRTPI	1.12837916709551257390	/* 2/sqrt(pi) */
#define	M_SQRT2		1.41421356237309504880	/* sqrt(2) */
#define	M_SQRT1_2	0.70710678118654752440	/* 1/sqrt(2) */

#ifdef __vax__
#define	MAXFLOAT        ((float)1.70141173319264430e+38)
#else
#define	MAXFLOAT	((float)3.40282346638528860e+38)
#endif

extern int signgam;

#if !defined(_XOPEN_SOURCE)
enum fdversion {fdlibm_ieee = -1, fdlibm_svid, fdlibm_xopen, fdlibm_posix};

#define _LIB_VERSION_TYPE enum fdversion
#define _LIB_VERSION _fdlib_version  

/* if global variable _LIB_VERSION is not desirable, one may 
 * change the following to be a constant by: 
 *	#define _LIB_VERSION_TYPE const enum version
 * In that case, after one initializes the value _LIB_VERSION (see
 * s_lib_version.c) during compile time, it cannot be modified
 * in the middle of a program
 */ 
extern  _LIB_VERSION_TYPE  _LIB_VERSION;

#define _IEEE_  fdlibm_ieee
#define _SVID_  fdlibm_svid
#define _XOPEN_ fdlibm_xopen
#define _POSIX_ fdlibm_posix

#ifndef __cplusplus
struct exception {
	int type;
	char *name;
	double arg1;
	double arg2;
	double retval;
};
#endif

#define	HUGE		MAXFLOAT

/* 
 * set X_TLOSS = pi*2**52, which is possibly defined in <values.h>
 * (one may replace the following line by "#include <values.h>")
 */

#define X_TLOSS		1.41484755040568800000e+16 

#define	DOMAIN		1
#define	SING		2
#define	OVERFLOW	3
#define	UNDERFLOW	4
#define	TLOSS		5
#define	PLOSS		6

#endif /* !_XOPEN_SOURCE */
#endif /* !_ANSI_SOURCE && !_POSIX_SOURCE */


#include <sys/cdefs.h>
__BEGIN_DECLS
/*
 * ANSI/POSIX
 */
extern double acos(double);
extern double asin(double);
extern double atan(double);
extern double atan2(double, double);
extern double cos(double);
extern double sin(double);
extern double tan(double);

extern double cosh(double);
extern double sinh(double);
extern double tanh(double);

extern double exp(double);
extern double frexp(double, int *);
extern double ldexp(double, int);
extern double log(double);
extern double log10(double);
extern double modf(double, double *);

extern double pow(double, double);
extern double sqrt(double);

extern double ceil(double);
extern double fabs(double);
extern double floor(double);
extern double fmod(double, double);

#if !defined(_ANSI_SOURCE) && !defined(_POSIX_SOURCE)
extern double erf(double);
extern double erfc(double);
extern double gamma(double);
extern double hypot(double, double);
extern int isinf(double);
extern int isnan(double);
extern int finite(double);
extern double j0(double);
extern double j1(double);
extern double jn(int, double);
extern double lgamma(double);
extern double y0(double);
extern double y1(double);
extern double yn(int, double);

#if !defined(_XOPEN_SOURCE)
extern double acosh(double);
extern double asinh(double);
extern double atanh(double);
extern double cbrt(double);
extern double logb(double);
extern double nextafter(double, double);
extern double remainder(double, double);
extern double scalb(double, double);

#ifdef __LIBM_PRIVATE
extern int matherr(struct exception *);
#endif

/*
 * IEEE Test Vector
 */
extern double significand(double);

/*
 * Functions callable from C, intended to support IEEE arithmetic.
 */
extern double copysign(double, double);
extern int ilogb(double);
extern double rint(double);
extern long int lrint(double);
extern long int lround(double);
extern long long int llrint(double);
extern long long int llround(double);
extern double scalbn(double, int);

/*
 * BSD math library entry points
 */
extern double cabs();
extern double drem(double, double);
extern double expm1(double);
extern double log1p(double);

/*
 * Reentrant version of gamma & lgamma; passes signgam back by reference
 * as the second argument; user must allocate space for signgam.
 */
#ifdef _REENTRANT
extern double gamma_r(double, int *);
extern double lgamma_r(double, int *);
#endif /* _REENTRANT */


/* float versions of ANSI/POSIX functions */
extern float acosf(float);
extern float asinf(float);
extern float atanf(float);
extern float atan2f(float, float);
extern float cosf(float);
extern float sinf(float);
extern float tanf(float);

extern float coshf(float);
extern float sinhf(float);
extern float tanhf(float);

extern float expf(float);
extern float frexpf(float, int *);
extern float ldexpf(float, int);
extern float logf(float);
extern float log10f(float);
extern float modff(float, float *);

extern float powf(float, float);
extern float sqrtf(float);

extern float ceilf(float);
extern float fabsf(float);
extern float floorf(float);
extern float fmodf(float, float);

extern float erff(float);
extern float erfcf(float);
extern float gammaf(float);
extern float hypotf(float, float);
extern int isinff(float);
extern int isnanf(float);
extern int finitef(float);
extern float j0f(float);
extern float j1f(float);
extern float jnf(int, float);
extern float lgammaf(float);
extern float y0f(float);
extern float y1f(float);
extern float ynf(int, float);

extern float acoshf(float);
extern float asinhf(float);
extern float atanhf(float);
extern float cbrtf(float);
extern float logbf(float);
extern float nextafterf(float, float);
extern float remainderf(float, float);
extern float scalbf(float, float);

/*
 * float version of IEEE Test Vector
 */
extern float significandf(float);

/*
 * Float versions of functions callable from C, intended to support
 * IEEE arithmetic.
 */
extern float copysignf(float, float);
extern int ilogbf(float);
extern float rintf(float);
extern long int lrintf(float);
extern long int lroundf(float);
extern long long int llrintf(float);
extern long long int llroundf(float);
extern float scalbnf(float, int);

/*
 * float versions of BSD math library entry points
 */
extern float cabsf ();
extern float dremf(float, float);
extern float expm1f(float);
extern float log1pf(float);

/*
 * Float versions of reentrant version of gamma & lgamma; passes
 * signgam back by reference as the second argument; user must
 * allocate space for signgam.
 */
#ifdef _REENTRANT
extern float gammaf_r(float, int *);
extern float lgammaf_r(float, int *);
#endif	/* _REENTRANT */

#endif /* !_XOPEN_SOURCE */
#endif /* !_ANSI_SOURCE && !_POSIX_SOURCE */
__END_DECLS

#endif /* _MATH_H_ */
