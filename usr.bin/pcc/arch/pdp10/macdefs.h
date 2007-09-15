/*	$Id: macdefs.h,v 1.1.1.1 2007/09/15 18:12:29 otto Exp $	*/
/*
 * Copyright (c) 2003 Anders Magnusson (ragge@ludd.luth.se).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Machine-dependent defines for both passes.
 */

/*
 * Convert (multi-)character constant to integer.
 * Assume: If only one value; store at left side (char size), otherwise 
 * treat it as an integer.
 */
#define makecc(val,i) {			\
	if (i == 0) { lastcon = val;	\
	} else if (i == 1) { lastcon = (lastcon << 9) | val; lastcon <<= 18; \
	} else { lastcon |= (val << (27 - (i * 9))); } }

#define ARGINIT		36	/* # bits below fp where arguments start */
#define AUTOINIT	36	/* # bits above fp where automatics start */

/*
 * Storage space requirements
 */
#define SZCHAR		9
#define SZINT		36
#define SZFLOAT		36
#define SZDOUBLE	72
#define SZLONG		36
#define SZSHORT		18
#define SZPOINT		36
#define SZLONGLONG	72

/*
 * Alignment constraints
 */
#define ALCHAR		9
#define ALINT		36
#define ALFLOAT		36
#define ALDOUBLE	36
#define ALLONG		36
#define ALLONGLONG	36
#define ALSHORT		18
#define ALPOINT		36
#define ALSTRUCT	36
#define ALSTACK		36 

/*
 * Max values.
 */
#define	MAX_INT		0377777777777LL
#define	MAX_UNSIGNED	0777777777777ULL
#define	MAX_LONG	0377777777777LL
#define	MAX_ULONG	0777777777777ULL
#define	MAX_LONGLONG	000777777777777777777777LL	/* XXX cross */
#define	MAX_ULONGLONG	001777777777777777777777ULL	/* XXX cross */

/* Default char is unsigned */
#define	CHAR_UNSIGNED

/*
 * Use large-enough types.
 */
typedef	long long CONSZ;
typedef	unsigned long long U_CONSZ;
typedef long long OFFSZ;

#define CONFMT	"0%llo"		/* format for printing constants */
#define LABFMT	"L%d"		/* format for printing labels */

#define FPREG	016		/* frame pointer */
#define STKREG	017		/* stack pointer */

/*
 * Maximum and minimum register variables
 */
#define MINRVAR	010		/* use 10 thru ... */
#define MAXRVAR	015		/* ... 15 */

#define	PARAMS_UPWARD		/* stack grows upwards for parameters */
#undef BACKAUTO 		/* stack grows negatively for automatics */
#undef BACKTEMP 		/* stack grows negatively for temporaries */

#define	MYP2TREE(p) myp2tree(p);

#undef	FIELDOPS		/* no bit-field instructions */
#undef	RTOLBYTES		/* bytes are numbered left to right */

#define ENUMSIZE(high,low) INT	/* enums are always stored in full int */

/* Definitions mostly used in pass2 */

#define REGSZ	020
#define TMPREG	016

#define BYTEOFF(x)	((x)&03)
#define wdal(k)		(BYTEOFF(k)==0)
#define BITOOR(x)	((x)/36)		/* bit offset to oreg offset */

#define STOARG(p)
#define STOFARG(p)
#define STOSTARG(p)
#define genfcall(a,b)	gencall(a,b)

#define	szty(t)	(((t) == DOUBLE || (t) == FLOAT || \
	(t) == LONGLONG || (t) == ULONGLONG) ? 2 : 1)

#define	shltype(o, p) \
	((o) == REG || (o) == NAME || (o) == ICON || \
	 (o) == OREG || ((o) == UMUL && shumul((p)->n_left)))

#define MYREADER(p) myreader(p)
#define MYCANON(p) mycanon(p)
#define	MYOPTIM

#undef	SPECIAL_INTEGERS

/*
 * Special shapes used in code generation.
 */
#define	SUSHCON	(SPECIAL|6)	/* unsigned short constant */
#define	SNSHCON	(SPECIAL|7)	/* negative short constant */
#define	SILDB	(SPECIAL|8)	/* use ildb here */

