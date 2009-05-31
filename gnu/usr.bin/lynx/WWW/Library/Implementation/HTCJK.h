/*			CJK character converter		HTCJK.h
 *			=======================
 *
 *	Added 11-Jun-96 by FM, based on jiscode.h for
 *	  Yutaka Sato's (ysato@etl.go.jp) SJIS.c, and
 *	  Takuya ASADA's (asada@three-a.co.jp) CJK patches.
 *	  (see SGML.c).
 *
 */

#ifndef HTCJK_H
#define HTCJK_H

#ifndef HTUTILS_H
#include <HTUtils.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif
/*
 *	STATUS CHANGE CODES
 */
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
#define IS_SJIS_2BYTE(hi,lo) (IS_SJIS_LO(lo)&&(IS_SJIS_HI1(hi)||IS_SJIS_HI2(hi)))
#define IS_SJIS_X0201KANA(lo) ((0xA1<=lo)&&(lo<=0xDF))
#define IS_EUC_LOX(lo)	((0xA1<=lo)&&(lo<=0xFE))	/* extended */
#define IS_EUC_HI(hi)	((0xA1<=hi)&&(hi<=0xFE))
#define IS_EUC_X0201KANA(hi,lo) ((hi==0x8E)&&(0xA1<=lo)&&(lo<=0xDF))
#define IS_EUC(hi,lo) ((IS_EUC_HI(hi) && IS_EUC_LOX(lo))||IS_EUC_X0201KANA(hi,lo))
#define IS_JAPANESE_2BYTE(hi,lo) (IS_SJIS_2BYTE(hi,lo) || IS_EUC(hi,lo))
#define IS_BIG5_LOS(lo)	((0x40<=lo)&&(lo<=0x7E))	/* standard */
#define IS_BIG5_LOX(lo)	((0xA1<=lo)&&(lo<=0xFE))	/* extended */
#define IS_BIG5_HI(hi)	((0xA1<=hi)&&(hi<=0xFE))
#define IS_BIG5(hi,lo) (IS_BIG5_HI(hi) && (IS_BIG5_LOS(lo) || IS_BIG5_LOX(lo)))
    typedef enum {
	NOKANJI = 0, EUC, SJIS, JIS
    } HTkcode;
    typedef enum {
	NOCJK = 0, JAPANESE, CHINESE, KOREAN, TAIPEI
    } HTCJKlang;

    extern HTCJKlang HTCJK;

/*
 *  Function prototypes.
 */
    extern void JISx0201TO0208_EUC(register unsigned char IHI,
				   register unsigned char ILO,
				   register unsigned char *OHI,
				   register unsigned char *OLO);

    extern unsigned char *SJIS_TO_JIS1(register unsigned char HI,
				       register unsigned char LO,
				       register unsigned char *JCODE);

    extern unsigned char *JIS_TO_SJIS1(register unsigned char HI,
				       register unsigned char LO,
				       register unsigned char *SJCODE);

    extern unsigned char *EUC_TO_SJIS1(unsigned char HI,
				       unsigned char LO,
				       register unsigned char *SJCODE);

    extern void JISx0201TO0208_SJIS(register unsigned char I,
				    register unsigned char *OHI,
				    register unsigned char *OLO);

    extern unsigned char *SJIS_TO_EUC1(unsigned char HI,
				       unsigned char LO,
				       unsigned char *EUCp);

    extern unsigned char *SJIS_TO_EUC(unsigned char *src,
				      unsigned char *dst);

    extern unsigned char *EUC_TO_SJIS(unsigned char *src,
				      unsigned char *dst);

    extern unsigned char *EUC_TO_JIS(unsigned char *src,
				     unsigned char *dst,
				     const char *toK,
				     const char *toA);

    extern unsigned char *TO_EUC(const unsigned char *jis,
				 unsigned char *euc);

    extern void TO_SJIS(const unsigned char *any,
			unsigned char *sjis);

    extern void TO_JIS(const unsigned char *any,
		       unsigned char *jis);

    extern char *str_kcode(HTkcode code);

#ifdef __cplusplus
}
#endif
#endif				/* HTCJK_H */
