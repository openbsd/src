/*
 * $LynxId: UCDefs.h,v 1.17 2009/03/10 20:02:44 tom Exp $
 *
 * Definitions for Unicode character-translations
 */

#ifndef UCDEFS_H
#define UCDEFS_H

#ifndef HTUTILS_H
#include <HTUtils.h>
#endif

typedef struct _LYUCcharset {
    int UChndl;			/* -1 for "old" charsets, >= 0 for chartrans tables */

    const char *MIMEname;
    int enc;
    int codepage;		/* if positive, an IBM OS/2 specific number;
				   if negative, flag for no table translation */

    /* parameters below are not used by chartrans mechanism, */
    /* they describe some relationships against built-in Latin1 charset... */
    int repertoire;		/* unused */
    int codepoints;		/* subset/superset of Latin1 ? */
    int cpranges;		/* unused, obsolete by LYlowest_eightbit;
				   "which ranges have valid displayable chars
				   (including nbsp and shy)" */
    int like8859;		/* currently used for nbsp and shy only
				   (but UCT_R_8859SPECL assumed for any UCT_R_8BIT...);
				   "for which ranges is it like 8859-1" */
} LYUCcharset;

typedef enum {
    UCT_ENC_7BIT,
    UCT_ENC_8BIT,
    UCT_ENC_8859,		/* no displayable chars in 0x80-0x9F */
    UCT_ENC_8BIT_C0,		/* 8-bit + some chars in C0 control area */
    UCT_ENC_MAYBE2022,
    UCT_ENC_CJK,
    UCT_ENC_16BIT,
    UCT_ENC_UTF8
} eUCT_ENC;

#define UCT_REP_SUBSETOF_LAT1 0x01
#define UCT_REP_SUPERSETOF_LAT1 0x02
#define UCT_REP_IS_LAT1 UCT_REP_SUBSETOF_LAT1 | UCT_REP_SUPERSETOF_LAT1
/*
 *  Assume everything we deal with is included in the UCS2 reperoire,
 *  so a flag for _REP_SUBSETOF_UCS2 would be redundant.
 */

/*
 *  More general description how the code points relate to 8859-1 and UCS:
 */
#define UCT_CP_SUBSETOF_LAT1 0x01	/* implies UCT_CP_SUBSETOF_UCS2 */
#define UCT_CP_SUPERSETOF_LAT1 0x02
#define UCT_CP_SUBSETOF_UCS2 0x04

#define UCT_CP_IS_LAT1 UCT_CP_SUBSETOF_LAT1 | UCT_CP_SUPERSETOF_LAT1

/*
 *  More specific bitflags for practically important code point ranges:
 */
#define UCT_R_LOWCTRL 0x08	/* 0x00-0x1F, for completeness */
#define UCT_R_7BITINV 0x10	/* invariant???, displayable 7bit chars */
#define UCT_R_7BITNAT 0x20	/* displayable 7bit, national??? */
#define UCT_R_HIGHCTRL 0x40	/* chars in 0x80-0x9F range */
#define UCT_R_8859SPECL 0x80	/* special chars in 8859-x sets: nbsp and shy */
#define UCT_R_HIGH8BIT 0x100	/* rest of 0xA0-0xFF range */

#define UCT_R_ASCII UCT_R_7BITINV | UCT_R_7BITNAT	/* displayable US-ASCII */
#define UCT_R_LAT1  UCT_R_ASCII   | UCT_R_8859SPECL | UCT_R_HIGH8BIT
#define UCT_R_8BIT  UCT_R_LAT1    | UCT_R_HIGHCTRL	/* full 8bit range */

/*
 *  For the following some comments are in HTAnchor.c.
 */
typedef enum {
    UCT_STAGE_MIME,
    UCT_STAGE_PARSER,		/* What the parser (SGML.c) gets to see */
    UCT_STAGE_STRUCTURED,	/* What the structured stream (HTML) gets fed */
    UCT_STAGE_HTEXT,		/* What gets fed to the HText_* functions */
    UCT_STAGEMAX
} eUCT_STAGE;

typedef enum {
    UCT_SETBY_NONE,
    UCT_SETBY_DEFAULT,
    UCT_SETBY_LINK,		/* set by A or LINK CHARSET= hint */
    UCT_SETBY_STRUCTURED,	/* structured stream stage (HTML.c) */
    UCT_SETBY_PARSER,		/* set by SGML parser or similar */
    UCT_SETBY_MIME		/* set explicitly by MIME charset parameter */
} eUCT_SETBY;

typedef struct _UCStageInfo {
    int lock;			/* by what it has been set */
    int LYhndl;
    LYUCcharset C;
} UCStageInfo;

typedef struct _UCAnchorInfo {
    struct _UCStageInfo s[UCT_STAGEMAX];
} UCAnchorInfo;

#endif /* UCDEFS_H */
