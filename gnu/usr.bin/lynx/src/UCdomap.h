#ifndef UCDOMAP_H
#define UCDOMAP_H

#ifndef HTUTILS_H
#include <HTUtils.h>
#endif

#ifndef ALL_CHARSETS
#define ALL_CHARSETS 1
#endif

#include <UCkd.h>

/*
 *  [old comments: - KW ]
 *  consolemap.h
 *
 *  Interface between console.c, selection.c  and UCmap.c
 */
#define LAT1_MAP 0
#define GRAF_MAP 1
#define IBMPC_MAP 2
#define USER_MAP 3

/*
 *  Some conventions I try to follow (loosely):
 *	[a-z]* only internal, names from linux driver code.
 *	UC_* to be only known internally.
 *	UC[A-Z]* to be exported to other parts of Lynx. -KW
 */
extern void UC_Charset_Setup PARAMS((
	CONST char *		UC_MIMEcharset,
	CONST char *		UC_LYNXcharset,
	CONST u8 *		unicount,
	CONST u16 *		unitable,
	int			nnuni,
	struct unimapdesc_str	replacedesc,
	int			lowest_eight,
	int			UC_rawuni,
	int			codepage));

CONST char *UC_GNsetMIMEnames[4] =
	{"iso-8859-1", "x-dec-graphics", "cp437", "x-transparent"};

int UC_GNhandles[4] = {-1, -1, -1, -1};

struct UC_charset {
	CONST char *MIMEname;
	CONST char *LYNXname;
	CONST u8* unicount;
	CONST u16* unitable;
	int num_uni;
	struct unimapdesc_str replacedesc;
	int uc_status;
	int LYhndl;
	int GN;
	int lowest_eight;
	int enc;
	int codepage;	/* codepage number, used by OS/2 font-switching code */
};

extern int UCNumCharsets;

extern void UCInit NOARGS;


/*
 *  INSTRUCTIONS for adding new character sets which do not have
 *              Unicode tables.
 *
 *  Several #defines below are declarations for charsets which need no
 *  tables for mapping to Unicode - CJK multibytes, x-transparent, UTF8 -
 *  Lynx takes care of them internally.
 *
 *  The declaration's format is kept in chrtrans/XXX_uni.h -
 *  keep this in mind when changing ucmaketbl.c,
 *  see also UC_Charset_Setup() above for details.
 */

  /*
   *  There is no strict correlation for the next five, since the transfer
   *  charset gets decoded into Display Char Set by the CJK code (separate
   *  from Unicode mechanism).  For now we use the MIME name that describes
   *  what is output to the terminal. - KW
   */

/*----------------------------------------------------------------------------*/

#ifndef NO_CHARSET_euc_cn
#define NO_CHARSET_euc_cn !ALL_CHARSETS
#endif

#if NO_CHARSET_euc_cn
#define UC_CHARSET_SETUP_euc_cn /*nothing*/
#else
#define UC_CHARSET_SETUP_euc_cn UC_Charset_NoUctb_Setup("euc-cn","Chinese",\
       1, 128,UCT_ENC_CJK,0)
#endif

/*----------------------------------------------------------------------------*/

#ifndef NO_CHARSET_euc_jp
#define NO_CHARSET_euc_jp !ALL_CHARSETS
#endif

#if NO_CHARSET_euc_jp
#define UC_CHARSET_SETUP_euc_jp /*nothing*/
#else
#define UC_CHARSET_SETUP_euc_jp UC_Charset_NoUctb_Setup("euc-jp","Japanese (EUC-JP)",\
       1, 128,UCT_ENC_CJK,0)
#endif

/*----------------------------------------------------------------------------*/

#ifndef NO_CHARSET_shift_jis
#define NO_CHARSET_shift_jis !ALL_CHARSETS
#endif

#if NO_CHARSET_shift_jis
#define UC_CHARSET_SETUP_shift_jis /*nothing*/
#else
#define UC_CHARSET_SETUP_shift_jis UC_Charset_NoUctb_Setup("shift_jis","Japanese (Shift_JIS)",\
       1, 128,UCT_ENC_CJK,0)
#endif

/*----------------------------------------------------------------------------*/

#ifndef NO_CHARSET_euc_kr
#define NO_CHARSET_euc_kr !ALL_CHARSETS
#endif

#if NO_CHARSET_euc_kr
#define UC_CHARSET_SETUP_euc_kr /*nothing*/
#else
#define UC_CHARSET_SETUP_euc_kr UC_Charset_NoUctb_Setup("euc-kr","Korean",\
       1, 128,UCT_ENC_CJK,0)
#endif

/*----------------------------------------------------------------------------*/

#ifndef NO_CHARSET_big5
#define NO_CHARSET_big5 !ALL_CHARSETS
#endif

#if NO_CHARSET_big5
#define UC_CHARSET_SETUP_big5 /*nothing*/
#else
#define UC_CHARSET_SETUP_big5 UC_Charset_NoUctb_Setup("big5","Taipei (Big5)",\
       1, 128,UCT_ENC_CJK,0)
#endif

/*----------------------------------------------------------------------------*/

  /*
   *  Placeholder for non-translation mode. - FM
   */

#ifndef NO_CHARSET_x_transparent
#define NO_CHARSET_x_transparent !ALL_CHARSETS
#endif

#if NO_CHARSET_x_transparent
#define UC_CHARSET_SETUP_x_transparent /*nothing*/
#else
#define UC_CHARSET_SETUP_x_transparent UC_Charset_NoUctb_Setup("x-transparent","Transparent",\
       0, 128,UCT_ENC_8BIT,0)
#endif

/*----------------------------------------------------------------------------*/

#ifndef NO_CHARSET_utf_8
#define NO_CHARSET_utf_8 !ALL_CHARSETS
#endif

#if NO_CHARSET_utf_8
#define UC_CHARSET_SETUP_utf_8 /*nothing*/
#else
#define UC_CHARSET_SETUP_utf_8 UC_Charset_NoUctb_Setup("utf-8","UNICODE (UTF-8)",\
       0, 128,UCT_ENC_UTF8,-4)
#endif


#endif /* UCDOMAP_H */
