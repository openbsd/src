/*			CJK character converter		HTCJK.h
**			=======================
**
**	Added 11-Jun-96 by FM, based on jiscode.h for
**	  Yutaka Sato's (ysato@etl.go.jp) SJIS.c, and
**	  Takuya ASADA's (asada@three-a.co.jp) CJK patches.
**	  (see SGML.c).
**
*/

#ifndef HTCJK_H
#define HTCJK_H

#ifndef HTUTILS_H
#include <HTUtils.h>
#endif

/*
**	STATUS CHANGE CODES
*/
#ifdef ESC
#undef ESC
#endif /* ESC */
#define ESC		CH_ESC  /* S/390 -- gil -- 0098 */
#define TO_2BCODE	'$'
#define TO_1BCODE	'('

#define TO_KANA		'\016'
#define TO_KANAOUT	'\017'

#define TO_KANJI	"\033$B"
#define TO_HANJI	"\033$A"
#define TO_HANGUL	"\033$(C"
#define TO_ASCII	"\033(B"

#define IS_SJIS_LO(lo)	((0x40<=lo)&&(lo!=0x7F)&&(lo<=0xFC))
#define IS_SJIS_HI1(hi) ((0x81<=hi)&&(hi<=0x9F))	/* 1st lev. */
#define IS_SJIS_HI2(hi) ((0xE0<=hi)&&(hi<=0xEF))	/* 2nd lev. */
#define IS_SJIS(hi,lo,in_sjis) (!IS_SJIS_LO(lo)?0:IS_SJIS_HI1(hi)?(in_sjis=1):in_sjis&&IS_SJIS_HI2(hi))

#define IS_EUC_LOS(lo)	((0x21<=lo)&&(lo<=0x7E))	/* standard */
#define IS_EUC_LOX(lo)	((0xA1<=lo)&&(lo<=0xFE))	/* extended */
#define IS_EUC_HI(hi)	((0xA1<=hi)&&(hi<=0xFE))
#define IS_EUC(hi,lo) IS_EUC_HI(hi) && (IS_EUC_LOS(lo) || IS_EUC_LOX(lo))

#define IS_BIG5_LOS(lo)	((0x40<=lo)&&(lo<=0x7E))	/* standard */
#define IS_BIG5_LOX(lo)	((0xA1<=lo)&&(lo<=0xFE))	/* extended */
#define IS_BIG5_HI(hi)	((0xA1<=hi)&&(hi<=0xFE))
#define IS_BIG5(hi,lo) IS_BIG5_HI(hi) && (IS_BIG5_LOS(lo) || IS_BIG5_LOX(lo))

typedef enum _HTkcode {NOKANJI, EUC, SJIS, JIS} HTkcode;
typedef enum _HTCJKlang {NOCJK, JAPANESE, CHINESE, KOREAN, TAIPEI} HTCJKlang;

/*
**  Function prototypes.
*/
extern void JISx0201TO0208_EUC PARAMS((
	register unsigned char		IHI,
	register unsigned char		ILO,
	register unsigned char *	OHI,
	register unsigned char *	OLO));

extern unsigned char * SJIS_TO_JIS1 PARAMS((
	register unsigned char		HI,
	register unsigned char		LO,
	register unsigned char *	JCODE));

extern unsigned char * JIS_TO_SJIS1 PARAMS((
	register unsigned char		HI,
	register unsigned char		LO,
	register unsigned char *	SJCODE));

extern unsigned char * EUC_TO_SJIS1 PARAMS((
	unsigned char			HI,
	unsigned char			LO,
	register unsigned char *	SJCODE));

extern void JISx0201TO0208_SJIS PARAMS((
	register unsigned char		I,
	register unsigned char *	OHI,
	register unsigned char *	OLO));

extern unsigned char * SJIS_TO_EUC1 PARAMS((
	unsigned char		HI,
	unsigned char		LO,
	unsigned char *		EUCp));

extern unsigned char * SJIS_TO_EUC PARAMS((
	unsigned char *		src,
	unsigned char *		dst));

extern unsigned char * EUC_TO_SJIS PARAMS((
	unsigned char *		src,
	unsigned char *		dst));

extern unsigned char * EUC_TO_JIS PARAMS((
	unsigned char *		src,
	unsigned char *		dst,
	CONST char *		toK,
	CONST char *		toA));

extern unsigned char * TO_EUC PARAMS((
	CONST unsigned char *	jis,
	unsigned char *		euc));

extern void TO_SJIS PARAMS((
	CONST unsigned char *	any,
	unsigned char *		sjis));

extern void TO_JIS PARAMS((
	CONST unsigned char *	any,
	unsigned char *		jis));

#endif /* HTCJK_H */
