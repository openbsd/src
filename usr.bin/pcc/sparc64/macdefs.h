/*	$OpenBSD: macdefs.h,v 1.2 2008/04/11 20:45:52 stefan Exp $	*/
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


/*
 * Many arithmetic instructions take 'reg_or_imm' in SPARCv9, where imm
 * means we can use a signed 13-bit constant (simm13). This gives us a
 * shortcut for small constants, instead of loading them into a register.
 * Special handling is required because 13 bits lies between SSCON and SCON.
 */
#define SIMM13(val) (val < 4096 && val > -4097)

/*
 * The SPARCv9 ABI specifies a stack bias of 2047 bits. This means that the
 * end of our call space is %fp+V9BIAS, working back towards %sp+V9BIAS+176.
 */
#define V9BIAS 2047

/*
 * The ABI requires that every frame reserve 176 bits for saving registers
 * in the case of a spill. The stack size must be 16-bit aligned.
 */
#define V9RESERVE 176
#define V9STEP(x) ALIGN(x, 0xf)
#define ALIGN(x, y) ((x & y) ? (x + y) & ~y : x)


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
#define SZLONG		64
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
#define ALLONG		64
#define ALLONGLONG	64
#define ALSHORT		16
#define ALPOINT		64
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
#define	MIN_LONGLONG	0x8000000000000000LL
#define	MAX_LONGLONG	0x7fffffffffffffffLL
#define	MAX_ULONGLONG	0xffffffffffffffffULL
#define	MIN_LONG	MIN_LONGLONG
#define	MAX_LONG	MAX_LONGLONG
#define	MAX_ULONG	MAX_ULONGLONG

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

#define	szty(t)	((ISPTR(t) || (t) == DOUBLE || \
	         (t) == LONG || (t) == ULONG || \
	         (t) == LONGLONG || (t) == ULONGLONG) ? 2 : 1)


/* Register names. */

#define MAXREGS (31 + 31 + 16 + 2)
#define NUMCLASS 4

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

#define F0 	31
#define F1 	32
#define F2 	33
#define F3 	34
#define F4 	35
#define F5 	36
#define F6 	37
#define F7 	38
#define F8 	39
#define F9 	40
#define F10	41
#define F11	42
#define F12	43
#define F13	44
#define F14	45
#define F15	46
#define F16	47
#define F17	48
#define F18	49
#define F19	50
#define F20	51
#define F21	52
#define F22	53
#define F23	54
#define F24	55
#define F25	56
#define F26	57
#define F27	58
#define F28	59
#define F29	60
#define F30	61
//define F31    XXX
#define D0	62
#define D1	63
#define D2	64
#define D3	65
#define D4	66
#define D5	67
#define D6	68
#define D7	69
#define D8	70
#define D9	71
#define D10	72
#define D11	73
#define D12	74
#define D13	75
#define D14	76
#define D15	77

#define SP 	78
#define FP 	79

#define FPREG 	FP

#define RETREG(x)	((x)==DOUBLE ? D0 : (x)==FLOAT ? F1 : O0)
#define RETREG_PRE(x)	((x)==DOUBLE ? D0 : (x)==FLOAT ? F1 : I0)

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
	/* 32-bit floating point */ \
		SBREG, SBREG, SBREG, SBREG, SBREG, SBREG, SBREG, SBREG, \
		SBREG, SBREG, SBREG, SBREG, SBREG, SBREG, SBREG, SBREG, \
		SBREG, SBREG, SBREG, SBREG, SBREG, SBREG, SBREG, SBREG, \
		SBREG, SBREG, SBREG, SBREG, SBREG, SBREG, SBREG, /*, SBREG */ \
	/* 64-bit floating point */ \
		SCREG, SCREG, SCREG, SCREG, SCREG, SCREG, SCREG, SCREG, \
		SCREG, SCREG, SCREG, SCREG, SCREG, SCREG, SCREG, SCREG, \
	/* sp */ SDREG, \
	/* fp */ SDREG

#define ROVERLAP \
	        { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, \
	{ -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, \
	{ -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, \
	{ -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, \
/* 32-bit floating point */ \
	{  D0, -1 }, {  D0, -1 }, {  D1, -1 }, {  D1, -1 }, \
	{  D2, -1 }, {  D2, -1 }, {  D3, -1 }, {  D3, -1 }, \
	{  D4, -1 }, {  D4, -1 }, {  D5, -1 }, {  D5, -1 }, \
	{  D6, -1 }, {  D6, -1 }, {  D7, -1 }, {  D7, -1 }, \
	{  D8, -1 }, {  D8, -1 }, {  D9, -1 }, {  D9, -1 }, \
	{ D10, -1 }, { D10, -1 }, { D11, -1 }, { D11, -1 }, \
	{ D12, -1 }, { D12, -1 }, { D13, -1 }, { D13, -1 }, \
	{ D14, -1 }, { D14, -1 }, { D15, -1 }, /* { D15, -1 }, */ \
/* 64-bit floating point */ \
	{  F0,  F1, -1 }, {  F2,  F3, -1 }, {  F4,  F5, -1 }, \
	{  F6,  F7, -1 }, {  F8,  F9, -1 }, { F10, F11, -1 }, \
	{ F12, F13, -1 }, { F14, F15, -1 }, { F16, F17, -1 }, \
	{ F18, F19, -1 }, { F20, F21, -1 }, { F22, F23, -1 }, \
	{ F24, F25, -1 }, { F26, F27, -1 }, { F28, F29, -1 }, \
	{ F30, /* F31, */ -1 }, \
	{ -1 }, \
	{ -1 }

#define GCLASS(x) 	(x <= I7                ? CLASSA : \
			(x <= F30               ? CLASSB : \
			(x <= D15               ? CLASSC : \
			(x == SP || x == FP     ? CLASSD : 0))))
#define PCLASS(p)	(1 << gclass((p)->n_type))
#define DECRA(x,y)	(((x) >> (y*7)) & 127)
#define ENCRA(x,y)	((x) << (7+y*7))
#define ENCRD(x)	(x)

int COLORMAP(int c, int *r);
