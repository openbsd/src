#ifndef _UC_KD_H
#define _UC_KD_H

/*
 *  NOTE: THE FOLLOWING #define MAY NEED ADJUSTMENT.
 *  u16 should be an unsigned type of 16 bit length (two octets).
 *  u8  should be an unsigned type of 8  bit length (one octet).
 */
#ifndef u16
#define u16 unsigned short
#endif /* u16 */


#ifndef u8
#define u8 unsigned char
#endif /* u8 */

#ifdef NOTDEFINED
struct consolefontdesc {
	u_short charcount;	/* characters in font (256 or 512) */
	u_short charheight;	/* scan lines per character (1-32) */
	char *chardata;		/* font data in expanded form */
};
#endif /* NOTDEFINED */
typedef char scrnmap_t;
#define		E_TABSZ		256

struct unipair {
	u16 unicode;
	u16 fontpos;
};
struct unipair_str {
	u16 unicode;
	CONST char * replace_str;
};
struct unimapdesc {
	u16 entry_ct;
	struct unipair *entries;
};
struct unimapdesc_str {
	u16 entry_ct;
	struct unipair_str *entries;
        int isdefault;
        int trydefault;
};


#define UNI_DIRECT_BASE 0xF000	/* start of Direct Font Region */
#define UNI_DIRECT_MASK 0x01FF	/* Direct Font Region bitmask */

#define UC_MAXLEN_ID_APPEND 20
#define UC_MAXLEN_MIMECSNAME 40
#define UC_MAXLEN_LYNXCSNAME 40
#define UC_LEN_LYNXCSNAME 20

#undef  EX_OK			/* may be defined in system headers */
#define EX_OK		0	/* successful termination */
#define EX_USAGE	64	/* command line usage error */
#define EX_DATAERR	65	/* data format error */
#define EX_NOINPUT	66	/* cannot open input */

#endif /* _UC_KD_H */
