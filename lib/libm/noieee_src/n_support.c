/*	$OpenBSD: n_support.c,v 1.25 2016/09/12 19:47:02 guenther Exp $	*/
/*	$NetBSD: n_support.c,v 1.1 1995/10/10 23:37:06 ragge Exp $	*/
/*
 * Copyright (c) 1985, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Some IEEE standard 754 recommended functions and remainder and sqrt for
 * supporting the C elementary functions.
 ******************************************************************************
 * WARNING:
 *      These codes are developed (in double) to support the C elementary
 * functions temporarily. They are not universal, and some of them are very
 * slow (in particular, remainder and sqrt is extremely inefficient). Each
 * computer system should have its implementation of these functions using
 * its own assembler.
 ******************************************************************************
 *
 * IEEE 754 required operations:
 *     remainder(x,p)
 *              returns  x REM y  =  x - [x/y]*y , where [x/y] is the integer
 *              nearest x/y; in half way case, choose the even one.
 *     sqrt(x)
 *              returns the square root of x correctly rounded according to
 *		the rounding mod.
 *
 * IEEE 754 recommended functions:
 * (a) copysign(x,y)
 *              returns x with the sign of y.
 * (b) scalbn(x,N)
 *              returns  x * (2**N), for integer values N.
 * (c) logb(x)
 *              returns the unbiased exponent of x, a signed integer in
 *              double precision, except that logb(0) is -INF, logb(INF)
 *              is +INF, and logb(NAN) is that NAN.
 *
 *
 * CODED IN C BY K.C. NG, 11/25/84;
 * REVISED BY K.C. NG on 1/22/85, 2/13/85, 3/24/85.
 */

#include <math.h>

#include "mathimpl.h"

#if defined(__vax__)      /* VAX D format */
#include <errno.h>
    static const unsigned short msign=0x7fff, mexp =0x7f80 ;
    static const short  prep1=57, gap=7, bias=129           ;
    static const double novf=1.7E38, nunf=3.0E-39;
#else	/* defined(__vax__) */
    static const unsigned short msign=0x7fff, mexp =0x7ff0  ;
    static const short prep1=54, gap=4, bias=1023           ;
    static const double novf=1.7E308, nunf=3.0E-308;
#endif	/* defined(__vax__) */

static const double zero = 0.0;

double
scalbn(double x, int N)
{
        int k;
        unsigned short *px=(unsigned short *) &x;

        if( x == zero )  return(x);

#if defined(__vax__)
        if( (k= *px & mexp ) != ~msign ) {
            if (N < -260)
		return(nunf*nunf);
	    else if (N > 260) {
		return(copysign(infnan(ERANGE),x));
	    }
#else	/* defined(__vax__) */
        if( (k= *px & mexp ) != mexp ) {
            if( N<-2100) return(nunf*nunf); else if(N>2100) return(novf+novf);
            if( k == 0 ) {
                 x *= scalbn(1.0,(int)prep1);  N -= prep1; return(scalbn(x,N));}
#endif	/* defined(__vax__) */

            if((k = (k>>gap)+ N) > 0 )
                if( k < (mexp>>gap) ) *px = (*px&~mexp) | (k<<gap);
                else x=novf+novf;               /* overflow */
            else
                if( k > -prep1 )
                                        /* gradual underflow */
                    {*px=(*px&~mexp)|(short)(1<<gap); x *= scalbn(1.0,k-1);}
                else
                return(nunf*nunf);
            }
        return(x);
}
DEF_STD(scalbn);
LDBL_CLONE(scalbn);

double
copysign(double x, double y)
{
        unsigned short  *px=(unsigned short *) &x,
                        *py=(unsigned short *) &y;

#if defined(__vax__)
        if ( (*px & mexp) == 0 ) return(x);
#endif	/* defined(__vax__) */

        *px = ( *px & msign ) | ( *py & ~msign );
        return(x);
}
DEF_STD(copysign);
LDBL_CLONE(copysign);

double
logb(double x)
{

        short *px=(short *) &x, k;

#if defined(__vax__)
        return (int)(((*px&mexp)>>gap)-bias);
#else	/* defined(__vax__) */
        if( (k= *px & mexp ) != mexp )
            if ( k != 0 )
                return ( (k>>gap) - bias );
            else if( x != zero)
                return ( -1022.0 );
            else
                return(-(1.0/zero));
        else if(isnan(x))
            return(x);
        else
            {*px &= msign; return(x);}
#endif	/* defined(__vax__) */
}
DEF_STD(logb);
LDBL_UNUSED_CLONE(logb);

double
remainder(double x, double p)
{
        short sign;
        double hp, dp, tmp;
        unsigned short k;
        unsigned short
              *px=(unsigned short *) &x  ,
              *pp=(unsigned short *) &p  ,
              *pd=(unsigned short *) &dp ,
              *pt=(unsigned short *) &tmp;

        *pp &= msign ;

#if defined(__vax__)
        if( ( *px & mexp ) == ~msign )	/* is x a reserved operand? */
#else	/* defined(__vax__) */
        if( ( *px & mexp ) == mexp )
#endif	/* defined(__vax__) */
		return  (x-p)-(x-p);	/* create nan if x is inf */
	if (p == zero) {
#if defined(__vax__)
		return(infnan(EDOM));
#else	/* defined(__vax__) */
		return zero/zero;
#endif	/* defined(__vax__) */
	}

#if defined(__vax__)
        if( ( *pp & mexp ) == ~msign )	/* is p a reserved operand? */
#else	/* defined(__vax__) */
        if( ( *pp & mexp ) == mexp )
#endif	/* defined(__vax__) */
		{ if (p != p) return p; else return x;}

        else  if ( ((*pp & mexp)>>gap) <= 1 )
                /* subnormal p, or almost subnormal p */
            { double b; b=scalbn(1.0,(int)prep1);
              p *= b; x = remainder(x,p); x *= b; return(remainder(x,p)/b);}
        else  if ( p >= novf/2)
            { p /= 2 ; x /= 2; return(remainder(x,p)*2);}
        else
            {
                dp=p+p; hp=p/2;
                sign= *px & ~msign ;
                *px &= msign       ;
                while ( x > dp )
                    {
                        k=(*px & mexp) - (*pd & mexp) ;
                        tmp = dp ;
                        *pt += k ;

#if defined(__vax__)
                        if( x < tmp ) *pt -= 128 ;
#else	/* defined(__vax__) */
                        if( x < tmp ) *pt -= 16 ;
#endif	/* defined(__vax__) */

                        x -= tmp ;
                    }
                if ( x > hp )
                    { x -= p ;  if ( x >= hp ) x -= p ; }

#if defined(__vax__)
		if (x)
#endif	/* defined(__vax__) */
			*px ^= sign;
                return( x);

            }
}
DEF_STD(remainder);

/* The drem() function is a deprecated alias for remainder(). */

double
drem(double x, double p)
{
	return remainder(x, p);
}

float
sqrtf(float x)
{
	return (float)sqrt((double) x);
}
DEF_STD(sqrtf);

double
sqrt(double x)
{
        double q, s, b, r;
        double t;
        int m, n, i;
#if defined(__vax__)
        int k=54;
#else	/* defined(__vax__) */
        int k=51;
#endif	/* defined(__vax__) */

    /* sqrt(NaN) is NaN, sqrt(+-0) = +-0 */
        if(isnan(x) || x == zero) return(x);

    /* sqrt(negative) is invalid */
        if(x<zero) {
#if defined(__vax__)
		return (infnan(EDOM));	/* NaN */
#else	/* defined(__vax__) */
		return(zero/zero);
#endif	/* defined(__vax__) */
	}

    /* sqrt(INF) is INF */
        if(!isfinite(x)) return(x);

    /* scale x to [1,4) */
        n=logb(x);
        x=scalbn(x,-n);
        if((m=logb(x))!=0) x=scalbn(x,-m);       /* subnormal number */
        m += n;
        n = m/2;
        if((n+n)!=m) {x *= 2; m -=1; n=m/2;}

    /* generate sqrt(x) bit by bit (accumulating in q) */
            q=1.0; s=4.0; x -= 1.0; r=1;
            for(i=1;i<=k;i++) {
                t=s+1; x *= 4; r /= 2;
                if(t<=x) {
                    s = t+t+2;
                    x -= t;
                    q += r;
                }
                else
                    s *= 2;
                }

    /* generate the last bit and determine the final rounding */
            r/=2; x *= 4;
            if(x==zero) goto end;
	    if (100+r >= 100) {			/* trigger inexact flag */
            if(s<x) {
                q+=r; x -=s; s += 2; s *= 2; x *= 4;
                t = (x-s)-5;
                b=1.0+3*r/4; if(b==1.0) goto end; /* b==1 : Round-to-zero */
                b=1.0+r/4;   if(b>1.0) t=1;	/* b>1 : Round-to-(+INF) */
                if(t>=0) q+=r; }	      /* else: Round-to-nearest */
            else {
                s *= 2; x *= 4;
                t = (x-s)-1;
                b=1.0+3*r/4; if(b==1.0) goto end;
                b=1.0+r/4;   if(b>1.0) t=1;
                if(t>=0) q+=r; }
	    }

end:        return(scalbn(q,n));
}
DEF_STD(sqrt);
LDBL_CLONE(sqrt);

#if 0
/* REMAINDER(X,Y)
 * RETURN X REM Y =X-N*Y, N=[X/Y] ROUNDED (ROUNDED TO EVEN IN THE HALF WAY CASE)
 * DOUBLE PRECISION (VAX D format 56 bits, IEEE DOUBLE 53 BITS)
 * INTENDED FOR ASSEMBLY LANGUAGE
 * CODED IN C BY K.C. NG, 3/23/85, 4/8/85.
 *
 * Warning: this code should not get compiled in unless ALL of
 * the following machine-dependent routines are supplied.
 *
 * Required machine dependent functions (not on a VAX):
 *     swapINX(i): save inexact flag and reset it to "i"
 *     swapENI(e): save inexact enable and reset it to "e"
 */

double
remainder(double x, double y)
{
	static const n0=0, n1=1, n2=2, n3=3;
    	static const unsigned short mexp =0x7ff0, m25 =0x0190, m57 =0x0390;
	double hy, y1, t, t1;
	short k;
	long n;
	int i, e;
	unsigned short xexp,yexp, *px  =(unsigned short *) &x  ,
	      		nx,nf,	  *py  =(unsigned short *) &y  ,
	      		sign,	  *pt  =(unsigned short *) &t  ,
	      			  *pt1 =(unsigned short *) &t1 ;

	xexp = px[n0] & mexp ;	/* exponent of x */
	yexp = py[n0] & mexp ;	/* exponent of y */
	sign = px[n0] &0x8000;	/* sign of x     */

/* return NaN if x is NaN, or y is NaN, or x is INF, or y is zero */
	if(isnan(x)) return(x); if(isnan(y)) return(y);	/* x or y is NaN */
	if( xexp == mexp )   return(zero/zero);		/* x is INF */
	if(y==zero) return(y/y);

/* save the inexact flag and inexact enable in i and e respectively
 * and reset them to zero
 */
	i=swapINX(0);	e=swapENI(0);

/* subnormal number */
	nx=0;
	if (yexp == 0) {
		t = 1.0;
		pt[n0] += m57;
		y *= t;
		nx = m57;
	}

/* if y is tiny (biased exponent <= 57), scale up y to y*2**57 */
	if( yexp <= m57 ) {py[n0]+=m57; nx+=m57; yexp+=m57;}

	nf=nx;
	py[n0] &= 0x7fff;
	px[n0] &= 0x7fff;

/* mask off the least significant 27 bits of y */
	t=y; pt[n3]=0; pt[n2]&=0xf800; y1=t;

/* LOOP: argument reduction on x whenever x > y */
loop:
	while ( x > y )
	{
	    t=y;
	    t1=y1;
	    xexp=px[n0]&mexp;	  /* exponent of x */
	    k=xexp-yexp-m25;
	    if(k>0) 	/* if x/y >= 2**26, scale up y so that x/y < 2**26 */
		{pt[n0]+=k;pt1[n0]+=k;}
	    n=x/t; x=(x-n*t1)-n*(t-t1);
	}
    /* end while (x > y) */

	if(nx!=0) {t=1.0; pt[n0]+=nx; x*=t; nx=0; goto loop;}

/* final adjustment */

	hy=y/2.0;
	if(x>hy||((x==hy)&&n%2==1)) x-=y;
	px[n0] ^= sign;
	if(nf!=0) { t=1.0; pt[n0]-=nf; x*=t;}

/* restore inexact flag and inexact enable */
	swapINX(i); swapENI(e);

	return(x);
}
DEF_STD(remainder);
#endif

#if 0
/* SQRT
 * RETURN CORRECTLY ROUNDED (ACCORDING TO THE ROUNDING MODE) SQRT
 * FOR IEEE DOUBLE PRECISION ONLY, INTENDED FOR ASSEMBLY LANGUAGE
 * CODED IN C BY K.C. NG, 3/22/85.
 *
 * Warning: this code should not get compiled in unless ALL of
 * the following machine-dependent routines are supplied.
 *
 * Required machine dependent functions:
 *     swapINX(i)  ...return the status of INEXACT flag and reset it to "i"
 *     swapRM(r)   ...return the current Rounding Mode and reset it to "r"
 *     swapENI(e)  ...return the status of inexact enable and reset it to "e"
 *     addc(t)     ...perform t=t+1 regarding t as a 64 bit unsigned integer
 *     subc(t)     ...perform t=t-1 regarding t as a 64 bit unsigned integer
 */

static const unsigned long table[] = {
0, 1204, 3062, 5746, 9193, 13348, 18162, 23592, 29598, 36145, 43202, 50740,
58733, 67158, 75992, 85215, 83599, 71378, 60428, 50647, 41945, 34246, 27478,
21581, 16499, 12183, 8588, 5674, 3403, 1742, 661, 130, };

double
newsqrt(double x)
{
        double y, z, t, addc(), subc();
	double const b54=134217728.*134217728.; /* b54=2**54 */
        long mx, scalx;
	long const mexp=0x7ff00000;
        int i, j, r, e, swapINX(), swapRM(), swapENI();
        unsigned long *py=(unsigned long *) &y   ,
                      *pt=(unsigned long *) &t   ,
                      *px=(unsigned long *) &x   ;
        const int n0=0, n1=1;
/* Rounding Mode:  RN ...round-to-nearest
 *                 RZ ...round-towards 0
 *                 RP ...round-towards +INF
 *		   RM ...round-towards -INF
 */
        const int RN=0, RZ=1, RP=2, RM=3;
				/* machine dependent: work on a Zilog Z8070
                                 * and a National 32081 & 16081
                                 */

/* exceptions */
	if(isnan(x) || x == 0.0) return(x);	/* sqrt(NaN) is NaN,
						   sqrt(+-0) = +-0 */
	if(x<0) return((x-x)/(x-x)); /* sqrt(negative) is invalid */
        if((mx=px[n0]&mexp)==mexp) return(x);  /* sqrt(+INF) is +INF */

/* save, reset, initialize */
        e=swapENI(0);   /* ...save and reset the inexact enable */
        i=swapINX(0);   /* ...save INEXACT flag */
        r=swapRM(RN);   /* ...save and reset the Rounding Mode to RN */
        scalx=0;

/* subnormal number, scale up x to x*2**54 */
        if(mx==0) {x *= b54 ; scalx-=0x01b00000;}

/* scale x to avoid intermediate over/underflow:
 * if (x > 2**512) x=x/2**512; if (x < 2**-512) x=x*2**512 */
        if(mx>0x5ff00000) {px[n0] -= 0x20000000; scalx+= 0x10000000;}
        if(mx<0x1ff00000) {px[n0] += 0x20000000; scalx-= 0x10000000;}

/* magic initial approximation to almost 8 sig. bits */
        py[n0]=(px[n0]>>1)+0x1ff80000;
        py[n0]=py[n0]-table[(py[n0]>>15)&31];

/* Heron's rule once with correction to improve y to almost 18 sig. bits */
        t=x/y; y=y+t; py[n0]=py[n0]-0x00100006; py[n1]=0;

/* triple to almost 56 sig. bits; now y approx. sqrt(x) to within 1 ulp */
        t=y*y; z=t;  pt[n0]+=0x00100000; t+=z; z=(x-z)*y;
        t=z/(t+x) ;  pt[n0]+=0x00100000; y+=t;

/* twiddle last bit to force y correctly rounded */
        swapRM(RZ);     /* ...set Rounding Mode to round-toward-zero */
        swapINX(0);     /* ...clear INEXACT flag */
        swapENI(e);     /* ...restore inexact enable status */
        t=x/y;          /* ...chopped quotient, possibly inexact */
        j=swapINX(i);   /* ...read and restore inexact flag */
        if(j==0) { if(t==y) goto end; else t=subc(t); }  /* ...t=t-ulp */
        x=b54+0.1;      /* ..trigger inexact flag, sqrt(x) is inexact */
        if(r==RN) t=addc(t);            /* ...t=t+ulp */
        else if(r==RP) { t=addc(t);y=addc(y);}/* ...t=t+ulp;y=y+ulp; */
        y=y+t;                          /* ...chopped sum */
        py[n0]=py[n0]-0x00100000;       /* ...correctly rounded sqrt(x) */
end:    py[n0]=py[n0]+scalx;            /* ...scale back y */
        swapRM(r);                      /* ...restore Rounding Mode */
        return(y);
}
#endif
