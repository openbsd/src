/*	$OpenBSD: option.h,v 1.1.1.1 1996/09/07 21:40:27 downsj Exp $	*/
/* vi:set ts=4 sw=4:
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 */

/*
 * option.h: definition of global variables for settable options
 *
 * EXTERN is only defined in main.c (and vim.h)
 */

#ifndef EXTERN
# define EXTERN extern
# define INIT(x)
#else
# ifndef INIT
#  define INIT(x) x
# endif
#endif

/* Formatting options for the p_fo option: */
#define FO_WRAP			't'
#define FO_WRAP_COMS	'c'
#define FO_RET_COMS		'r'
#define FO_OPEN_COMS	'o'
#define FO_Q_COMS		'q'
#define FO_Q_SECOND		'2'
#define FO_INS_VI		'v'
#define FO_INS_LONG		'l'
#define FO_INS_BLANK	'b'

#define FO_DFLT_VI		"vt"
#define FO_DFLT			"tcq"
#define FO_ALL			"tcroq2vlb,"	/* for do_set() */

/* characters for the p_cpo option: */
#define CPO_BAR			'b'		/* "\|" ends a mapping */
#define CPO_BSLASH		'B'		/* backslash in mapping is not special */
#define CPO_SEARCH		'c'
#define CPO_EXECBUF		'e'
#define CPO_FNAMER		'f'		/* set file name for ":r file" */
#define CPO_FNAMEW		'F'		/* set file name for ":w file" */
#define CPO_KEYCODE		'k'		/* don't recognize raw key code in mappings */
#define CPO_SHOWMATCH	'm'
#define CPO_LINEOFF		'o'
#define CPO_REDO		'r'
#define CPO_BUFOPT		's'
#define CPO_BUFOPTGLOB	'S'
#define CPO_TAGPAT		't'
#define CPO_ESC			'x'
#define CPO_DOLLAR		'$'
#define CPO_FILTER		'!'
#define CPO_MATCH		'%'
#define CPO_SPECI		'<'		/* don't recognize <> in mappings */
#define CPO_DEFAULT		"BceFs"
#define CPO_ALL			"bBcefFkmorsStx$!%<"

/* characters for p_ww option: */
#define WW_ALL			"bshl<>[],"

/* characters for p_mouse option: */
#define MOUSE_NORMAL	'n'				/* use mouse in normal mode */
#define MOUSE_VISUAL	'v'				/* use mouse in visual mode */
#define MOUSE_INSERT	'i'				/* use mouse in insert mode */
#define MOUSE_COMMAND	'c'				/* use mouse in command line mode */
#define MOUSE_HELP		'h'				/* use mouse in help buffers */
#define MOUSE_RETURN	'r'				/* use mouse for hit-return message */
#define MOUSE_A			"nvich"			/* used for 'a' flag */
#define MOUSE_ALL		"anvicrh"		/* all possible characters */

/* characters for p_shm option: */
#define SHM_RO			'r'			/* readonly */
#define SHM_MOD			'm'			/* modified */
#define SHM_FILE		'f'			/* (file 1 of 2) */
#define SHM_LAST		'i'			/* last line incomplete */
#define SHM_TEXT		'x'			/* tx instead of textmode */
#define SHM_LINES		'l'			/* "L" instead of "lines" */
#define SHM_NEW			'n'			/* "[New]" instead of "[New file]" */
#define SHM_WRI			'w'			/* "[w]" instead of "written" */
#define SHM_A			"rmfixlnw"	/* represented by 'a' flag */
#define SHM_WRITE		'W'			/* don't use "written" at all */
#define SHM_TRUNC		't'			/* trunctate message */
#define SHM_OVER		'o'			/* overwrite file messages */
#define SHM_SEARCH		's'			/* no search hit bottom messages */
#define SHM_ALL			"rmfixlnwaWtos"	/* all possible flags for 'shm' */

/* characters for p_guioptions: */
#define GO_ASEL			'a'			/* GUI: autoselect */
#define GO_BOT			'b'			/* GUI: use bottom scrollbar */
#define GO_FORG			'f'			/* GUI: start GUI in foreground */
#define GO_GREY			'g'			/* GUI: use grey menu items */
#define GO_LEFT			'l'			/* GUI: use left scrollbar */
#define GO_MENUS		'm'			/* GUI: use menu bar */
#define GO_RIGHT		'r'			/* GUI: use right scrollbar */
#define GO_ALL			"abfglmr"	/* all possible flags for 'go' */

/* flags for 'comments' option */
#define COM_NEST		'n'			/* comments strings nest */
#define COM_BLANK		'b'			/* needs blank after string */
#define COM_START		's'			/* start of comment */
#define COM_MIDDLE		'm'			/* middle of comment */
#define COM_END			'e'			/* end of comment */
#define COM_FIRST		'f'			/* first line comment only */
#define COM_LEFT		'l'			/* left adjusted */
#define COM_RIGHT		'r'			/* right adjusted */
#define COM_ALL			"nbsmeflr"	/* all flags for 'comments' option */
#define COM_MAX_LEN		50			/* maximum lenght of a part */

/*
 * The following are actual variabables for the options
 */

#ifdef RIGHTLEFT
EXTERN int		p_aleph;	/* Hebrew 'Aleph' encoding */
#endif
EXTERN int		p_aw;		/* auto-write */
EXTERN long		p_bs;		/* backspace over newlines in insert mode */
EXTERN int		p_bk;		/* make backups when writing out files */
EXTERN char_u  *p_bdir;		/* list of directory names for backup files */
EXTERN char_u  *p_bex;		/* extension for backup file */
#ifdef MSDOS
EXTERN int		p_biosk;	/* Use bioskey() instead of kbhit() */
#endif
EXTERN char_u  *p_breakat;	/* characters that can cause a line break */
EXTERN long		p_ch;		/* command line height */
EXTERN int		p_cp;		/* vi-compatible */
EXTERN char_u  *p_cpo;		/* vi-compatible option flags */
EXTERN char_u  *p_def;		/* Pattern for recognising definitions */
EXTERN char_u  *p_dict;		/* Dictionaries for ^P/^N */
#ifdef DIGRAPHS
EXTERN int		p_dg;		/* enable digraphs */
#endif /* DIGRAPHS */
EXTERN char_u  	*p_dir;		/* list of directories for swap file */
EXTERN int		p_ed;		/* :s is ed compatible */
EXTERN int		p_ea;		/* make windows equal height */
EXTERN char_u  	*p_ep;		/* program name for '=' command */
EXTERN int		p_eb;		/* ring bell for errors */
EXTERN char_u  *p_ef;		/* name of errorfile */
EXTERN char_u  *p_efm;		/* error format */
EXTERN int		p_ek;		/* function keys with ESC in insert mode */
EXTERN int		p_exrc;		/* read .exrc in current dir */
EXTERN char_u  *p_fp;		/* name of format program */
EXTERN int		p_gd;		/* /g is default for :s */
#ifdef USE_GUI
EXTERN char_u  *p_guifont;		/* GUI font list */
EXTERN char_u  *p_guioptions;	/* Which GUI components? */
EXTERN int		p_guipty;		/* use pseudo pty for external commands */
#endif
EXTERN char_u  *p_hf;		/* name of help file */
EXTERN long 	p_hh;		/* help window height */
EXTERN int		p_hid;		/* buffers can be hidden */
EXTERN char_u  *p_hl;		/* which highlight mode to use */
EXTERN long 	p_hi;		/* command line history size */
#ifdef RIGHTLEFT
EXTERN int		p_hkmap;	/* Hebrew keyboard map */
#endif
EXTERN int		p_icon;		/* put file name in icon if possible */
EXTERN int		p_ic;		/* ignore case in searches */
EXTERN int		p_is;		/* incremental search */
EXTERN int		p_im;		/* start editing in input mode */
EXTERN char_u  *p_inc;		/* Pattern for including other files */
EXTERN char_u  *p_isf;		/* characters in a file name */
EXTERN char_u  *p_isi;		/* characters in an identifier */
EXTERN char_u  *p_isp;		/* characters that are printable */
EXTERN int		p_js;		/* use two spaces after '.' with Join */
EXTERN char_u  *p_kp;		/* keyword program */
#ifdef HAVE_LANGMAP
EXTERN char_u  *p_langmap;	/* mapping for some language */
#endif
EXTERN long		p_ls;		/* last window has status line */
EXTERN int		p_magic;	/* use some characters for reg exp */
EXTERN char_u  *p_mp;		/* program for :make command */
EXTERN long 	p_mmd;		/* maximal map depth */
EXTERN long 	p_mm;		/* maximal amount of memory for buffer */
EXTERN long 	p_mmt;		/* maximal amount of memory for Vim */
EXTERN long 	p_mls;		/* number of mode lines */
EXTERN char_u  *p_mouse;	/* enable mouse clicks (for xterm) */
EXTERN long		p_mouset;  	/* mouse double click time */
EXTERN int		p_more;		/* wait when screen full when listing */
EXTERN char_u  *p_para;		/* paragraphs */
EXTERN int		p_paste;	/* paste mode */
EXTERN char_u  *p_pm;  		/* patchmode file suffix */
EXTERN char_u  *p_path;		/* path for "]f" and "^Wf" */
EXTERN int		p_remap;	/* remap */
EXTERN long		p_report;	/* minimum number of lines for report */
#ifdef WIN32
EXTERN int		p_rs;		/* restore startup screen upon exit */
#endif
#ifdef RIGHTLEFT
EXTERN int		p_ri;		/* reverse direction of insert */
#endif
EXTERN int		p_ru;		/* show column/line number */
EXTERN long		p_sj;		/* scroll jump size */
EXTERN long		p_so;		/* scroll offset */
EXTERN char_u  *p_sections;	/* sections */
EXTERN int		p_secure;	/* do .exrc and .vimrc in secure mode */
EXTERN char_u  *p_sh;		/* name of shell to use */
EXTERN char_u  *p_sp;		/* string for output of make */
EXTERN char_u  *p_srr;		/* string for output of filter */
EXTERN long		p_st;		/* type of shell */
EXTERN int		p_sr;		/* shift round off (for < and >) */
EXTERN char_u  *p_shm;		/* When to use short message */
EXTERN char_u  *p_sbr;		/* string for break of line */
EXTERN int		p_sc;		/* show command in status line */
EXTERN int		p_sm;		/* showmatch */
EXTERN int		p_smd;		/* show mode */
EXTERN long		p_ss;		/* sideways scrolling offset */
EXTERN int		p_scs;		/* 'smartcase' */
EXTERN int		p_sta;		/* smart-tab for expand-tab */
EXTERN int		p_sb;		/* split window backwards */
EXTERN int		p_sol;		/* Move cursor to start-of-line? */
EXTERN char_u  *p_su;		/* suffixes for wildcard expansion */
EXTERN char_u  *p_sws;		/* swap file syncing */
EXTERN long 	p_tl;		/* used tag length */
EXTERN int		p_tr;		/* tag file name is relative */
EXTERN char_u  *p_tags;		/* tags search path */
EXTERN int		p_terse;	/* terse messages */
EXTERN int		p_ta;		/* auto textmode detection */
EXTERN int		p_to;		/* tilde is an operator */
EXTERN int		p_timeout;	/* mappings entered within one second */
EXTERN long 	p_tm;		/* timeoutlen (msec) */
EXTERN int		p_title;	/* set window title if possible */
EXTERN int		p_ttimeout;	/* key codes entered within one second */
EXTERN long 	p_ttm;		/* key code timeoutlen (msec) */
EXTERN int		p_tbi;		/* 'ttybuiltin' use builtin termcap first */
EXTERN int		p_tf;		/* terminal fast I/O */
EXTERN long		p_ttyscroll; /* maximum number of screen lines for a scroll */
EXTERN long 	p_ul;		/* number of Undo Levels */
EXTERN long 	p_uc;		/* update count for swap file */
EXTERN long 	p_ut;		/* update time for swap file */
#ifdef VIMINFO
EXTERN char_u  *p_viminfo;	/* Parameters for using ~/.viminfo file */
#endif /* VIMINFO */
EXTERN int		p_vb;		/* visual bell only (no beep) */
EXTERN int		p_warn;		/* warn for changes at shell command */
EXTERN int		p_wiv;		/* inversion of text is weird */
EXTERN char_u  *p_ww;		/* which keys wrap to next/prev line */
EXTERN long		p_wc;		/* character for wildcard exapansion */
EXTERN long		p_wh;		/* desired window height */
EXTERN int		p_ws;		/* wrap scan */
EXTERN int		p_wa;		/* write any */
EXTERN int		p_wb;		/* write backup files */
EXTERN long		p_wd;		/* write delay for screen output (for testing) */
