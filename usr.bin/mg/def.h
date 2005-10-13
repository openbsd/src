/*	$OpenBSD: def.h,v 1.68 2005/10/13 05:24:52 kjell Exp $	*/

/* This file is in the public domain. */

#include <sys/queue.h>

/*
 * This file is the general header file for all parts
 * of the Mg display editor. It contains all of the
 * general definitions and macros. It also contains some
 * conditional compilation flags. All of the per-system and
 * per-terminal definitions are in special header files.
 */

#include	"sysdef.h"	/* Order is critical.		 */
#include	"ttydef.h"
#include	"chrdef.h"

#ifdef	NO_MACRO
#ifndef NO_STARTUP
#define NO_STARTUP		/* NO_MACRO implies NO_STARTUP */
#endif
#endif

typedef int	(*PF)(int, int);	/* generally useful type */

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
#define NKNAME	20		/* Length, key names.		 */
#define NTIME	50		/* Length, timestamp string.	 */
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
#define CFINS	0x0004		/* Last command was self-insert	 */

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
#define EFAUTO	0x0007		/* Some autocompletion on	 */
#define EFNEW	0x0008		/* New prompt.			 */
#define EFCR	0x0010		/* Echo CR at end; last read.	 */
#define EFDEF	0x0020		/* buffer contains default args	 */
#define EFNUL	0x0040		/* Null Minibuffer OK		 */

/*
 * Flags for "ldelete"/"kinsert"
 */
#define KNONE	0
#define KFORW	1
#define KBACK	2


/*
 * This structure holds the starting position
 * (as a line/offset pair) and the number of characters in a
 * region of a buffer. This makes passing the specification
 * of a region around a little bit easier.
 */
typedef struct {
	struct LINE	*r_linep;	/* Origin LINE address.		 */
	int		 r_offset;	/* Origin LINE offset.		 */
	RSIZE		 r_size;	/* Length in characters.	 */
} REGION;


/*
 * All text is kept in circularly linked
 * lists of "LINE" structures. These begin at the
 * header line (which is the blank line beyond the
 * end of the buffer). This line is pointed to by
 * the "BUFFER". Each line contains the number of
 * bytes in the line (the "used" size), the size
 * of the text array, and the text. The end of line
 * is not stored as a byte; it's implied. Future
 * additions will include update hints, and a
 * list of marks into the line.
 */
typedef struct LINE {
	struct LINE	*l_fp;		/* Link to the next line	 */
	struct LINE	*l_bp;		/* Link to the previous line	 */
	int		 l_size;	/* Allocated size		 */
	int		 l_used;	/* Used size			 */
	char		*l_text;	/* Content of the line		 */
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
 * I decide to do that. It does mean that there are four extra
 * bytes per window. I feel that this is an acceptable price,
 * considering that there are usually only one or two windows.
 */
typedef struct LIST {
	union {
		struct MGWIN	*l_wp;
		struct BUFFER	*x_bp;	/* l_bp is used by LINE */
		struct LIST	*l_nxt;
	} l_p;
	const char *l_name;
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
	LIST		 w_list;	/* List header			*/
	struct BUFFER	*w_bufp;	/* Buffer displayed in window	*/
	struct LINE	*w_linep;	/* Top line in the window	*/
	struct LINE	*w_dotp;	/* Line containing "."		*/
	struct LINE	*w_markp;	/* Line containing "mark"	*/
	int		 w_doto;	/* Byte offset for "."		*/
	int		 w_marko;	/* Byte offset for "mark"	*/
	char		 w_toprow;	/* Origin 0 top row of window	*/
	char		 w_ntrows;	/* # of rows of text in window	*/
	char		 w_force;	/* If NZ, forcing row.		*/
	char		 w_flag;	/* Flags.			*/
	struct LINE	*w_wrapline;
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
#define WFFORCE 0x01			/* Force reframe.		 */
#define WFMOVE	0x02			/* Movement from line to line.	 */
#define WFEDIT	0x04			/* Editing within a line.	 */
#define WFHARD	0x08			/* Better to a full display.	 */
#define WFMODE	0x10			/* Update mode line.		 */

struct undo_rec;

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
	LIST		 b_list;	/* buffer list pointer		 */
	struct BUFFER	*b_altb;	/* Link to alternate buffer	 */
	struct LINE	*b_dotp;	/* Link to "." LINE structure	 */
	struct LINE	*b_markp;	/* ditto for mark		 */
	struct LINE	*b_linep;	/* Link to the header LINE	 */
	struct MAPS_S	*b_modes[PBMODES]; /* buffer modes		 */
	int		 b_doto;	/* Offset of "." in above LINE	 */
	int		 b_marko;	/* ditto for the "mark"		 */
	short		 b_nmodes;	/* number of non-fundamental modes */
	char		 b_nwnd;	/* Count of windows on buffer	 */
	char		 b_flag;	/* Flags			 */
	char		 b_fname[NFILEN]; /* File name			 */
	struct fileinfo	 b_fi;		/* File attributes		 */
	LIST_HEAD(, undo_rec) b_undo;	/* Undo actions list		*/
	int		 b_undopos;	/* Where we were during the	*/
                                        /* last undo action.		*/
	struct undo_rec *b_undoptr;
} BUFFER;
#define b_bufp	b_list.l_p.x_bp
#define b_bname b_list.l_name

#define BFCHG	0x01			/* Changed.			 */
#define BFBAK	0x02			/* Need to make a backup.	 */
#ifdef	NOTAB
#define BFNOTAB 0x04			/* no tab mode			 */
#endif
#define BFOVERWRITE 0x08		/* overwrite mode		 */
#define BFREADONLY  0x10		/* read only mode		 */

/*
 * This structure holds information about recent actions for the Undo command.
 */
struct undo_rec {
	LIST_ENTRY(undo_rec) next;
	enum {
		INSERT = 1,
		DELETE,
		BOUNDARY
	} type;
	REGION		 region;
	int		 pos;
	char		*content;
};

/*
 * Prototypes.
 */

/* tty.c X */
void	 ttinit(void);
void	 ttreinit(void);
void	 tttidy(void);
void	 ttmove(int, int);
void	 tteeol(void);
void	 tteeop(void);
void	 ttbeep(void);
void	 ttinsl(int, int, int);
void	 ttdell(int, int, int);
void	 ttwindow(int, int);
void	 ttnowindow(void);
void	 ttcolor(int);
void	 ttresize(void);

volatile sig_atomic_t winch_flag;

/* ttyio.c */
void	 ttopen(void);
int	 ttraw(void);
void	 ttclose(void);
int	 ttcooked(void);
int	 ttputc(int);
void	 ttflush(void);
int	 ttgetc(void);
int	 ttwait(int);
int	 typeahead(void);

/* dir.c */
void	 dirinit(void);
int	 changedir(int, int);
int	 showcwdir(int, int);

/* dired.c */
int	 dired(int, int);
int	 d_otherwindow(int, int);
int	 d_undel(int, int);
int	 d_undelbak(int, int);
int	 d_findfile(int, int);
int	 d_ffotherwindow(int, int);
int	 d_expunge(int, int);
int	 d_copy(int, int);
int	 d_del(int, int);
int	 d_rename(int, int);
int	 d_shell_command(int, int);
int	 d_create_directory(int, int);

/* file.c X */
int	 fileinsert(int, int);
int	 filevisit(int, int);
int	 filevisitalt(int, int);
int	 filevisitro(int, int);
int	 poptofile(int, int);
BUFFER  *findbuffer(char *);
int	 readin(char *);
int	 insertfile(char *, char *, int);
int	 filewrite(int, int);
int	 filesave(int, int);
int	 buffsave(BUFFER *);
int	 makebkfile(int, int);
int	 writeout(BUFFER *, char *);
void	 upmodes(BUFFER *);

/* line.c X */
LINE	*lalloc(int);
int	 lrealloc(LINE *, int);
void	 lfree(LINE *);
void	 lchange(int);
int	 linsert_str(const char *, int);
int	 linsert(int, int);
int	 lnewline_at(LINE *, int);
int	 lnewline(void);
int	 ldelete(RSIZE, int);
int	 ldelnewline(void);
int	 lreplace(RSIZE, char *, int);
void	 kdelete(void);
int	 kinsert(int, int);
int	 kremove(int);

/* window.c X */
MGWIN	*new_window(BUFFER *);
void	 free_window(MGWIN *);
int	 reposition(int, int);
int	 refresh(int, int);
int	 nextwind(int, int);
int	 prevwind(int, int);
int	 onlywind(int, int);
int	 splitwind(int, int);
int	 enlargewind(int, int);
int	 shrinkwind(int, int);
int	 delwind(int, int);
MGWIN   *wpopup(void);

/* buffer.c */
int	 togglereadonly(int, int);
BUFFER  *bfind(const char *, int);
int	 poptobuffer(int, int);
int	 killbuffer(BUFFER *);
int	 killbuffer_cmd(int, int);
int	 savebuffers(int, int);
int	 listbuffers(int, int);
int	 addlinef(BUFFER *, char *, ...);
#define	 addline(bp, text)	addlinef(bp, "%s", text)
int	 anycb(int);
int	 bclear(BUFFER *);
int	 showbuffer(BUFFER *, MGWIN *, int);
MGWIN   *popbuf(BUFFER *);
int	 bufferinsert(int, int);
int	 usebuffer(int, int);
int	 notmodified(int, int);
int	 popbuftop(BUFFER *);

/* display.c */
int	vtresize(int, int, int);
void	vtinit(void);
void	vttidy(void);
void	update(void);

/* echo.c X */
void	 eerase(void);
int	 eyorn(const char *);
int	 eyesno(const char *);
void	 ewprintf(const char *fmt, ...);
char	*ereply(const char *, char *, size_t, ...);
char	*eread(const char *, char *, size_t, int, ...);
int	 getxtra(LIST *, LIST *, int, int);
void	 free_file_list(LIST *);

/* fileio.c */
int	 ffropen(const char *, BUFFER *);
int	 ffwopen(const char *, BUFFER *);
int	 ffclose(BUFFER *);
int	 ffputbuf(BUFFER *);
int	 ffgetline(char *, int, int *);
int	 fbackupfile(const char *);
char	*adjustname(const char *);
char	*startupfile(char *);
int	 copy(char *, char *);
BUFFER  *dired_(char *);
int	 d_makename(LINE  *, char *, int);
LIST	*make_file_list(char *);

/* kbd.c X */
int	 do_meta(int, int);
int	 bsmap(int, int);
void	 ungetkey(int);
int	 getkey(int);
int	 doin(void);
int	 rescan(int, int);
int	 universal_argument(int, int);
int	 digit_argument(int, int);
int	 negative_argument(int, int);
int	 selfinsert(int, int);
int	 quote(int, int);

/* main.c */
int	 ctrlg(int, int);
int	 quit(int, int);

/* ttyio.c */
void	 panic(char *);

/* cinfo.c */
char	*keyname(char  *, size_t, int);

/* basic.c */
int	 gotobol(int, int);
int	 backchar(int, int);
int	 gotoeol(int, int);
int	 forwchar(int, int);
int	 gotobob(int, int);
int	 gotoeob(int, int);
int	 forwline(int, int);
int	 backline(int, int);
void	 setgoal(void);
int	 getgoal(LINE *);
int	 forwpage(int, int);
int	 backpage(int, int);
int	 forw1page(int, int);
int	 back1page(int, int);
int	 pagenext(int, int);
void	 isetmark(void);
int	 setmark(int, int);
int	 swapmark(int, int);
int	 gotoline(int, int);

/* random.c X */
int	 showcpos(int, int);
int	 getcolpos(void);
int	 twiddle(int, int);
int	 openline(int, int);
int	 newline(int, int);
int	 deblank(int, int);
int	 justone(int, int);
int	 delwhite(int, int);
int	 indent(int, int);
int	 forwdel(int, int);
int	 backdel(int, int);
int	 killline(int, int);
int	 yank(int, int);
int	 space_to_tabstop(int, int);

/* extend.c X */
int	 insert(int, int);
int	 bindtokey(int, int);
int	 localbind(int, int);
int	 define_key(int, int);
int	 unbindtokey(int, int);
int	 localunbind(int, int);
int	 extend(int, int);
int	 evalexpr(int, int);
int	 evalbuffer(int, int);
int	 evalfile(int, int);
int	 load(const char *);
int	 excline(char *);

/* help.c X */
int	 desckey(int, int);
int	 wallchart(int, int);
int	 help_help(int, int);
int	 apropos_command(int, int);

/* paragraph.c X */
int	 gotobop(int, int);
int	 gotoeop(int, int);
int	 fillpara(int, int);
int	 killpara(int, int);
int	 fillword(int, int);
int	 setfillcol(int, int);

/* word.c X */
int	 backword(int, int);
int	 forwword(int, int);
int	 upperword(int, int);
int	 lowerword(int, int);
int	 capword(int, int);
int	 delfword(int, int);
int	 delbword(int, int);
int	 inword(void);

/* region.c X */
int	 killregion(int, int);
int	 copyregion(int, int);
int	 lowerregion(int, int);
int	 upperregion(int, int);
int	 prefixregion(int, int);
int	 setprefix(int, int);
int	 region_get_data(REGION *, char *, int);
int	 region_put_data(const char *, int);

/* search.c X */
int	 forwsearch(int, int);
int	 backsearch(int, int);
int	 searchagain(int, int);
int	 forwisearch(int, int);
int	 backisearch(int, int);
int	 queryrepl(int, int);
int	 forwsrch(void);
int	 backsrch(void);
int	 readpattern(char *);

/* spawn.c X */
int	 spawncli(int, int);

/* ttykbd.c X */
void	 ttykeymapinit(void);
void	 ttykeymaptidy(void);

/* match.c X */
int	 showmatch(int, int);

/* version.c X */
int	 showversion(int, int);

#ifndef NO_MACRO
/* macro.c X */
int	 definemacro(int, int);
int	 finishmacro(int, int);
int	 executemacro(int, int);
#endif	/* !NO_MACRO */

/* modes.c X */
int	 indentmode(int, int);
int	 fillmode(int, int);
int	 blinkparen(int, int);
#ifdef NOTAB
int	 notabmode(int, int);
#endif	/* NOTAB */
int	 overwrite(int, int);
int	 set_default_mode(int,int);

#ifdef REGEX
/* re_search.c X */
int	 re_forwsearch(int, int);
int	 re_backsearch(int, int);
int	 re_searchagain(int, int);
int	 re_queryrepl(int, int);
int	 replstr(int, int);
int	 setcasefold(int, int);
int	 delmatchlines(int, int);
int	 delnonmatchlines(int, int);
int	 cntmatchlines(int, int);
int	 cntnonmatchlines(int, int);
#endif	/* REGEX */

/* undo.c X */
void	 free_undo_record(struct undo_rec *);
int	 undo_dump(int, int);
int	 undo_enable(int);
int	 undo_add_boundary(void);
int	 undo_add_insert(LINE *, int, int);
int	 undo_add_delete(LINE *, int, int);
void	 undo_no_boundary(int);
int	 undo_add_change(LINE *, int, int);
int	 undo(int, int);

/* autoexec.c X */
int	 auto_execute(int, int);
PF	*find_autoexec(const char *);
int	 add_autoexec(const char *, const char *);

/* mail.c X */
void	 mail_init(void);

/* grep.c X */
int	 next_error(int, int);

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
extern int	 startrow;
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
