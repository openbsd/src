/*
 * Copyright (c) 2008 David Crawshaw <david@zentus.com>
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#define makecc(val,i)	lastcon = (lastcon<<8)|((val<<24)>>24);

#define ARGINIT		(7*8) /* XXX */
#define AUTOINIT	(0)

/* Type sizes */
#define SZCHAR		8
#define SZBOOL		32
#define SZINT		32
#define SZFLOAT		32
#define SZDOUBLE	64
#define SZLDOUBLE	64
#define SZLONG		32
#define SZSHORT		16
#define SZLONGLONG	64
#define SZPOINT(t)	64

/* Type alignments */
#define ALCHAR		8
#define ALBOOL		32
#define ALINT		32
#define ALFLOAT		32
#define ALDOUBLE	64
#define ALLDOUBLE	64
#define ALLONG		32
#define ALLONGLONG	64
#define ALSHORT		16
#define ALPOINT		32
#define ALSTRUCT	32
#define ALSTACK		64 

/* Min/max values. */
#define	MIN_CHAR	-128
#define	MAX_CHAR	127
#define	MAX_UCHAR	255
#define	MIN_SHORT	-32768
#define	MAX_SHORT	32767
#define	MAX_USHORT	65535
#define	MIN_INT		-1
#define	MAX_INT		0x7fffffff
#define	MAX_UNSIGNED	0xffffffff
#define	MIN_LONG	MIN_INT
#define	MAX_LONG	MAX_INT
#define	MAX_ULONG	MAX_UNSIGNED
#define	MIN_LONGLONG	0x8000000000000000LL
#define	MAX_LONGLONG	0x7fffffffffffffffLL
#define	MAX_ULONGLONG	0xffffffffffffffffULL

#define BOOL_TYPE	INT
#define WCHAR_TYPE	INT

typedef	long long CONSZ;
typedef	unsigned long long U_CONSZ;
typedef long long OFFSZ;

#define CONFMT	"%lld"
#define LABFMT  "L%d"
#define STABLBL "LL%d"

#define BACKAUTO 		/* Stack grows negatively for automatics. */
#define BACKTEMP 		/* Stack grows negatively for temporaries. */

#undef	FIELDOPS
#define RTOLBYTES

#define ENUMSIZE(high,low) INT
#define BYTEOFF(x) 	((x)&03)
#define BITOOR(x)	(x)

#define	szty(t)	(((t) == DOUBLE || (t) == FLOAT || \
	(t) == LONGLONG || (t) == ULONGLONG) ? 2 : 1)


/* Register names. */

#define MAXREGS	(31 + 2 + 31)
#define NUMCLASS 3

//define G0 	-1
#define G1 	0
#define G2 	1
#define G3 	2
#define G4 	3
#define G5 	4
#define G6 	5
#define G7 	6
#define O0 	7
#define O1 	8
#define O2 	9
#define O3 	10
#define O4 	11
#define O5 	12
#define O6 	13
#define O7 	14
#define L0 	15
#define L1 	16
#define L2 	17
#define L3 	18
#define L4 	19
#define L5 	20
#define L6 	21
#define L7 	22
#define I0 	23
#define I1 	24
#define I2 	25
#define I3 	26
#define I4 	27
#define I5 	28
#define I6 	29
#define I7 	30

#define SP 	31
#define FP 	32
/*
#define F0 	33
#define F1 	34
#define F2 	35
#define F3 	36
#define F4 	37
#define F5 	38
#define F6 	39
#define F7 	40
#define F8 	41
#define F9 	42
#define F10	43
#define F11	44
#define F12	45
#define F13	46
#define F14	47
#define F15	48
#define F16	49
#define F17	50
#define F18	51
#define F19	52
#define F20	53
#define F21	54
#define F22	55
#define F23	56
#define F24	57
#define F25	58
#define F26	59
#define F27	60
#define F28	61
#define F29	62
#define F30	63
*/

#define FPREG 	FP

#define RETREG(x)	((x)==DOUBLE || (x)==LDOUBLE || (x)==FLOAT ? 33 : O0)

#define RSTATUS \
	/* global */ \
		               SAREG|PERMREG, SAREG|PERMREG, SAREG|PERMREG, \
		SAREG|PERMREG, SAREG|PERMREG, SAREG|PERMREG, SAREG|PERMREG, \
	/* out */ \
		SAREG|TEMPREG, SAREG|TEMPREG, SAREG|TEMPREG, SAREG|TEMPREG, \
		SAREG|TEMPREG, SAREG|TEMPREG, SAREG|TEMPREG, SAREG|TEMPREG, \
	/* local */ \
		SAREG|TEMPREG, SAREG|TEMPREG, SAREG|TEMPREG, SAREG|TEMPREG, \
		SAREG|TEMPREG, SAREG|TEMPREG, SAREG|TEMPREG, SAREG|TEMPREG, \
	/* in */ \
		SAREG|TEMPREG, SAREG|TEMPREG, SAREG|TEMPREG, SAREG|TEMPREG, \
		SAREG|TEMPREG, SAREG|TEMPREG, SAREG|TEMPREG, SAREG|TEMPREG, \
	/* sp */ 0, \
	/* fp */ 0, \
	/* FP */ \
		SBREG, SBREG, SBREG, SBREG, SBREG, SBREG, SBREG, SBREG, \
		SBREG, SBREG, SBREG, SBREG, SBREG, SBREG, SBREG, SBREG, \
		SBREG, SBREG, SBREG, SBREG, SBREG, SBREG, SBREG, SBREG, \
		SBREG, SBREG, SBREG, SBREG, SBREG, SBREG, SBREG /*, SBREG */

#define ROVERLAP \
	        { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, \
	{ -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, \
	{ -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, \
	{ -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, \
	{ -1 }, { -1 }, \
	{ -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, \
	{ -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, \
	{ -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, \
	{ -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 } /*, { -1 } */

#define GCLASS(x) 	(x < 32 ? CLASSA : (x < 34 ? CLASSB : CLASSC))
#define PCLASS(p)	(1 << gclass((p)->n_type))
#define DECRA(x,y)	(((x) >> (y*6)) & 63)   /* decode encoded regs XXX */
#define ENCRA(x,y)	((x) << (6+y*6))        /* encode regs in int XXX */
#define ENCRD(x)	(x)			/* Encode dest reg in n_reg */

int COLORMAP(int c, int *r);
