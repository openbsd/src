/*
 * This file is the general header file for all parts
 * of the MicroEMACS display editor. It contains all of the
 * general definitions and macros. It also contains some
 * conditional compilation flags. All of the per-system and
 * per-terminal definitions are in special header files.
 * The most common reason to edit this file would be to zap
 * the definition of CVMVAS or BACKUP.
 */
#include	"sysdef.h"		/* Order is critical.		*/
#include	"ttydef.h"
#include	"chrdef.h"

/*
 * If your system and/or compiler does not support the "void" type
 * then define NO_VOID_TYPE in sysdef.h.  In the absence of some
 * other definition for VOID, the default in that case will be to
 * turn it into an int, which works with most compilers that don't
 * support void.  In the absence of any definition of VOID or
 * NO_VOID_TYPE, the default is to assume void is supported, which
 * should be the case for most modern C compilers.
 */

#ifdef NO_VOID_TYPE
#  undef VOID
#  define VOID int			/* Default for no void is int */
#else
#ifndef VOID
#  define VOID void			/* Just use normal void */
#endif /* VOID */
#endif /* NO_VOID_TYPE */

#ifdef	NO_MACRO
#ifndef NO_STARTUP
#define NO_STARTUP			/* NO_MACRO implies NO_STARTUP */
#endif
#endif

typedef int (*PF)();			/* generaly useful type */

/*
 * Table sizes, etc.
 */
#define NFILEN	80			/* Length, file name.		*/
#define NBUFN	24			/* Length, buffer name.		*/
#define NLINE	256			/* Length, line.		*/
#define PBMODES 4			/* modes per buffer		*/
#define NKBDM	256			/* Length, keyboard macro.	*/
#define NPAT	80			/* Length, pattern.		*/
#define HUGE	1000			/* A rather large number.	*/
#define NSRCH	128			/* Undoable search commands.	*/
#define NXNAME	64			/* Length, extended command.	*/
#define NKNAME	20			/* Length, key names		*/
/*
 * Universal.
 */
#define FALSE	0			/* False, no, bad, etc.		*/
#define TRUE	1			/* True, yes, good, etc.	*/
#define ABORT	2			/* Death, ^G, abort, etc.	*/

#define KPROMPT 2			/* keyboard prompt		*/

/*
 * These flag bits keep track of
 * some aspects of the last command. The CFCPCN
 * flag controls goal column setting. The CFKILL
 * flag controls the clearing versus appending
 * of data in the kill buffer.
 */
#define CFCPCN	0x0001			/* Last command was C-P, C-N	*/
#define CFKILL	0x0002			/* Last command was a kill	*/
#define CFINS	0x0004			/* Last command was self-insert */

/*
 * File I/O.
 */
#define FIOSUC	0			/* Success.			*/
#define FIOFNF	1			/* File not found.		*/
#define FIOEOF	2			/* End of file.			*/
#define FIOERR	3			/* Error.			*/
#define FIOLONG 4			/* long line partially read	*/

/*
 * Directory I/O.
 */
#define DIOSUC	0			/* Success.			*/
#define DIOEOF	1			/* End of file.			*/
#define DIOERR	2			/* Error.			*/

/*
 * Display colors.
 */
#define CNONE	0			/* Unknown color.		*/
#define CTEXT	1			/* Text color.			*/
#define CMODE	2			/* Mode line color.		*/

/* Flags for keyboard involked functions */

#define FFUNIV		1		/* universal argument		*/
#define FFNEGARG	2		/* negitive only argument	*/
#define FFOTHARG	4		/* other argument		*/
#define FFARG		7		/* any argument			*/
#define FFRAND		8		/* Called by other function	*/

/*
 * Flags for "eread".
 */
#define EFFUNC	0x0001			/* Autocomplete functions.	*/
#define EFBUF	0x0002			/* Autocomplete buffers.	*/
#define EFFILE	0x0004			/* " files (maybe someday)	*/
#define EFAUTO	0x0007			/* Some autocompleteion on	*/
#define EFNEW	0x0008			/* New prompt.			*/
#define EFCR	0x0010			/* Echo CR at end; last read.	*/

/*
 * Flags for "ldelete"/"kinsert"
 */

#define KNONE	0
#define KFORW	1
#define KBACK	2

/*
 * All text is kept in circularly linked
 * lists of "LINE" structures. These begin at the
 * header line (which is the blank line beyond the
 * end of the buffer). This line is pointed to by
 * the "BUFFER". Each line contains a the number of
 * bytes in the line (the "used" size), the size
 * of the text array, and the text. The end of line
 * is not stored as a byte; it's implied. Future
 * additions will include update hints, and a
 * list of marks into the line.
 */
typedef struct	LINE {
	struct	LINE *l_fp;		/* Link to the next line	*/
	struct	LINE *l_bp;		/* Link to the previous line	*/
	int	l_size;			/* Allocated size		*/
	int	l_used;			/* Used size			*/
#ifndef ZEROARRAY
	char	l_text[1];		/* A bunch of characters.	*/
#else
	char	l_text[];		/* A bunch of characters.	*/
#endif
}	LINE;

/*
 * The rationale behind these macros is that you
 * could (with some editing, like changing the type of a line
 * link from a "LINE *" to a "REFLINE", and fixing the commands
 * like file reading that break the rules) change the actual
 * storage representation of lines to use something fancy on
 * machines with small address spaces.
 */
#define lforw(lp)	((lp)->l_fp)
#define lback(lp)	((lp)->l_bp)
#define lgetc(lp, n)	(CHARMASK((lp)->l_text[(n)]))
#define lputc(lp, n, c) ((lp)->l_text[(n)]=(c))
#define llength(lp)	((lp)->l_used)
#define ltext(lp)	((lp)->l_text)

/*
 * All repeated structures are kept as linked lists of structures.
 * All of these start with a LIST structure (except lines, which
 * have their own abstraction). This will allow for
 * later conversion to generic list manipulation routines should
 * I decide to do that. it does mean that there are four extra
 * bytes per window. I feel that this is an acceptable price,
 * considering that there are usually only one or two windows.
 */
typedef struct LIST {
	union {
		struct MGWIN	*l_wp;
		struct BUFFER	*x_bp;	/* l_bp is used by LINE */
		struct LIST	*l_nxt;
	} l_p;
	char	*l_name;
} LIST;
/*
 * Usual hack - to keep from uglifying the code with lotsa
 * references through the union, we #define something for it.
 */
#define l_next	l_p.l_nxt

/*
 * There is a window structure allocated for
 * every active display window. The windows are kept in a
 * big list, in top to bottom screen order, with the listhead at
 * "wheadp". Each window contains its own values of dot and mark.
 * The flag field contains some bits that are set by commands
 * to guide redisplay; although this is a bit of a compromise in
 * terms of decoupling, the full blown redisplay is just too
 * expensive to run for every input character.
 */
typedef struct	MGWIN {
	LIST	w_list;			/* List header		       */
	struct	BUFFER *w_bufp;		/* Buffer displayed in window	*/
	struct	LINE *w_linep;		/* Top line in the window	*/
	struct	LINE *w_dotp;		/* Line containing "."		*/
	struct	LINE *w_markp;		/* Line containing "mark"	*/
	int	w_doto;			/* Byte offset for "."		*/
	int	w_marko;		/* Byte offset for "mark"	*/
	char	w_toprow;		/* Origin 0 top row of window	*/
	char	w_ntrows;		/* # of rows of text in window	*/
	char	w_force;		/* If NZ, forcing row.		*/
	char	w_flag;			/* Flags.			*/
}	MGWIN;
#define w_wndp	w_list.l_p.l_wp
#define w_name	w_list.l_name

/*
 * Window flags are set by command processors to
 * tell the display system what has happened to the buffer
 * mapped by the window. Setting "WFHARD" is always a safe thing
 * to do, but it may do more work than is necessary. Always try
 * to set the simplest action that achieves the required update.
 * Because commands set bits in the "w_flag", update will see
 * all change flags, and do the most general one.
 */
#define WFFORCE 0x01			/* Force reframe.		*/
#define WFMOVE	0x02			/* Movement from line to line.	*/
#define WFEDIT	0x04			/* Editing within a line.	*/
#define WFHARD	0x08			/* Better to a full display.	*/
#define WFMODE	0x10			/* Update mode line.		*/

/*
 * Text is kept in buffers. A buffer header, described
 * below, exists for every buffer in the system. The buffers are
 * kept in a big list, so that commands that search for a buffer by
 * name can find the buffer header. There is a safe store for the
 * dot and mark in the header, but this is only valid if the buffer
 * is not being displayed (that is, if "b_nwnd" is 0). The text for
 * the buffer is kept in a circularly linked list of lines, with
 * a pointer to the header line in "b_linep".
 */
typedef struct	BUFFER {
	LIST	b_list;			/* buffer list pointer		*/
	struct	BUFFER *b_altb;		/* Link to alternate buffer	*/
	struct	LINE *b_dotp;		/* Link to "." LINE structure	*/
	struct	LINE *b_markp;		/* ditto for mark		*/
	struct	LINE *b_linep;		/* Link to the header LINE	*/
	struct	MAPS_S *b_modes[PBMODES]; /* buffer modes		*/
	int	b_doto;			/* Offset of "." in above LINE	*/
	int	b_marko;		/* ditto for the "mark"		*/
	short	b_nmodes;		/* number of non-fundamental modes */
	char	b_nwnd;			/* Count of windows on buffer	*/
	char	b_flag;			/* Flags			*/
	char	b_fname[NFILEN];	/* File name			*/
	struct	fileinfo b_fi;		/* File attributes		*/
}	BUFFER;
#define b_bufp	b_list.l_p.x_bp
#define b_bname b_list.l_name

#define BFCHG	0x01			/* Changed.			*/
#define BFBAK	0x02			/* Need to make a backup.	*/
#ifdef	NOTAB
#define BFNOTAB 0x04			/* no tab mode			*/
#endif
#define BFOVERWRITE 0x08		/* overwrite mode		*/

/*
 * This structure holds the starting position
 * (as a line/offset pair) and the number of characters in a
 * region of a buffer. This makes passing the specification
 * of a region around a little bit easier.
 */
typedef struct	{
	struct	LINE *r_linep;		/* Origin LINE address.		*/
	int	r_offset;		/* Origin LINE offset.		*/
	RSIZE	r_size;			/* Length in characters.	*/
}	REGION;

/*
 * Externals.
 */
extern	int	thisflag;
extern	int	lastflag;
extern	int	curgoal;
extern	int	epresf;
extern	int	sgarbf;
extern	int	mode;
extern	MGWIN	*curwp;
extern	BUFFER	*curbp;
extern	MGWIN	*wheadp;
extern	BUFFER	*bheadp;
extern	char	pat[];
extern	BUFFER	*bfind();
extern	MGWIN	*popbuf();
extern	MGWIN	*wpopup();
extern	LINE	*lalloc();
extern	LINE	*lallocx();
extern	VOID	ewprintf();
extern	int	nrow;
extern	int	ncol;
extern	int	ttrow;
extern	int	ttcol;
extern	int	tceeol;
extern	int	tcinsl;
extern	int	tcdell;
extern	char	cinfo[];
extern	char	*keystrings[];
extern	VOID	update();
extern	char	*keyname();
extern	char	*adjustname();
extern	VOID	kdelete();
extern	VOID	lchange();
