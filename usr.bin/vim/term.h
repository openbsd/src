/*	$OpenBSD: term.h,v 1.1.1.1 1996/09/07 21:40:27 downsj Exp $	*/
/* vi:set ts=4 sw=4:
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 */

/*
 * This file contains the machine dependent escape sequences that the editor
 * needs to perform various operations. Some of the sequences here are
 * optional. Anything not available should be indicated by a null string. In
 * the case of insert/delete line sequences, the editor checks the capability
 * and works around the deficiency, if necessary.
 */

#ifdef SASC
/*
 * the SAS C compiler has a bug that makes typedefs being forgot sometimes
 */
typedef unsigned char char_u;
#endif

/*
 * Index of the termcap codes in the term_strings array.
 */
enum SpecialKeys
{
	KS_NAME = 0,		/* name of this terminal entry */
	KS_CE,		/* clear to end of line */
	KS_AL,		/* add new blank line */
	KS_CAL,		/* add number of blank lines */
	KS_DL,		/* delete line */
	KS_CDL,		/* delete number of lines */
	KS_CS,		/* scroll region */
	KS_CL,		/* clear screen */
	KS_CD,		/* clear to end of display */
	KS_DA,		/* text may be scrolled down from up */
	KS_DB,		/* text may be scrolled up from down */
	KS_VI,		/* cursor invisible */
	KS_VE,		/* cursor visible */
	KS_VS,		/* cursor very visible */
	KS_ME,		/* normal mode */
	KS_MR,		/* reverse mode */
	KS_MD,		/* bold mode */
	KS_SE,		/* normal mode */
	KS_SO,		/* standout mode */
	KS_CZH,		/* italic mode start */
	KS_CZR,		/* italic mode end */
	KS_UE,		/* exit underscore mode */
	KS_US,		/* underscore mode */
	KS_MS,		/* save to move cur in reverse mode */
	KS_CM,		/* cursor motion */
	KS_SR,		/* scroll reverse (backward) */
	KS_CRI,		/* cursor number of chars right */
	KS_VB,		/* visual bell */
	KS_KS,		/* put term in "keypad transmit" mode */
	KS_KE,		/* out of "keypad transmit" mode */
	KS_TI,		/* put terminal in termcap mode */
	KS_TE,		/* out of termcap mode */
	
	KS_CSC		/* cur is relative to scroll region */
};

#define KS_LAST		KS_CSC

/*
 * the terminal capabilities are stored in this array
 * IMPORTANT: When making changes, note the following:
 * - there should be an entry for each code in the builtin termcaps
 * - there should be an option for each code in option.c
 * - there should be code in term.c to obtain the value from the termcap
 */

extern char_u *(term_strings[]);	/* current terminal strings */

/*
 * strings used for terminal
 */
#define T_CE	(term_strings[KS_CE])	/* clear to end of line */
#define T_AL	(term_strings[KS_AL])	/* add new blank line */
#define T_CAL	(term_strings[KS_CAL])	/* add number of blank lines */
#define T_DL	(term_strings[KS_DL])	/* delete line */
#define T_CDL	(term_strings[KS_CDL])	/* delete number of lines */
#define T_CS	(term_strings[KS_CS])	/* scroll region */
#define T_CL	(term_strings[KS_CL])	/* clear screen */
#define T_CD	(term_strings[KS_CD])	/* clear to end of display */
#define T_DA	(term_strings[KS_DA])	/* text may be scrolled down from up */
#define T_DB	(term_strings[KS_DB])	/* text may be scrolled up from down */
#define T_VI	(term_strings[KS_VI])	/* cursor invisible */
#define T_VE	(term_strings[KS_VE])	/* cursor visible */
#define T_VS	(term_strings[KS_VS])	/* cursor very visible */
#define T_ME	(term_strings[KS_ME])	/* normal mode */
#define T_MR	(term_strings[KS_MR])	/* reverse mode */
#define T_MD	(term_strings[KS_MD])	/* bold mode */
#define T_SE	(term_strings[KS_SE])	/* normal mode */
#define T_SO	(term_strings[KS_SO])	/* standout mode */
#define T_CZH	(term_strings[KS_CZH])	/* italic mode start */
#define T_CZR	(term_strings[KS_CZR])	/* italic mode end */
#define T_UE	(term_strings[KS_UE])	/* exit underscore mode */
#define T_US	(term_strings[KS_US])	/* underscore mode */
#define T_MS	(term_strings[KS_MS])	/* save to move cur in reverse mode */
#define T_CM	(term_strings[KS_CM])	/* cursor motion */
#define T_SR	(term_strings[KS_SR])	/* scroll reverse (backward) */
#define T_CRI	(term_strings[KS_CRI])	/* cursor number of chars right */
#define T_VB	(term_strings[KS_VB])	/* visual bell */
#define T_KS	(term_strings[KS_KS])	/* put term in "keypad transmit" mode */
#define T_KE	(term_strings[KS_KE])	/* out of "keypad transmit" mode */
#define T_TI	(term_strings[KS_TI])	/* put terminal in termcap mode */
#define T_TE	(term_strings[KS_TE])	/* out of termcap mode */
#define T_CSC	(term_strings[KS_CSC])	/* cur is relative to scroll region */
