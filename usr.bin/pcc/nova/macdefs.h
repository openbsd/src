/*	$OpenBSD: macdefs.h,v 1.1 2007/10/07 17:58:51 otto Exp $	*/
/*
 * Copyright (c) 2006 Anders Magnusson (ragge@ludd.luth.se).
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
 * Machine-dependent defines for Data General Nova.
 */

/*
 * Convert (multi-)character constant to integer.
 */
#define makecc(val,i)	lastcon = (lastcon<<8)|(val);

#define ARGINIT		16	/* adjusted in MD code */
#define AUTOINIT	16	/* adjusted in MD code */

/*
 * Storage space requirements
 */
#define SZCHAR		8
#define SZINT		16
#define SZFLOAT		32
#define SZDOUBLE	64
#define SZLDOUBLE	64
#define SZLONG		32
#define SZSHORT		16
#define SZLONGLONG	32
#define SZPOINT(t)	16	/* Actually 15 */

/*
 * Alignment constraints
 */
#define ALCHAR		8
#define ALINT		16
#define ALFLOAT		16
#define ALDOUBLE	16
#define ALLDOUBLE	16
#define ALLONG		16
#define ALLONGLONG	16
#define ALSHORT		16
#define ALPOINT		16
#define ALSTRUCT	16
#define ALSTACK		16 

/*
 * Min/max values.
 */
#define	MIN_CHAR	-128
#define	MAX_CHAR	127
#define	MAX_UCHAR	255
#define	MIN_SHORT	-32768
#define	MAX_SHORT	32767
#define	MAX_USHORT	65535
#define	MIN_INT		MIN_SHORT
#define	MAX_INT		MAX_SHORT
#define	MAX_UNSIGNED	MAX_USHORT
#define	MIN_LONG	0x80000000L
#define	MAX_LONG	0x7fffffffL
#define	MAX_ULONG	0xffffffffUL
#define	MIN_LONGLONG	MIN_LONG
#define	MAX_LONGLONG	MAX_LONG
#define	MAX_ULONGLONG	MAX_ULONG

/* Default char is unsigned */
#define	CHAR_UNSIGNED

/*
 * Use large-enough types.
 */
typedef	long CONSZ;
typedef	unsigned long U_CONSZ;
typedef long OFFSZ;

#define CONFMT	"%ld"		/* format for printing constants */
#define LABFMT	".L%d"		/* format for printing labels */
#define	STABLBL	".LL%d"		/* format for stab (debugging) labels */
#ifdef FORTRAN
#define XL 8
#define	FLABELFMT "%s:\n"
#define USETEXT ".text"
#define USECONST ".data\t0" 	/* XXX - fix */
#define USEBSS  ".data\t1" 	/* XXX - fix */
#define USEINIT ".data\t2" 	/* XXX - fix */
#define MAXREGVAR 3             /* XXX - fix */
#define BLANKCOMMON "_BLNK_"
#define MSKIREG  (M(TYSHORT)|M(TYLONG))
#define TYIREG TYLONG
#define FSZLENG  FSZLONG
#define FUDGEOFFSET 1
#define	AUTOREG	EBP
#define	ARGREG	EBP
#define ARGOFFSET 4
#endif

#define BACKAUTO 		/* stack grows negatively for automatics */
#define BACKTEMP 		/* stack grows negatively for temporaries */

#define	MYP2TREE(p) myp2tree(p);

#undef	FIELDOPS		/* no bit-field instructions */
#define	RTOLBYTES		/* bytes are numbered right to left */

#define ENUMSIZE(high,low) INT	/* enums are always stored in full int */

/* Definitions mostly used in pass2 */

#define BYTEOFF(x)	((x)&01)
#define wdal(k)		(BYTEOFF(k)==0)
#define BITOOR(x)	(x)	/* bit offset to oreg offset XXX die! */

#define STOARG(p)
#define STOFARG(p)
#define STOSTARG(p)
#define genfcall(a,b)	gencall(a,b)

#define	szty(t)	(((t) == DOUBLE || (t) == LDOUBLE) ? 4 : \
	((t) == LONGLONG || (t) == ULONGLONG || \
	 (t) == LONG || (t) == ULONG) ? 2 : 1)

/*
 * The Nova has three register classes.  Note that the space used in 
 * zero page is considered registers.
 * Register 28 and 29 are FP and SP.
 *
 * The classes used on Nova are:
 *	A - AC0-AC3 (as non-index registers)	: reg 0-3
 *	B - AC2-AC3 (as index registers)	: reg 2-3
 *	C - address 50-77 in memory		: reg 4-27
 */
#define	MAXREGS	30	/* 0-29 */

#define	RSTATUS	\
	SAREG|TEMPREG, SAREG|TEMPREG, SAREG|SBREG|TEMPREG, SAREG|SBREG|TEMPREG,\
	SCREG|TEMPREG, SCREG|TEMPREG, SCREG|TEMPREG, SCREG|TEMPREG,	\
	SCREG|TEMPREG, SCREG|TEMPREG, SCREG|TEMPREG, SCREG|TEMPREG,	\
	SCREG|PERMREG, SCREG|PERMREG, SCREG|PERMREG, SCREG|PERMREG,	\
	SCREG|PERMREG, SCREG|PERMREG, SCREG|PERMREG, SCREG|PERMREG,	\
	SCREG|PERMREG, SCREG|PERMREG, SCREG|PERMREG, SCREG|PERMREG,	\
	SCREG|PERMREG, SCREG|PERMREG, SCREG|PERMREG, SCREG|PERMREG,	\
	0,	0

#define	ROVERLAP \
	{ -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 },	\
	{ -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 },	\
	{ -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 },	\
	{ -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 },


/* Return a register class based on the type of the node */
/* all types in all classes */
#define PCLASS(p) (SAREG|SBREG|SCREG)

#define	NUMCLASS 	4	/* highest number of reg classes used */
				/* XXX - must be 4 */

int COLORMAP(int c, int *r);
#define	GCLASS(x) (x < 4 ? CLASSA : CLASSC)
#define DECRA(x,y)	(((x) >> (y*6)) & 63)	/* decode encoded regs */
#define	ENCRD(x)	(x)		/* Encode dest reg in n_reg */
#define ENCRA1(x)	((x) << 6)	/* A1 */
#define ENCRA2(x)	((x) << 12)	/* A2 */
#define ENCRA(x,y)	((x) << (6+y*6))	/* encode regs in int */
#define	RETREG(x)	(0) /* ? Sanity */

/* XXX - to die */
#define FPREG	28	/* frame pointer */
#define STKREG	29	/* stack pointer */

#define MYREADER(p) myreader(p)
#define MYCANON(p) mycanon(p)
#define	MYOPTIM
