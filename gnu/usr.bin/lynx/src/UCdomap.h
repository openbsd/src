
#ifndef UCDOMAP_H
#define UCDOMAP_H

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
	u8 *			unicount,
	u16 *			unitable,
	int			nnuni,
	struct unimapdesc_str	replacedesc,
	int			lowest_eight,
	int			UC_rawuni));

char *UC_GNsetMIMEnames[4] =
	{"iso-8859-1", "x-dec-graphics", "cp437", "x-transparent"};

int UC_GNhandles[4] = {-1, -1, -1, -1};

struct UC_charset {
	CONST char *MIMEname;
	CONST char *LYNXname;
	u8* unicount;
	u16* unitable;
	int num_uni;
	struct unimapdesc_str replacedesc;
	int uc_status;
	int LYhndl;
	int GN;
	int lowest_eight;
	int enc;
};

extern int UCNumCharsets;

extern void UCInit NOARGS;

#endif /* UCDOMAP_H */
