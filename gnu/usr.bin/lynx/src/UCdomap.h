#ifndef UCDOMAP_H
#define UCDOMAP_H

#ifndef HTUTILS_H
#include <HTUtils.h>
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
 *  Lynx care of them internally.
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
static CONST struct unimapdesc_str dfont_replacedesc_fallback = {0,NULL,0,1};

#define UC_CHARSET_SETUP_euc_cn UC_Charset_Setup("euc-cn","Chinese",\
       NULL,NULL,0,dfont_replacedesc_fallback,\
       128,UCT_ENC_CJK,0)
#define UC_CHARSET_SETUP_euc_jp UC_Charset_Setup("euc-jp","Japanese (EUC-JP)",\
       NULL,NULL,0,dfont_replacedesc_fallback,\
       128,UCT_ENC_CJK,0)
#define UC_CHARSET_SETUP_shift_jis UC_Charset_Setup("shift_jis","Japanese (Shift_JIS)",\
       NULL,NULL,0,dfont_replacedesc_fallback,\
       128,UCT_ENC_CJK,0)
#define UC_CHARSET_SETUP_euc_kr UC_Charset_Setup("euc-kr","Korean",\
       NULL,NULL,0,dfont_replacedesc_fallback,\
       128,UCT_ENC_CJK,0)
#define UC_CHARSET_SETUP_big5 UC_Charset_Setup("big5","Taipei (Big5)",\
       NULL,NULL,0,dfont_replacedesc_fallback,\
       128,UCT_ENC_CJK,0)
  /*
   *  Placeholder for non-translation mode. - FM
   */
#define UC_CHARSET_SETUP_x_transparent UC_Charset_Setup("x-transparent","Transparent",\
       NULL,NULL,0,dfont_replacedesc_fallback,\
       128,1,0)

static CONST struct unimapdesc_str dfont_replacedesc_NO_fallback = {0,NULL,0,0};

#define UC_CHARSET_SETUP_utf_8 UC_Charset_Setup("utf-8","UNICODE (UTF-8)",\
       NULL,NULL,0,dfont_replacedesc_NO_fallback,\
       128,UCT_ENC_UTF8,0)


#endif /* UCDOMAP_H */
