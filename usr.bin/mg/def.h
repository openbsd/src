/*	$OpenBSD: def.h,v 1.27 2002/02/14 13:41:44 deraadt Exp $	*/

/*
 * This file is the general header file for all parts
 * of the Mg display editor. It contains all of the
 * general definitions and macros. It also contains some
 * conditional compilation flags. All of the per-system and
 * per-terminal definitions are in special header files.
 * The most common reason to edit this file would be to zap
 * the definition of CVMVAS or BACKUP.
 */
#include	"sysdef.h"	/* Order is critical.		 */
#include	"ttydef.h"
#include	"chrdef.h"

#ifdef	NO_MACRO
#ifndef NO_STARTUP
#define NO_STARTUP		/* NO_MACRO implies NO_STARTUP */
#endif
#endif

typedef int	(*PF)();	/* generally useful type */

/*
 * Table sizes, etc.
 */
#define NFILEN	1024		/* Length, file name.		 */
#define NBUFN	NFILEN		/* Length, buffer name.		 */
#define NLINE	256		/* Length, line.		 */
#define PBMODES 4		/* modes per buffer		 */
#define NKBDM	256		/* Length, keyboard macro.	 */
#define NPAT	80		/* Length, pattern.		 */
#define HUGE	1000		/* A rather large number.	 */
#define NSRCH	128		/* Undoable search commands.	 */
#define NXNAME	64		/* Length, extended command.	 */
#define NKNAME	20		/* Length, key names		 */
/*
 * Universal.
 */
#define FALSE	0		/* False, no, bad, etc.		 */
#define TRUE	1		/* True, yes, good, etc.	 */
#define ABORT	2		/* Death, ^G, abort, etc.	 */

#define KPROMPT 2		/* keyboard prompt		 */

/*
 * These flag bits keep track of
 * some aspects of the last command. The CFCPCN
 * flag controls goal column setting. The CFKILL
 * flag controls the clearing versus appending
 * of data in the kill buffer.
 */
#define CFCPCN	0x0001		/* Last command was C-P, C-N	 */
#define CFKILL	0x0002		/* Last command was a kill	 */
#define CFINS	0x0004		/* Last command was self-insert */

/*
 * File I/O.
 */
#define FIOSUC	0		/* Success.			 */
#define FIOFNF	1		/* File not found.		 */
#define FIOEOF	2		/* End of file.			 */
#define FIOERR	3		/* Error.			 */
#define FIOLONG 4		/* long line partially read	 */

/*
 * Directory I/O.
 */
#define DIOSUC	0		/* Success.			 */
#define DIOEOF	1		/* End of file.			 */
#define DIOERR	2		/* Error.			 */

/*
 * Display colors.
 */
#define CNONE	0		/* Unknown color.		 */
#define CTEXT	1		/* Text color.			 */
#define CMODE	2		/* Mode line color.		 */

/*
 * Flags for keyboard invoked functions.
 */
#define FFUNIV		1	/* universal argument		 */
#define FFNEGARG	2	/* negative only argument	 */
#define FFOTHARG	4	/* other argument		 */
#define FFARG		7	/* any argument			 */
#define FFRAND		8	/* Called by other function	 */

/*
 * Flags for "eread".
 */
#define EFFUNC	0x0001		/* Autocomplete functions.	 */
#define EFBUF	0x0002		/* Autocomplete buffers.	 */
#define EFFILE	0x0004		/* " files (maybe someday)	 */
#define EFAUTO	0x0007		/* Some autocompleteion on	 */
#define EFNEW	0x0008		/* New prompt.			 */
#define EFCR	0x0010		/* Echo CR at end; last read.	 */
#define EFDEF	0x0020		/* buffer contains default args	 */

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
typedef struct LINE {
	struct LINE	*l_fp;	/* Link to the next line	 */
	struct LINE	*l_bp;	/* Link to the previous line	 */
	int		l_size;	/* Allocated size		 */
	int		l_used;	/* Used size			 */
	char		*l_text;	/* Content of the line */
} LINE;

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
	char *l_name;
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
typedef struct MGWIN {
	LIST		w_list;		/* List header			*/
	struct BUFFER	*w_bufp;	/* Buffer displayed in window	*/
	struct LINE	*w_linep;	/* Top line in the window	*/
	struct LINE	*w_dotp;	/* Line containing "."		*/
	struct LINE	*w_markp;	/* Line containing "mark"	*/
	int		w_doto;		/* Byte offset for "."		*/
	int		w_marko;	/* Byte offset for "mark"	*/
	char		w_toprow;	/* Origin 0 top row of window	*/
	char		w_ntrows;	/* # of rows of text in window	*/
	char		w_force;	/* If NZ, forcing row.		*/
	char		w_flag;		/* Flags.			*/
} MGWIN;
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
#define WFFORCE 0x01		/* Force reframe.		 */
#define WFMOVE	0x02		/* Movement from line to line.	 */
#define WFEDIT	0x04		/* Editing within a line.	 */
#define WFHARD	0x08		/* Better to a full display.	 */
#define WFMODE	0x10		/* Update mode line.		 */

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
typedef struct BUFFER {
	LIST		b_list;		/* buffer list pointer		 */
	struct BUFFER	*b_altb;	/* Link to alternate buffer	 */
	struct LINE	*b_dotp;	/* Link to "." LINE structure	 */
	struct LINE	*b_markp;	/* ditto for mark		 */
	struct LINE	*b_linep;	/* Link to the header LINE	 */
	struct MAPS_S	*b_modes[PBMODES]; /* buffer modes		 */
	int		b_doto;		/* Offset of "." in above LINE	 */
	int		b_marko;	/* ditto for the "mark"		 */
	short		b_nmodes;	/* number of non-fundamental modes */
	char		b_nwnd;		/* Count of windows on buffer	 */
	char		b_flag;		/* Flags			 */
	char		b_fname[NFILEN];/* File name			 */
	struct fileinfo	b_fi;		/* File attributes		 */
} BUFFER;
#define b_bufp	b_list.l_p.x_bp
#define b_bname b_list.l_name

#define BFCHG	0x01		/* Changed.			 */
#define BFBAK	0x02		/* Need to make a backup.	 */
#ifdef	NOTAB
#define BFNOTAB 0x04		/* no tab mode			 */
#endif
#define BFOVERWRITE 0x08	/* overwrite mode		 */

/*
 * This structure holds the starting position
 * (as a line/offset pair) and the number of characters in a
 * region of a buffer. This makes passing the specification
 * of a region around a little bit easier.
 */
typedef struct {
	struct LINE	*r_linep;	/* Origin LINE address.		 */
	int		r_offset;	/* Origin LINE offset.		 */
	RSIZE		r_size;		/* Length in characters.	 */
} REGION;

/*
 * Prototypes.
 */

/* tty.c X */
void	 ttinit			__P((void));
void	 ttreinit		__P((void));
void	 tttidy			__P((void));
void	 ttmove			__P((int, int));
void	 tteeol			__P((void));
void	 tteeop			__P((void));
void	 ttbeep			__P((void));
void	 ttinsl			__P((int, int, int));
void	 ttdell			__P((int, int, int));
void	 ttwindow		__P((int, int));
void	 ttnowindow		__P((void));
void	 ttcolor		__P((int));
void	 ttresize		__P((void));

/* ttyio.c */
void	 ttopen			__P((void));
int	 ttraw			__P((void));
void	 ttclose		__P((void));
int	 ttcooked		__P((void));
int	 ttputc			__P((int));
void	 ttflush		__P((void));
int	 ttgetc			__P((void));
int	 ttwait			__P((int));
int	 typeahead		__P((void));

/* dir.c */
void	 dirinit		__P((void));
int	 changedir		__P((int, int));
int	 showcwdir		__P((int, int));

/* dired.c */
int	 dired			__P((int, int));
int	 d_otherwindow		__P((int, int));
int	 d_undel		__P((int, int));
int	 d_undelbak		__P((int, int));
int	 d_findfile		__P((int, int));
int	 d_ffotherwindow	__P((int, int));
int	 d_expunge		__P((int, int));
int	 d_copy			__P((int, int));
int	 d_del			__P((int, int));
int	 d_rename		__P((int, int));

/* file.c X */
int	 fileinsert		__P((int, int));
int	 filevisit		__P((int, int));
int	 poptofile		__P((int, int));
BUFFER  *findbuffer		__P((char *));
int	 readin			__P((char *));
int	 insertfile		__P((char *, char *, int));
int	 filewrite		__P((int, int));
int	 filesave		__P((int, int));
int	 buffsave		__P((BUFFER *));
int	 makebkfile		__P((int, int));
int	 writeout		__P((BUFFER *, char *));
void	 upmodes		__P((BUFFER *));

/* line.c X */
LINE	*lalloc			__P((int));
int	 lrealloc		__P((LINE *, int));
void	 lfree			__P((LINE *));
void	 lchange		__P((int));
int	 linsert		__P((int, int));
int	 lnewline		__P((void));
int	 ldelete		__P((RSIZE, int));
int	 ldelnewline		__P((void));
int	 lreplace		__P((RSIZE, char *, int));
void	 kdelete		__P((void));
int	 kinsert		__P((int, int));
int	 kremove		__P((int));

/* window.c X */
int	 reposition		__P((int, int));
int	 refresh		__P((int, int));
int	 nextwind		__P((int, int));
int	 prevwind		__P((int, int));
int	 onlywind		__P((int, int));
int	 splitwind		__P((int, int));
int	 enlargewind		__P((int, int));
int	 shrinkwind		__P((int, int));
int	 delwind		__P((int, int));
MGWIN   *wpopup			__P((void));

/* buffer.c */
BUFFER  *bfind			__P((char *, int));
int	 poptobuffer		__P((int, int));
int	 killbuffer		__P((int, int));
int	 savebuffers		__P((int, int));
int	 listbuffers		__P((int, int));
int	 addlinef		__P((BUFFER *, char *, ...));
#define	 addline(bp, text)	addlinef(bp, "%s", text)
int	 anycb			__P((int));
int	 bclear			__P((BUFFER *));
int	 showbuffer		__P((BUFFER *, MGWIN *, int));
MGWIN   *popbuf			__P((BUFFER *));
int	 bufferinsert		__P((int, int));
int	 usebuffer		__P((int, int));
int	 notmodified		__P((int, int));
int	 popbuftop		__P((BUFFER *));

/* display.c */
int	vtresize		__P((int, int, int));
void	vtinit			__P((void));
void	vttidy			__P((void));
void	update			__P((void));

/* echo.c X */
void	 eerase			__P((void));
int	 eyorn			__P((char *));
int	 eyesno			__P((char *));
void	 ewprintf		__P((const char *fmt, ...));
int	 ereply			__P((const char *, char *, int, ...));
int	 eread			__P((const char *, char *, int, int, ...));
int	 getxtra		__P((LIST *, LIST *, int, int));
void	 free_file_list	__P((LIST *));

/* fileio.c */
int	 ffropen		__P((char *, BUFFER *));
int	 ffwopen		__P((char *, BUFFER *));
int	 ffclose		__P((BUFFER *));
int	 ffputbuf		__P((BUFFER *));
int	 ffgetline		__P((char *, int, int *));
int	 fbackupfile		__P((char *));
char	*adjustname		__P((char *));
char	*startupfile		__P((char *));
int	 copy			__P((char *, char *));
BUFFER  *dired_			__P((char *));
int	 d_makename		__P((LINE  *, char *));
LIST	*make_file_list		__P((char *));

/* kbd.c X */
int	 do_meta		__P((int, int));
int	 bsmap			__P((int, int));
void	 ungetkey		__P((int));
int	 getkey			__P((int));
int	 doin			__P((void));
int	 rescan			__P((int, int));
int	 universal_argument	__P((int, int));
int	 digit_argument		__P((int, int));
int	 negative_argument	__P((int, int));
int	 selfinsert		__P((int, int));
int	 quote			__P((int, int));

/* main.c */
int	 ctrlg			__P((int, int));
int	 quit			__P((int, int));

/* ttyio.c */
void	panic			__P((char *));

/* cinfo.c */
char	*keyname		__P((char  *, size_t, int));

/* basic.c */
int	 gotobol		__P((int, int));
int	 backchar		__P((int, int));
int	 gotoeol		__P((int, int));
int	 forwchar		__P((int, int));
int	 gotobob		__P((int, int));
int	 gotoeob		__P((int, int));
int	 forwline		__P((int, int));
int	 backline		__P((int, int));
void	 setgoal		__P((void));
int	 getgoal		__P((LINE *));
int	 forwpage		__P((int, int));
int	 backpage		__P((int, int));
int	 forw1page		__P((int, int));
int	 back1page		__P((int, int));
int	 pagenext		__P((int, int));
void	 isetmark		__P((void));
int	 setmark		__P((int, int));
int	 swapmark		__P((int, int));
int	 gotoline		__P((int, int));

/* random.c X */
int	 showcpos		__P((int, int));
int	 getcolpos		__P((void));
int	 twiddle		__P((int, int));
int	 openline		__P((int, int));
int	 newline		__P((int, int));
int	 deblank		__P((int, int));
int	 justone		__P((int, int));
int	 delwhite		__P((int, int));
int	 indent			__P((int, int));
int	 forwdel		__P((int, int));
int	 backdel		__P((int, int));
int	 killline		__P((int, int));
int	 yank			__P((int, int));
int	 space_to_tabstop	__P((int, int));

/* extend.c X */
int	 insert			__P((int, int));
int	 bindtokey		__P((int, int));
int	 localbind		__P((int, int));
int	 define_key		__P((int, int));
int	 unbindtokey		__P((int, int));
int	 localunbind		__P((int, int));
int	 extend			__P((int, int));
int	 evalexpr		__P((int, int));
int	 evalbuffer		__P((int, int));
int	 evalfile		__P((int, int));
int	 load			__P((char *));
int	 excline		__P((char *));

/* help.c X */
int	 desckey		__P((int, int));
int	 wallchart		__P((int, int));
int	 help_help		__P((int, int));
int	 apropos_command	__P((int, int));

/* paragraph.c X */
int	 gotobop		__P((int, int));
int	 gotoeop		__P((int, int));
int	 fillpara		__P((int, int));
int	 killpara		__P((int, int));
int	 fillword		__P((int, int));
int	 setfillcol		__P((int, int));

/* word.c X */
int	 backword		__P((int, int));
int	 forwword		__P((int, int));
int	 upperword		__P((int, int));
int	 lowerword		__P((int, int));
int	 capword		__P((int, int));
int	 delfword		__P((int, int));
int	 delbword		__P((int, int));
int	 inword			__P((void));

/* region.c X */
int	 killregion		__P((int, int));
int	 copyregion		__P((int, int));
int	 lowerregion		__P((int, int));
int	 upperregion		__P((int, int));
int	 prefixregion		__P((int, int));
int	 setprefix		__P((int, int));

/* search.c X */
int	 forwsearch		__P((int, int));
int	 backsearch		__P((int, int));
int	 searchagain		__P((int, int));
int	 forwisearch		__P((int, int));
int	 backisearch		__P((int, int));
int	 queryrepl		__P((int, int));
int	 forwsrch		__P((void));
int	 backsrch		__P((void));
int	 readpattern		__P((char *));

/* spawn.c X */
int	 spawncli		__P((int, int));

/* ttykbd.c X */
void	 ttykeymapinit		__P((void));
void	 ttykeymaptidy		__P((void));

/* match.c X */
int	 showmatch		__P((int, int));

/* version.c X */
int	 showversion		__P((int, int));

#ifndef NO_MACRO
/* macro.c X */
int	 definemacro		__P((int, int));
int	 finishmacro		__P((int, int));
int	 executemacro		__P((int, int));
#endif	/* !NO_MACRO */

/* modes.c X */
int	 indentmode		__P((int, int));
int	 fillmode		__P((int, int));
int	 blinkparen		__P((int, int));
#ifdef NOTAB
int	 notabmode		__P((int, int));
#endif	/* NOTAB */
int	 overwrite		__P((int, int));
int	 set_default_mode	__P((int,int));

#ifdef REGEX
/* re_search.c X */
int	 re_forwsearch		__P((int, int));
int	 re_backsearch		__P((int, int));
int	 re_searchagain		__P((int, int));
int	 re_queryrepl		__P((int, int));
int	 setcasefold		__P((int, int));
int	 delmatchlines		__P((int, int));
int	 delnonmatchlines	__P((int, int));
int	 cntmatchlines		__P((int, int));
int	 cntnonmatchlines	__P((int, int));
#endif	/* REGEX */

/*
 * Externals.
 */
extern BUFFER	*bheadp;
extern BUFFER	*curbp;
extern MGWIN	*curwp;
extern MGWIN	*wheadp;
extern int	 thisflag;
extern int	 lastflag;
extern int	 curgoal;
extern int	 epresf;
extern int	 sgarbf;
extern int	 mode;
extern int	 nrow;
extern int	 ncol;
extern int	 ttrow;
extern int	 ttcol;
extern int	 tttop;
extern int	 ttbot;
extern int	 tthue;
extern int	 defb_nmodes;
extern int	 defb_flag;
extern const char cinfo[];
extern char	*keystrings[];
extern char	 pat[NPAT];
#ifndef NO_DPROMPT
extern char	 prompt[];
#endif	/* !NO_DPROMPT */

/*
 * Globals.
 */
int	 tceeol;
int	 tcinsl;
int	 tcdell;

