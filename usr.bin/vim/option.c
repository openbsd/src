/*	$OpenBSD: option.c,v 1.2 1996/09/21 06:23:14 downsj Exp $	*/
/* vi:set ts=4 sw=4:
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 */

/*
 * Code to handle user-settable options. This is all pretty much table-
 * driven. To add a new option, put it in the options array, and add a
 * variable for it in option.h. If it's a numeric option, add any necessary
 * bounds checks to do_set().
 */

#include "vim.h"
#include "globals.h"
#include "proto.h"
#include "option.h"

struct option
{
	char		*fullname;		/* full option name */
	char		*shortname; 	/* permissible abbreviation */
	short 		flags;			/* see below */
	char_u		*var;			/* pointer to variable */
	char_u		*def_val;		/* default value for variable (can be the same
									as the actual value) */
};

/*
 * Flags
 *
 * Note: P_EXPAND and P_IND can never be used at the same time.
 * Note: P_IND cannot be used for a terminal option.
 */
#define P_BOOL			0x01	/* the option is boolean */
#define P_NUM			0x02	/* the option is numeric */
#define P_STRING		0x04	/* the option is a string */
#define P_ALLOCED		0x08	/* the string option is in allocated memory,
									must use vim_free() when assigning new
									value. Not set if default is the same. */
#define P_EXPAND		0x10	/* environment expansion */
#define P_IND			0x20	/* indirect, is in curwin or curbuf */
#define P_NODEFAULT		0x40	/* has no default value */
#define P_DEF_ALLOCED	0x80	/* default value is in allocated memory, must
									use vim_free() when assigning new value */
#define P_WAS_SET		0x100	/* option has been set/reset */
#define P_NO_MKRC		0x200	/* don't include in :mkvimrc output */

/*
 * The options that are in curwin or curbuf have P_IND set and a var field
 * that contains one of the values below.
 */
#define PV_LIST		1
#define PV_NU		2
#define PV_SCROLL	3
#define PV_WRAP		4
#define PV_LBR		5

#define PV_AI		6
#define PV_BIN		7
#define PV_CIN		8
#define PV_CINK		9
#define PV_CINO		10
#define PV_CINW		11
#define PV_COM		12
#define PV_EOL		13
#define PV_ET		14
#define PV_FO		15
#define PV_LISP		16
#define PV_ML		17
#define PV_MOD		18
#define PV_RO		20
#define PV_SI		21
#define PV_SN		22
#define PV_SW		23
#define PV_TS		24
#define PV_TW		25
#define PV_TX		26
#define PV_WM		27
#define PV_ISK		28
#define PV_INF		29
#define PV_RL		30

/*
 * The option structure is initialized here.
 * The order of the options should be alphabetic for ":set all".
 * The options with a NULL variable are 'hidden': a set command for
 * them is ignored and they are not printed.
 */
static struct option options[] =
{
#ifdef RIGHTLEFT
	{"aleph",		"al",	P_NUM,				(char_u *)&p_aleph,
# if defined(MSDOS) || defined(WIN32) || defined(OS2)
							(char_u *)128L},
# else
							(char_u *)224L},
# endif
#endif
	{"autoindent",	"ai",	P_BOOL|P_IND,		(char_u *)PV_AI,
							(char_u *)FALSE},
	{"autoprint",	"ap",	P_BOOL,				(char_u *)NULL,
							(char_u *)FALSE},
	{"autowrite",	"aw",	P_BOOL,				(char_u *)&p_aw,
							(char_u *)FALSE},
	{"backspace",	"bs",	P_NUM,				(char_u *)&p_bs,
							(char_u *)0L},
	{"backup",		"bk",	P_BOOL,				(char_u *)&p_bk,
							(char_u *)FALSE},
	{"backupdir",	"bdir",	P_STRING|P_EXPAND,
												(char_u *)&p_bdir,
							(char_u *)DEF_BDIR},
	{"backupext",	"bex",	P_STRING,			(char_u *)&p_bex,
#ifdef VMS
							(char_u *)"_"},
#else
							(char_u *)"~"},
#endif
	{"beautify",	"bf",	P_BOOL,				(char_u *)NULL,
							(char_u *)FALSE},
	{"binary",		"bin",	P_BOOL|P_IND,		(char_u *)PV_BIN,
							(char_u *)FALSE},
	{"bioskey",		"biosk",P_BOOL,
#ifdef MSDOS
												(char_u *)&p_biosk,
#else
												(char_u *)NULL,
#endif
							(char_u *)TRUE},
	{"breakat",		"brk",	P_STRING,			(char_u *)&p_breakat,
							(char_u *)" \t!@*-+_;:,./?"},
#ifdef CINDENT
	{"cindent",		"cin",  P_BOOL|P_IND,		(char_u *)PV_CIN,
							(char_u *)FALSE},
	{"cinkeys",		"cink",	P_STRING|P_IND|P_ALLOCED,	(char_u *)PV_CINK,
							(char_u *)"0{,0},:,0#,!^F,o,O,e"},
	{"cinoptions",	"cino", P_STRING|P_IND|P_ALLOCED,	(char_u *)PV_CINO,
							(char_u *)""},
#endif /* CINDENT */
#if defined(SMARTINDENT) || defined(CINDENT)
	{"cinwords",	"cinw",	P_STRING|P_IND|P_ALLOCED,	(char_u *)PV_CINW,
							(char_u *)"if,else,while,do,for,switch"},
#endif
	{"cmdheight",	"ch",	P_NUM,				(char_u *)&p_ch,
							(char_u *)1L},
	{"columns",		"co",	P_NUM|P_NODEFAULT|P_NO_MKRC, (char_u *)&Columns,
							(char_u *)80L},
	{"comments",	"com",	P_STRING|P_IND|P_ALLOCED,	(char_u *)PV_COM,
							(char_u *)"sr:/*,mb:*,el:*/,://,b:#,:%,:XCOMM,n:>,fb:-"},
	{"compatible",	"cp",	P_BOOL,				(char_u *)&p_cp,
							(char_u *)FALSE},
	{"cpoptions",	"cpo",	P_STRING,			(char_u *)&p_cpo,
#ifdef COMPATIBLE
							(char_u *)CPO_ALL},
#else
							(char_u *)CPO_DEFAULT},
#endif
	{"define",		"def",	P_STRING,			(char_u *)&p_def,
							(char_u *)"^#[ \\t]*define"},
	{"dictionary",	"dict",	P_STRING|P_EXPAND,	(char_u *)&p_dict,
							(char_u *)""},
	{"digraph",		"dg",	P_BOOL,
#ifdef DIGRAPHS
												(char_u *)&p_dg,
#else
												(char_u *)NULL,
#endif /* DIGRAPHS */
							(char_u *)FALSE},
	{"directory",	"dir",	P_STRING|P_EXPAND,	(char_u *)&p_dir,
							(char_u *)DEF_DIR},
	{"edcompatible","ed",	P_BOOL,				(char_u *)&p_ed,
							(char_u *)FALSE},
	{"endofline",	"eol",	P_BOOL|P_IND|P_NO_MKRC,	(char_u *)PV_EOL,
							(char_u *)FALSE},
	{"equalalways",	"ea",  	P_BOOL,				(char_u *)&p_ea,
							(char_u *)TRUE},
	{"equalprg",	"ep",  	P_STRING|P_EXPAND,	(char_u *)&p_ep,
							(char_u *)""},
	{"errorbells",	"eb",	P_BOOL,				(char_u *)&p_eb,
							(char_u *)FALSE},
	{"errorfile",	"ef",  	P_STRING|P_EXPAND,	(char_u *)&p_ef,
#ifdef AMIGA
							(char_u *)"AztecC.Err"},
#else
							(char_u *)"errors.vim"},
#endif
	{"errorformat",	"efm", 	P_STRING,			(char_u *)&p_efm,
#ifdef AMIGA
						/* don't use [^0-9] here, Manx C can't handle it */
							(char_u *)"%f>%l:%c:%t:%n:%m,%f:%l: %t%*[^0123456789]%n: %m,%f %l %t%*[^0123456789]%n: %m,%*[^\"]\"%f\"%*[^0123456789]%l: %m,%f:%l:%m"},
#else
# if defined MSDOS  ||  defined WIN32
							(char_u *)"%*[^\"]\"%f\"%*[^0-9]%l: %m,%f(%l) : %m,%*[^ ] %f %l: %m,%f:%l:%m"},
# else
#  if defined(__EMX__)	/* put most common here (i.e. gcc format) at front */
							(char_u *)"%f:%l:%m,%*[^\"]\"%f\"%*[^0-9]%l: %m,\"%f\"%*[^0-9]%l: %m,%f(%l:%c) : %m"},
#  else
#   if defined(__QNX__)
							(char_u *)"%f(%l):%*[^WE]%t%*[^0123456789]%n:%m"},
#   else /* Unix, probably */
							(char_u *)"%*[^\"]\"%f\"%*[^0-9]%l: %m,\"%f\"%*[^0-9]%l: %m,%f:%l:%m,\"%f\"\\, line %l%*[^0-9]%c%*[^ ] %m"},
#   endif
#  endif
# endif
#endif
	{"esckeys",		"ek",	P_BOOL,				(char_u *)&p_ek,
#ifdef COMPATIBLE
							(char_u *)FALSE},
#else
							(char_u *)TRUE},
#endif
	{"expandtab",	"et",	P_BOOL|P_IND,		(char_u *)PV_ET,
							(char_u *)FALSE},
	{"exrc",		NULL,	P_BOOL,				(char_u *)&p_exrc,
							(char_u *)FALSE},
	{"flash",		"fl",	P_BOOL,				(char_u *)NULL,
							(char_u *)FALSE},
	{"formatoptions","fo",	P_STRING|P_IND|P_ALLOCED,	(char_u *)PV_FO,
#ifdef COMPATIBLE
							(char_u *)FO_DFLT_VI},
#else
							(char_u *)FO_DFLT},
#endif
	{"formatprg",	"fp",  	P_STRING|P_EXPAND,	(char_u *)&p_fp,
							(char_u *)""},
	{"gdefault",	"gd",	P_BOOL,				(char_u *)&p_gd,
							(char_u *)FALSE},
	{"graphic",		"gr",	P_BOOL,				(char_u *)NULL,
							(char_u *)FALSE},
	{"guifont",		"gfn", 	P_STRING,
#ifdef USE_GUI
												(char_u *)&p_guifont,
							(char_u *)""},
#else
												(char_u *)NULL,
							(char_u *)NULL},
#endif
	{"guioptions",	"go", 	P_STRING,
#ifdef USE_GUI
												(char_u *)&p_guioptions,
# ifdef UNIX
							(char_u *)"agmr"},
# else
							(char_u *)"gmr"},
# endif
#else
												(char_u *)NULL,
							(char_u *)NULL},
#endif
#if defined(USE_GUI)
	{"guipty",		NULL,	P_BOOL,				(char_u *)&p_guipty,
							(char_u *)FALSE},
#endif
	{"hardtabs",	"ht",	P_NUM,				(char_u *)NULL,
							(char_u *)0L},
	{"helpfile",	"hf",  	P_STRING|P_EXPAND,	(char_u *)&p_hf,
							(char_u *)""},
	{"helpheight",	"hh",  	P_NUM,				(char_u *)&p_hh,
							(char_u *)20L},
	{"hidden",		"hid",	P_BOOL,				(char_u *)&p_hid,
							(char_u *)FALSE},
	{"highlight",	"hl",	P_STRING,			(char_u *)&p_hl,
							(char_u *)"8b,db,es,hs,mb,Mn,nu,rs,sr,tb,vr,ws"},
	{"history",		"hi", 	P_NUM,				(char_u *)&p_hi,
#ifdef COMPATIBLE
							(char_u *)0L},
#else
							(char_u *)20L},
#endif
#ifdef RIGHTLEFT
	{"hkmap",		"hk",	P_BOOL,				(char_u *)&p_hkmap,
							(char_u *)FALSE},
#endif
	{"icon",	 	NULL,	P_BOOL,				(char_u *)&p_icon,
							(char_u *)FALSE},
	{"ignorecase",	"ic",	P_BOOL,				(char_u *)&p_ic,
							(char_u *)FALSE},
	{"include",		"inc",	P_STRING,			(char_u *)&p_inc,
							(char_u *)"^#[ \\t]*include"},
	{"incsearch",	"is",	P_BOOL,				(char_u *)&p_is,
							(char_u *)FALSE},
	{"infercase",	"inf",	P_BOOL|P_IND,		(char_u *)PV_INF,
							(char_u *)FALSE},
	{"insertmode",	"im",	P_BOOL,				(char_u *)&p_im,
							(char_u *)FALSE},
	{"isfname",		"isf",	P_STRING,			(char_u *)&p_isf,
#ifdef BACKSLASH_IN_FILENAME
							(char_u *)"@,48-57,/,.,-,_,+,,,$,:,\\"},
#else
# ifdef AMIGA
							(char_u *)"@,48-57,/,.,-,_,+,,,$,:"},
# else /* UNIX */
							(char_u *)"@,48-57,/,.,-,_,+,,,$,:,~"},
# endif
#endif
	{"isident",		"isi",	P_STRING,			(char_u *)&p_isi,
#if defined(MSDOS) || defined(WIN32) || defined(OS2)
							(char_u *)"@,48-57,_,128-167,224-235"},
#else
							(char_u *)"@,48-57,_,192-255"},
#endif
	{"iskeyword",	"isk",	P_STRING|P_IND|P_ALLOCED,	(char_u *)PV_ISK,
#ifdef COMPATIBLE
							(char_u *)"@,48-57,_"},
#else
# if defined MSDOS  ||  defined WIN32
							(char_u *)"@,48-57,_,128-167,224-235"},
# else
							(char_u *)"@,48-57,_,192-255"},
# endif
#endif
	{"isprint",		"isp",	P_STRING,			(char_u *)&p_isp,
#if defined MSDOS  ||  defined WIN32
							(char_u *)"@,~-255"},
#else
							(char_u *)"@,161-255"},
#endif
	{"joinspaces",	"js",	P_BOOL,				(char_u *)&p_js,
							(char_u *)TRUE},
	{"keywordprg",	"kp",  	P_STRING|P_EXPAND,	(char_u *)&p_kp,
#if defined(MSDOS) ||  defined(WIN32)
							(char_u *)""},
#else
							(char_u *)"man"},
#endif
	{"langmap",     "lmap", P_STRING,  		
#ifdef HAVE_LANGMAP
												(char_u *)&p_langmap,
							(char_u *)""},
#else
												(char_u *)NULL,
							(char_u *)NULL},
#endif
	{"laststatus",	"ls", 	P_NUM,				(char_u *)&p_ls,
							(char_u *)1L},
	{"linebreak",	"lbr",	P_BOOL|P_IND,		(char_u *)PV_LBR,
							(char_u *)FALSE},
	{"lines",		NULL, 	P_NUM|P_NODEFAULT|P_NO_MKRC, (char_u *)&Rows,
#if defined MSDOS  ||  defined WIN32
							(char_u *)25L},
#else
							(char_u *)24L},
#endif
	{"lisp",		NULL,	P_BOOL|P_IND,		(char_u *)PV_LISP,
							(char_u *)FALSE},
	{"list",		NULL,	P_BOOL|P_IND,		(char_u *)PV_LIST,
							(char_u *)FALSE},
	{"magic",		NULL,	P_BOOL,				(char_u *)&p_magic,
							(char_u *)TRUE},
	{"makeprg",		"mp",  	P_STRING|P_EXPAND,	(char_u *)&p_mp,
							(char_u *)"make"},
	{"maxmapdepth",	"mmd",	P_NUM,				(char_u *)&p_mmd,
							(char_u *)1000L},
	{"maxmem",		"mm",	P_NUM,				(char_u *)&p_mm,
							(char_u *)MAXMEM},
	{"maxmemtot",	"mmt",	P_NUM,				(char_u *)&p_mmt,
							(char_u *)MAXMEMTOT},
	{"mesg",		NULL,	P_BOOL,				(char_u *)NULL,
							(char_u *)FALSE},
	{"modeline",	"ml",	P_BOOL|P_IND,		(char_u *)PV_ML,
#ifdef COMPATIBLE
							(char_u *)FALSE},
#else
							(char_u *)TRUE},
#endif
	{"modelines",	"mls",	P_NUM,				(char_u *)&p_mls,
							(char_u *)5L},
	{"modified",	"mod",	P_BOOL|P_IND|P_NO_MKRC,	(char_u *)PV_MOD,
							(char_u *)FALSE},
	{"more",		NULL,	P_BOOL,				(char_u *)&p_more,
#ifdef COMPATIBLE
							(char_u *)FALSE},
#else
							(char_u *)TRUE},
#endif
	{"mouse",		NULL,	P_STRING,			(char_u *)&p_mouse,
#if defined(MSDOS) || defined(WIN32)
							(char_u *)"a"},
#else
							(char_u *)""},
#endif
	{"mousetime",	"mouset",	P_NUM,			(char_u *)&p_mouset,
							(char_u *)500L},
	{"novice",		NULL,	P_BOOL,				(char_u *)NULL,
							(char_u *)FALSE},
	{"number",		"nu",	P_BOOL|P_IND,		(char_u *)PV_NU,
							(char_u *)FALSE},
	{"open",		NULL,	P_BOOL,				(char_u *)NULL,
							(char_u *)FALSE},
	{"optimize",	"opt",	P_BOOL,				(char_u *)NULL,
							(char_u *)FALSE},
	{"paragraphs",	"para",	P_STRING,			(char_u *)&p_para,
							(char_u *)"IPLPPPQPP LIpplpipbp"},
	{"paste",		NULL,	P_BOOL,				(char_u *)&p_paste,
							(char_u *)FALSE},
	{"patchmode",	"pm",   P_STRING,			(char_u *)&p_pm,
							(char_u *)""},
	{"path",		"pa",  	P_STRING|P_EXPAND,	(char_u *)&p_path,
#if defined AMIGA  ||  defined MSDOS  ||  defined WIN32
							(char_u *)".,,"},
#else
# if defined(__EMX__)
							(char_u *)".,/emx/include,,"},
# else /* Unix, probably */
							(char_u *)".,/usr/include,,"},
# endif
#endif
	{"prompt",		NULL,	P_BOOL,				(char_u *)NULL,
							(char_u *)FALSE},
	{"readonly",	"ro",	P_BOOL|P_IND,		(char_u *)PV_RO,
							(char_u *)FALSE},
	{"redraw",		NULL,	P_BOOL,				(char_u *)NULL,
							(char_u *)FALSE},
	{"remap",		NULL,	P_BOOL,				(char_u *)&p_remap,
							(char_u *)TRUE},
	{"report",		NULL,	P_NUM,				(char_u *)&p_report,
							(char_u *)2L},
#ifdef WIN32
	{"restorescreen", "rs",	P_BOOL,				(char_u *)&p_rs,
							(char_u *)TRUE},
#endif
#ifdef RIGHTLEFT
	{"revins",		"ri",	P_BOOL,				(char_u *)&p_ri,
							(char_u *)FALSE},
	{"rightleft",	"rl",	P_BOOL|P_IND,		(char_u *)PV_RL,
							(char_u *)FALSE},
#endif
	{"ruler",		"ru",	P_BOOL,				(char_u *)&p_ru,
							(char_u *)FALSE},
	{"scroll",		"scr", 	P_NUM|P_IND|P_NO_MKRC, (char_u *)PV_SCROLL,
							(char_u *)12L},
	{"scrolljump",	"sj", 	P_NUM,				(char_u *)&p_sj,
							(char_u *)1L},
	{"scrolloff",	"so", 	P_NUM,				(char_u *)&p_so,
							(char_u *)0L},
	{"sections",	"sect",	P_STRING,			(char_u *)&p_sections,
							(char_u *)"SHNHH HUnhsh"},
	{"secure",		NULL,	P_BOOL,				(char_u *)&p_secure,
							(char_u *)FALSE},
	{"shell",		"sh",	P_STRING|P_EXPAND,	(char_u *)&p_sh,
#if defined(MSDOS)
							(char_u *)"command"},
#else
# if defined(WIN32)
							(char_u *)""},		/* set in set_init_1() */
# else
#  if defined(__EMX__)
							(char_u *)"cmd.exe"},
#  else
#   if defined(ARCHIE)
							(char_u *)"gos"},
#   else
							(char_u *)"sh"},
#   endif
#  endif
# endif
#endif
	{"shellcmdflag","shcf", P_STRING,           (char_u *)&p_shcf,
#if defined(MSDOS) || defined(WIN32)
							(char_u *)"/c"},
#else
							(char_u *)"-c"},
#endif
	{"shellpipe",	"sp",	P_STRING,			(char_u *)&p_sp,
#if defined(UNIX) || defined(OS2)
# ifdef ARCHIE
							(char_u *)"2>"},
# else
							(char_u *)"| tee"},
# endif
#else
							(char_u *)">"},
#endif
	{"shellquote",  "shq",  P_STRING,           (char_u *)&p_shq,
							(char_u *)""},
	{"shellredir",	"srr",	P_STRING,			(char_u *)&p_srr,
							(char_u *)">"},
	{"shelltype",	"st",	P_NUM,				(char_u *)&p_st,
							(char_u *)0L},
	{"shiftround",	"sr",	P_BOOL,				(char_u *)&p_sr,
							(char_u *)FALSE},
	{"shiftwidth",	"sw",	P_NUM|P_IND,		(char_u *)PV_SW,
							(char_u *)8L},
	{"shortmess",	"shm",	P_STRING,			(char_u *)&p_shm,
							(char_u *)""},
	{"shortname",	"sn",	P_BOOL|P_IND,
#ifdef SHORT_FNAME
												(char_u *)NULL,
#else
												(char_u *)PV_SN,
#endif
							(char_u *)FALSE},
	{"showbreak",	"sbr",	P_STRING,			(char_u *)&p_sbr,
							(char_u *)""},
	{"showcmd",		"sc",	P_BOOL,				(char_u *)&p_sc,
#if defined(COMPATIBLE) || defined(UNIX)
							(char_u *)FALSE},
#else
							(char_u *)TRUE},
#endif
	{"showmatch",	"sm",	P_BOOL,				(char_u *)&p_sm,
							(char_u *)FALSE},
	{"showmode",	"smd",	P_BOOL,				(char_u *)&p_smd,
#if defined(COMPATIBLE)
							(char_u *)FALSE},
#else
							(char_u *)TRUE},
#endif
	{"sidescroll",	"ss",	P_NUM,				(char_u *)&p_ss,
							(char_u *)0L},
	{"slowopen",	"slow",	P_BOOL,				(char_u *)NULL,
							(char_u *)FALSE},
	{"smartcase",	"scs",	P_BOOL,				(char_u *)&p_scs,
							(char_u *)FALSE},
#ifdef SMARTINDENT
	{"smartindent",	"si",	P_BOOL|P_IND,		(char_u *)PV_SI,
							(char_u *)FALSE},
#endif
	{"smarttab",	"sta",	P_BOOL,				(char_u *)&p_sta,
							(char_u *)FALSE},
	{"sourceany",	NULL,	P_BOOL,				(char_u *)NULL,
							(char_u *)FALSE},
	{"splitbelow",	"sb",	P_BOOL,				(char_u *)&p_sb,
							(char_u *)FALSE},
	{"startofline",	"sol",	P_BOOL,				(char_u *)&p_sol,
							(char_u *)TRUE},
	{"suffixes",	"su",	P_STRING,			(char_u *)&p_su,
							(char_u *)".bak,~,.o,.h,.info,.swp"},
	{"swapsync",	"sws",	P_STRING,			(char_u *)&p_sws,
							(char_u *)"fsync"},
	{"tabstop",		"ts",	P_NUM|P_IND,		(char_u *)PV_TS,
							(char_u *)8L},
	{"taglength",	"tl",	P_NUM,				(char_u *)&p_tl,
							(char_u *)0L},
	{"tagrelative",	"tr",	P_BOOL,				(char_u *)&p_tr,
#if defined(COMPATIBLE)
							(char_u *)FALSE},
#else
							(char_u *)TRUE},
#endif
	{"tags",		"tag",	P_STRING|P_EXPAND,	(char_u *)&p_tags,
#ifdef EMACS_TAGS
							(char_u *)"./tags,./TAGS,tags,TAGS"},
#else
							(char_u *)"./tags,tags"},
#endif
	{"tagstack",	"tgst",	P_BOOL,				(char_u *)NULL,
							(char_u *)FALSE},
	{"term",		NULL,	P_STRING|P_EXPAND|P_NODEFAULT|P_NO_MKRC,
											(char_u *)&term_strings[KS_NAME],
							(char_u *)""},
	{"terse",		NULL,	P_BOOL,				(char_u *)&p_terse,
							(char_u *)FALSE},
	{"textauto",	"ta",	P_BOOL,				(char_u *)&p_ta,
#if defined(COMPATIBLE)
							(char_u *)FALSE},
#else
							(char_u *)TRUE},
#endif
	{"textmode",	"tx",	P_BOOL|P_IND,		(char_u *)PV_TX,
#ifdef USE_CRNL
							(char_u *)TRUE},
#else
							(char_u *)FALSE},
#endif
	{"textwidth",	"tw",	P_NUM|P_IND,		(char_u *)PV_TW,
							(char_u *)0L},
	{"tildeop",		"top",	P_BOOL,				(char_u *)&p_to,
							(char_u *)FALSE},
	{"timeout", 	"to",	P_BOOL,				(char_u *)&p_timeout,
							(char_u *)TRUE},
	{"timeoutlen",	"tm",	P_NUM,				(char_u *)&p_tm,
							(char_u *)1000L},
	{"title",	 	NULL,	P_BOOL,				(char_u *)&p_title,
							(char_u *)FALSE},
	{"titlelen",	NULL,	P_NUM,				(char_u *)&p_titlelen,
							(char_u *)85L},
	{"ttimeout", 	NULL,	P_BOOL,				(char_u *)&p_ttimeout,
							(char_u *)FALSE},
	{"ttimeoutlen",	"ttm",	P_NUM,				(char_u *)&p_ttm,
							(char_u *)-1L},
	{"ttybuiltin",	"tbi",	P_BOOL,				(char_u *)&p_tbi,
							(char_u *)TRUE},
	{"ttyfast",		"tf",	P_BOOL|P_NO_MKRC,	(char_u *)&p_tf,
							(char_u *)FALSE},
	{"ttyscroll",	"tsl",	P_NUM,				(char_u *)&p_ttyscroll,
							(char_u *)999L},
	{"ttytype",		"tty",	P_STRING|P_EXPAND|P_NODEFAULT|P_NO_MKRC,
											(char_u *)&term_strings[KS_NAME],
							(char_u *)""},
	{"undolevels",	"ul",	P_NUM,				(char_u *)&p_ul,
#ifdef COMPATIBLE
							(char_u *)0L},
#else
# if defined(UNIX) || defined(WIN32) || defined(OS2)
							(char_u *)1000L},
# else
							(char_u *)100L},
# endif
#endif
	{"updatecount",	"uc",	P_NUM,				(char_u *)&p_uc,
#ifdef COMPATIBLE
							(char_u *)0L},
#else
							(char_u *)200L},
#endif
	{"updatetime",	"ut",	P_NUM,				(char_u *)&p_ut,
							(char_u *)4000L},
	{"viminfo",		"vi",	P_STRING,
#ifdef VIMINFO
												(char_u *)&p_viminfo,
#else
												(char_u *)NULL,
#endif /* VIMINFO */
							(char_u *)""},
	{"visualbell",	"vb",	P_BOOL,				(char_u *)&p_vb,
							(char_u *)FALSE},
	{"w300",		NULL, 	P_NUM,				(char_u *)NULL,
							(char_u *)0L},
	{"w1200",		NULL, 	P_NUM,				(char_u *)NULL,
							(char_u *)0L},
	{"w9600",		NULL, 	P_NUM,				(char_u *)NULL,
							(char_u *)0L},
	{"warn",		NULL,	P_BOOL,				(char_u *)&p_warn,
							(char_u *)TRUE},
	{"weirdinvert",	"wiv",	P_BOOL,				(char_u *)&p_wiv,
							(char_u *)FALSE},
	{"whichwrap",	"ww",	P_STRING,			(char_u *)&p_ww,
#ifdef COMPATIBLE
							(char_u *)""},
#else
							(char_u *)"b,s"},
#endif
	{"wildchar",	"wc", 	P_NUM,				(char_u *)&p_wc,
#ifdef COMPATIBLE
							(char_u *)(long)Ctrl('E')},
#else
							(char_u *)(long)TAB},
#endif
	{"window",		"wi", 	P_NUM,				(char_u *)NULL,
							(char_u *)0L},
	{"winheight",	"wh",	P_NUM,				(char_u *)&p_wh,
							(char_u *)0L},
	{"wrap",		NULL,	P_BOOL|P_IND,		(char_u *)PV_WRAP,
							(char_u *)TRUE},
	{"wrapmargin",	"wm",	P_NUM|P_IND,		(char_u *)PV_WM,
							(char_u *)0L},
	{"wrapscan",	"ws",	P_BOOL,				(char_u *)&p_ws,
							(char_u *)TRUE},
	{"writeany",	"wa",	P_BOOL,				(char_u *)&p_wa,
							(char_u *)FALSE},
	{"writebackup",	"wb",	P_BOOL,				(char_u *)&p_wb,
#if defined(COMPATIBLE) && !defined(WRITEBACKUP)
							(char_u *)FALSE},
#else
							(char_u *)TRUE},
#endif
	{"writedelay",	"wd",	P_NUM,				(char_u *)&p_wd,
							(char_u *)0L},

/* terminal output codes */
	{"t_AL",		NULL,	P_STRING,	(char_u *)&term_strings[KS_CAL],
							(char_u *)""},
	{"t_al",		NULL,	P_STRING,	(char_u *)&term_strings[KS_AL],
							(char_u *)""},
	{"t_cd",		NULL,	P_STRING,	(char_u *)&term_strings[KS_CD],
							(char_u *)""},
	{"t_ce",		NULL,	P_STRING,	(char_u *)&term_strings[KS_CE],
							(char_u *)""},
	{"t_cl",		NULL,	P_STRING,	(char_u *)&term_strings[KS_CL],
							(char_u *)""},
	{"t_cm",		NULL,	P_STRING,	(char_u *)&term_strings[KS_CM],
							(char_u *)""},
	{"t_CS",		NULL,	P_STRING,	(char_u *)&term_strings[KS_CSC],
							(char_u *)""},
	{"t_cs",		NULL,	P_STRING,	(char_u *)&term_strings[KS_CS],
							(char_u *)""},
	{"t_da",		NULL,	P_STRING,	(char_u *)&term_strings[KS_DA],
							(char_u *)""},
	{"t_db",		NULL,	P_STRING,	(char_u *)&term_strings[KS_DB],
							(char_u *)""},
	{"t_DL",		NULL,	P_STRING,	(char_u *)&term_strings[KS_CDL],
							(char_u *)""},
	{"t_dl",		NULL,	P_STRING,	(char_u *)&term_strings[KS_DL],
							(char_u *)""},
	{"t_ke",		NULL,	P_STRING,	(char_u *)&term_strings[KS_KE],
							(char_u *)""},
	{"t_ks",		NULL,	P_STRING,	(char_u *)&term_strings[KS_KS],
							(char_u *)""},
	{"t_md",		NULL,	P_STRING,	(char_u *)&term_strings[KS_MD],
							(char_u *)""},
	{"t_me",		NULL,	P_STRING,	(char_u *)&term_strings[KS_ME],
							(char_u *)""},
	{"t_mr",		NULL,	P_STRING,	(char_u *)&term_strings[KS_MR],
							(char_u *)""},
	{"t_ms",		NULL,	P_STRING,	(char_u *)&term_strings[KS_MS],
							(char_u *)""},
	{"t_RI",		NULL,	P_STRING,	(char_u *)&term_strings[KS_CRI],
							(char_u *)""},
	{"t_se",		NULL,	P_STRING,	(char_u *)&term_strings[KS_SE],
							(char_u *)""},
	{"t_so",		NULL,	P_STRING,	(char_u *)&term_strings[KS_SO],
							(char_u *)""},
	{"t_sr",		NULL,	P_STRING,	(char_u *)&term_strings[KS_SR],
							(char_u *)""},
	{"t_te",		NULL,	P_STRING,	(char_u *)&term_strings[KS_TE],
							(char_u *)""},
	{"t_ti",		NULL,	P_STRING,	(char_u *)&term_strings[KS_TI],
							(char_u *)""},
	{"t_ue",		NULL,	P_STRING,	(char_u *)&term_strings[KS_UE],
							(char_u *)""},
	{"t_us",		NULL,	P_STRING,	(char_u *)&term_strings[KS_US],
							(char_u *)""},
	{"t_vb",		NULL,	P_STRING,	(char_u *)&term_strings[KS_VB],
							(char_u *)""},
	{"t_ve",		NULL,	P_STRING,	(char_u *)&term_strings[KS_VE],
							(char_u *)""},
	{"t_vi",		NULL,	P_STRING,	(char_u *)&term_strings[KS_VI],
							(char_u *)""},
	{"t_vs",		NULL,	P_STRING,	(char_u *)&term_strings[KS_VS],
							(char_u *)""},
	{"t_ZH",		NULL,	P_STRING,	(char_u *)&term_strings[KS_CZH],
							(char_u *)""},
	{"t_ZR",		NULL,	P_STRING,	(char_u *)&term_strings[KS_CZR],
							(char_u *)""},

/* terminal key codes are not here */

	{NULL, NULL, 0, NULL, NULL}			/* end marker */
};

#define PARAM_COUNT (sizeof(options) / sizeof(struct option))

#ifdef AUTOCMD
/*
 * structures for automatic commands
 */

typedef struct AutoCmd
{
	char_u			*cmd;					/* The command to be executed */
	struct AutoCmd	*next;					/* Next AutoCmd in list */
} AutoCmd;

typedef struct AutoPat
{
	char_u			*pat;					/* pattern as typed */
	char_u			*reg_pat;				/* pattern converted to regexp */
	int				allow_directories;		/* Pattern may match whole path */
	AutoCmd			*cmds;					/* list of commands to do */
	struct AutoPat	*next;					/* next AutoPat in AutoPat list */
} AutoPat;

static struct event_name
{
	char	*name;		/* event name */
	int		event;		/* event number */
} event_names[] =
{
	{"BufEnter",		EVENT_BUFENTER},
	{"BufLeave",		EVENT_BUFLEAVE},
	{"BufNewFile",		EVENT_BUFNEWFILE},
	{"BufReadPost",		EVENT_BUFREADPOST},
	{"BufReadPre",		EVENT_BUFREADPRE},
	{"BufRead", 		EVENT_BUFREADPOST},
	{"BufWritePost", 	EVENT_BUFWRITEPOST},
	{"BufWritePre",		EVENT_BUFWRITEPRE},
	{"BufWrite",		EVENT_BUFWRITEPRE},
	{"FileAppendPost",	EVENT_FILEAPPENDPOST},
	{"FileAppendPre",	EVENT_FILEAPPENDPRE},
	{"FileReadPost",	EVENT_FILEREADPOST},
	{"FileReadPre",		EVENT_FILEREADPRE},
	{"FileWritePost",	EVENT_FILEWRITEPOST},
	{"FileWritePre",	EVENT_FILEWRITEPRE},
	{"FilterReadPost",	EVENT_FILTERREADPOST},
	{"FilterReadPre",	EVENT_FILTERREADPRE},
	{"FilterWritePost",	EVENT_FILTERWRITEPOST},
	{"FilterWritePre",	EVENT_FILTERWRITEPRE},
	{"VimLeave",		EVENT_VIMLEAVE},
	{"WinEnter",		EVENT_WINENTER},
	{"WinLeave",		EVENT_WINLEAVE},
	{NULL,			0}
};

static AutoPat *first_autopat[NUM_EVENTS] =
{
	NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL
};
#endif

static void set_option_default __ARGS((int, int));
static void illegal_char __ARGS((char_u *, int));
static char_u *option_expand __ARGS((int));
static int findoption __ARGS((char_u *));
static int find_key_option __ARGS((char_u *));
static void	showoptions __ARGS((int));
static int option_changed __ARGS((struct option *));
static void showoneopt __ARGS((struct option *));
static int  istermoption __ARGS((struct option *));
static char_u *get_varp __ARGS((struct option *));
static void option_value2string __ARGS((struct option *));
#ifdef HAVE_LANGMAP
static void langmap_init __ARGS((void));
static void langmap_set __ARGS((void));
#endif
static void paste_option_changed __ARGS((void));
static void p_compatible_set __ARGS((void));
static void fill_breakat_flags __ARGS((void));

/*
 * Initialize the options, first part.
 *
 * Called only once from main(), just after creating the first buffer.
 */
	void
set_init_1()
{
	char_u	*p;
	int		opt_idx;
	long	n;

#ifdef HAVE_LANGMAP
	langmap_init();
#endif

/*
 * Find default value for 'shell' option.
 */
	if ((p = vim_getenv((char_u *)"SHELL")) != NULL
#if defined(MSDOS) || defined(WIN32) || defined(OS2)
# ifdef __EMX__
			|| (p = vim_getenv((char_u *)"EMXSHELL")) != NULL
# endif
			|| (p = vim_getenv((char_u *)"COMSPEC")) != NULL
# ifdef WIN32
			|| (p = default_shell()) != NULL
# endif
#endif
															)
	{
		p = strsave(p);
		if (p != NULL)			/* we don't want a NULL */
		{
			opt_idx = findoption((char_u *)"sh");
			options[opt_idx].def_val = p;
			options[opt_idx].flags |= P_DEF_ALLOCED;
		}
	}

/*
 * Set default for 'helpfile' option. This cannot be done at compile time,
 * because for Unix it is an external variable.
 */
	opt_idx = findoption((char_u *)"hf");
#if defined(HAVE_CONFIG_H) || defined(OS2)
	options[opt_idx].def_val = help_fname;
#else
	options[opt_idx].def_val = (char_u *)VIM_HLP;
#endif

/*
 * 'maxmemtot' and 'maxmem' may have to be adjusted for available memory
 */
	opt_idx = findoption((char_u *)"maxmemtot");
	if (options[opt_idx].def_val == (char_u *)0L)
	{
		n = (mch_avail_mem(FALSE) >> 11);
		options[opt_idx].def_val = (char_u *)n;
		opt_idx = findoption((char_u *)"maxmem");
		if ((long)options[opt_idx].def_val > n ||
										 (long)options[opt_idx].def_val == 0L)
			options[opt_idx].def_val = (char_u *)n;
	}

/*
 * set all the options (except the terminal options) to their default value
 */
	for (opt_idx = 0; !istermoption(&options[opt_idx]); opt_idx++)
		if (!(options[opt_idx].flags & P_NODEFAULT))
			set_option_default(opt_idx, FALSE);

	curbuf->b_p_initialized = TRUE;
	check_buf_options(curbuf);
	check_options();

	/*
	 * initialize the table for 'iskeyword' et.al.
	 * Must be before option_expand(), because that one needs isidchar()
	 */
	init_chartab();

	/*
	 * initialize the table for 'breakat'.
	 */
	fill_breakat_flags();

	/*
	 * Expand environment variables and things like "~" for the defaults.
	 * If option_expand() returns non-NULL the variable is expanded. This can
	 * only happen for non-indirect options.
	 * Also set the default to the expanded value, so ":set" does not list
	 * them. Don't set the P_ALLOCED flag, because we don't want to free the
	 * default.
	 */
	for (opt_idx = 0; !istermoption(&options[opt_idx]); opt_idx++)
	{
		p = option_expand(opt_idx);
		if (p != NULL)
		{
			*(char_u **)options[opt_idx].var = p;
			options[opt_idx].def_val = p;
			options[opt_idx].flags |= P_DEF_ALLOCED;
		}
	}
}

/*
 * Set an option to its default value.
 */
	static void
set_option_default(opt_idx, dofree)
	int		opt_idx;
	int		dofree;		/* TRUE when old value may be freed */
{
	char_u		*varp;			/* pointer to variable for current option */

	varp = get_varp(&(options[opt_idx]));
	if (varp != NULL)		/* nothing to do for hidden option */
	{
		if (options[opt_idx].flags & P_STRING)
		{
			/* indirect options are always in allocated memory */
			if (options[opt_idx].flags & P_IND)
				set_string_option(NULL, opt_idx,
											options[opt_idx].def_val, dofree);
			else
			{
				if (dofree && (options[opt_idx].flags & P_ALLOCED))
					free_string_option(*(char_u **)(varp));
				*(char_u **)varp = options[opt_idx].def_val;
				options[opt_idx].flags &= ~P_ALLOCED;
			}
		}
		else if (options[opt_idx].flags & P_NUM)
			*(long *)varp = (long)options[opt_idx].def_val;
		else	/* P_BOOL */
				/* the cast to long is required for Manx C */
			*(int *)varp = (int)(long)options[opt_idx].def_val;
	}
}

/*
 * Initialize the options, part two: After getting Rows and Columns
 */
	void
set_init_2()
{
/*
 * 'scroll' defaults to half the window height. Note that this default is
 * wrong when the window height changes.
 */
	options[findoption((char_u *)"scroll")].def_val = (char_u *)(Rows >> 1);

	comp_col();
}

/*
 * Initialize the options, part three: After reading the .vimrc
 */
	void
set_init_3()
{
	int		idx1;

#if defined(UNIX) || defined(OS2)
/*
 * Set 'shellpipe' and 'shellredir', depending on the 'shell' option.
 * This is done after other initializations, where 'shell' might have been
 * set, but only if they have not been set before.
 */
	char_u	*p;
	int		idx2;
	int		do_sp;
	int		do_srr;

	idx1 = findoption((char_u *)"sp");
	idx2 = findoption((char_u *)"srr");
	do_sp = !(options[idx1].flags & P_WAS_SET);
	do_srr = !(options[idx2].flags & P_WAS_SET);

	/*
	 * Default for p_sp is "| tee", for p_srr is ">".
	 * For known shells it is changed here to include stderr.
	 */
	p = gettail(p_sh);
	if (	fnamecmp(p, "csh") == 0 ||
			fnamecmp(p, "tcsh") == 0
# ifdef OS2			/* also check with .exe extension */
			|| fnamecmp(p, "csh.exe") == 0
			|| fnamecmp(p, "tcsh.exe") == 0
# endif
											)
	{
		if (do_sp)
		{
			p_sp = (char_u *)"|& tee";
			options[idx1].def_val = p_sp;
		}
		if (do_srr)
		{
			p_srr = (char_u *)">&";
			options[idx2].def_val = p_srr;
		}
	}
	else
# ifndef OS2	/* Always use bourne shell style redirection if we reach this */
		if (	STRCMP(p, "sh") == 0 ||
				STRCMP(p, "ksh") == 0 ||
				STRCMP(p, "zsh") == 0 ||
				STRCMP(p, "bash") == 0)
# endif
	{
		if (do_sp)
		{
			p_sp = (char_u *)"2>&1| tee";
			options[idx1].def_val = p_sp;
		}
		if (do_srr)
		{
			p_srr = (char_u *)">%s 2>&1";
			options[idx2].def_val = p_srr;
		}
	}
#endif

#if defined(MSDOS) || defined(WIN32)
	/*
	 * Set 'shellcmdflag and 'shellquote' depending on the 'shell' option.
	 * This is done after other initializations, where 'shell' might have been
	 * set, but only if they have not been set before.  Default for p_shcf is
	 * "/c", for p_shq is "".  For "sh" like  shells it is changed here to
	 * "-c" and "\"".
	 */
	if (strstr((char *)p_sh, "sh") != NULL)
	{
		idx1 = findoption((char_u *)"shcf");
		if (!(options[idx1].flags & P_WAS_SET))
		{
			p_shcf = (char_u *)"-c";
			options[idx1].def_val = p_shcf;
		}

		idx1 = findoption((char_u *)"shq");
		if (!(options[idx1].flags & P_WAS_SET))
		{
			p_shq = (char_u *)"\"";
			options[idx1].def_val = p_shq;
		}
	}
#endif

/*
 * 'title' and 'icon' only default to true if they have not been set or reset
 * in .vimrc and we can read the old value.
 * When 'title' and 'icon' have been reset in .vimrc, we won't even check if
 * they can be reset.  This reduces startup time when using X on a remote
 * machine.
 */
	idx1 = findoption((char_u *)"title");
	if (!(options[idx1].flags & P_WAS_SET) && mch_can_restore_title())
	{
		options[idx1].def_val = (char_u *)TRUE;
		p_title = TRUE;
	}
	idx1 = findoption((char_u *)"icon");
	if (!(options[idx1].flags & P_WAS_SET) && mch_can_restore_icon())
	{
		options[idx1].def_val = (char_u *)TRUE;
		p_icon = TRUE;
	}
}

/*
 * Parse 'arg' for option settings.
 *
 * 'arg' may be IObuff, but only when no errors can be present and option
 * does not need to be expanded with option_expand().
 *
 * return FAIL if errors are detected, OK otherwise
 */
	int
do_set(arg)
	char_u		*arg;	/* option string (may be written to!) */
{
	register int opt_idx;
	char_u		*errmsg;
	char_u		errbuf[80];
	char_u		*startarg;
	int			prefix;	/* 1: nothing, 0: "no", 2: "inv" in front of name */
	int 		nextchar;			/* next non-white char after option name */
	int			afterchar;			/* character just after option name */
	int 		len;
	int			i;
	int			key;
	int 		flags;				/* flags for current option */
	char_u		*varp = NULL;		/* pointer to variable for current option */
	char_u		*oldval;			/* previous value if *varp */
	int			errcnt = 0;			/* number of errornous entries */
	long		oldRows = Rows;		/* remember old Rows */
	long		oldColumns = Columns;	/* remember old Columns */
	int			oldbin;				/* remember old bin option */
	long		oldch = p_ch;		/* remember old command line height */
	int			oldea = p_ea;		/* remember old 'equalalways' */
	long		olduc = p_uc;		/* remember old 'updatecount' */
	int			did_show = FALSE;	/* already showed one value */
	WIN			*wp;

	if (*arg == NUL)
	{
		showoptions(0);
		return OK;
	}

	while (*arg)		/* loop to process all options */
	{
		errmsg = NULL;
		startarg = arg;		/* remember for error message */

		if (STRNCMP(arg, "all", (size_t)3) == 0)
		{
			showoptions(1);
			arg += 3;
		}
		else if (STRNCMP(arg, "termcap", (size_t)7) == 0)
		{
			showoptions(2);
			show_termcodes();
			arg += 7;
		}
		else
		{
			prefix = 1;
			if (STRNCMP(arg, "no", (size_t)2) == 0)
			{
				prefix = 0;
				arg += 2;
			}
			else if (STRNCMP(arg, "inv", (size_t)3) == 0)
			{
				prefix = 2;
				arg += 3;
			}
				/* find end of name */
			if (*arg == '<')
			{
				opt_idx = -1;
				/* check for <t_>;> */
				if (arg[1] == 't' && arg[2] == '_' && arg[3] && arg[4])
					len = 5;
				else
				{
					len = 1;
					while (arg[len] != NUL && arg[len] != '>')
						++len;
				}
				if (arg[len] != '>')
				{
					errmsg = e_invarg;
					goto skip;
				}
				nextchar = arg[len];
				arg[len] = NUL;						/* put NUL after name */
				if (arg[1] == 't' && arg[2] == '_')	/* could be term code */
					opt_idx = findoption(arg + 1);
				key = 0;
				if (opt_idx == -1)
					key = find_key_option(arg + 1);
				arg[len++] = nextchar;				/* restore nextchar */
				nextchar = arg[len];
			}
			else
			{
				len = 0;
				/*
				 * The two characters after "t_" may not be alphanumeric.
				 */
				if (arg[0] == 't' && arg[1] == '_' && arg[2] && arg[3])
				{
					len = 4;
				}
				else
				{
					while (isalnum(arg[len]) || arg[len] == '_')
						++len;
				}
				nextchar = arg[len];
				arg[len] = NUL;						/* put NUL after name */
				opt_idx = findoption(arg);
				key = 0;
				if (opt_idx == -1)
					key = find_key_option(arg);
				arg[len] = nextchar;				/* restore nextchar */
			}

			if (opt_idx == -1 && key == 0)		/* found a mismatch: skip */
			{
				errmsg = (char_u *)"Unknown option";
				goto skip;
			}

			if (opt_idx >= 0)
			{
				if (options[opt_idx].var == NULL)	/* hidden option: skip */
					goto skip;

				flags = options[opt_idx].flags;
				varp = get_varp(&(options[opt_idx]));
			}
			else
				flags = P_STRING;

			/* remember character after option name */
			afterchar = nextchar;

			/* skip white space, allow ":set ai  ?" */
			while (vim_iswhite(nextchar))
				nextchar = arg[++len];

			if (vim_strchr((char_u *)"?=:!&", nextchar) != NULL)
			{
				arg += len;
				len = 0;
			}

			/*
			 * allow '=' and ':' as MSDOS command.com allows only one
			 * '=' character per "set" command line. grrr. (jw)
			 */
			if (nextchar == '?' || (prefix == 1 && vim_strchr((char_u *)"=:&",
									  nextchar) == NULL && !(flags & P_BOOL)))
			{										/* print value */
				if (did_show)
					msg_outchar('\n');		/* cursor below last one */
				else
				{
					gotocmdline(TRUE);		/* cursor at status line */
					did_show = TRUE;		/* remember that we did a line */
				}
				if (opt_idx >= 0)
					showoneopt(&options[opt_idx]);
				else
				{
					char_u			name[2];
					char_u			*p;

					name[0] = KEY2TERMCAP0(key);
					name[1] = KEY2TERMCAP1(key);
					p = find_termcode(name);
					if (p == NULL)
					{
						errmsg = (char_u *)"Unknown option";
						goto skip;
					}
					else
						(void)show_one_termcode(name, p, TRUE);
				}
				if (nextchar != '?' && nextchar != NUL &&
													  !vim_iswhite(afterchar))
					errmsg = e_trailing;
			}
			else
			{
				if (flags & P_BOOL)					/* boolean */
				{
					if (nextchar == '=' || nextchar == ':')
					{
						errmsg = e_invarg;
						goto skip;
					}

					/*
					 * in secure mode, setting of the secure option is not
					 * allowed
					 */
					if (secure && (int *)varp == &p_secure)
					{
						errmsg = (char_u *)"not allowed here";
						goto skip;
					}

					oldbin = curbuf->b_p_bin;	/* remember old bin option */

					/*
					 * ":set opt!" or ":set invopt": invert
					 * ":set opt&": reset to default value
					 * ":set opt" or ":set noopt": set or reset
					 */
					if (prefix == 2 || nextchar == '!')
						*(int *)(varp) ^= 1;
					else if (nextchar == '&')
								/* the cast to long is required for Manx C */
						*(int *)(varp) = (int)(long)options[opt_idx].def_val;
					else
						*(int *)(varp) = prefix;

					/* handle the setting of the compatible option */
					if ((int *)varp == &p_cp && p_cp)
					{
						p_compatible_set();
					}
					/* when 'readonly' is reset, also reset readonlymode */
					else if ((int *)varp == &curbuf->b_p_ro && !curbuf->b_p_ro)
						readonlymode = FALSE;

					/* when 'bin' is set also set some other options */
					else if ((int *)varp == &curbuf->b_p_bin)
					{
						set_options_bin(oldbin, curbuf->b_p_bin);
					}
					/* when 'terse' is set change 'shortmess' */
					else if ((int *)varp == &p_terse)
					{
						char_u	*p;

						p = vim_strchr(p_shm, SHM_SEARCH);

						/* insert 's' in p_shm */
						if (p_terse && p == NULL)
						{
							STRCPY(IObuff, p_shm);
							STRCAT(IObuff, "s");
							set_string_option((char_u *)"shm", -1,
																IObuff, TRUE);
						}
						/* remove 's' from p_shm */
						else if (!p_terse && p != NULL)
							vim_memmove(p, p + 1, STRLEN(p));
					}
					/* when 'paste' is set or reset also change other options */
					else if ((int *)varp == &p_paste)
					{
						paste_option_changed();
					}
					/*
					 * When 'lisp' option changes include/exclude '-' in
					 * keyword characters.
					 */
					else if (varp == (char_u *)&(curbuf->b_p_lisp))
						init_chartab();		/* ignore errors */

					else if (!starting &&
#ifdef USE_GUI
								!gui.starting &&
#endif
						  ((int *)varp == &p_title || (int *)varp == &p_icon))
					{
						/*
						 * When setting 'title' or 'icon' on, call maketitle()
						 * to create and display it.
						 * When resetting 'title' or 'icon', call maketitle()
						 * to clear it and call mch_restore_title() to get the
						 * old value back.
						 */
						maketitle();
						if (!*(int *)varp)
							mch_restore_title((int *)varp == &p_title ? 1 : 2);
					}
				}
				else								/* numeric or string */
				{
					if (vim_strchr((char_u *)"=:&", nextchar) == NULL ||
																  prefix != 1)
					{
						errmsg = e_invarg;
						goto skip;
					}
					if (flags & P_NUM)				/* numeric */
					{
						/*
						 * Different ways to set a number option:
						 * &		set to default value
						 * <xx>		accept special key codes for 'wildchar'
						 * c        accept any non-digit for 'wildchar'
						 * 0-9		set number
						 * other	error
						 */
						arg += len + 1;
						if (nextchar == '&')
							*(long *)(varp) = (long)options[opt_idx].def_val;
						else if ((long *)varp == &p_wc &&
												(*arg == '<' || *arg == '^' ||
										  ((!arg[1] || vim_iswhite(arg[1])) &&
															 !isdigit(*arg))))
						{
							if (*arg == '<')
							{
								i = get_special_key_code(arg + 1);
								if (i == 0)
									i = find_key_option(arg + 1);
							}
							else if (*arg == '^')
								i = arg[1] ^ 0x40;
							else
								i = *arg;
							if (i == 0)
							{
								errmsg = e_invarg;
								goto skip;
							}
							p_wc = i;
						}
								/* allow negative numbers (for 'undolevels') */
						else if (*arg == '-' || isdigit(*arg))
						{
							i = 0;
							if (*arg == '-')
								i = 1;
#ifdef HAVE_STRTOL
							*(long *)(varp) = strtol((char *)arg, NULL, 0);
							if (arg[i] == '0' && TO_UPPER(arg[i + 1]) == 'X')
								i += 2;
#else
							*(long *)(varp) = atol((char *)arg);
#endif
							while (isdigit(arg[i]))
								++i;
							if (arg[i] != NUL && !vim_iswhite(arg[i]))
							{
								errmsg = e_invarg;
								goto skip;
							}
						}
						else
						{
							errmsg = (char_u *)"Number required after =";
							goto skip;
						}

						/*
						 * Number options that need some action when changed
						 */
						if ((long *)varp == &p_wh || (long *)varp == &p_hh)
						{
							if (p_wh < 0)
							{
								errmsg = e_positive;
								p_wh = 0;
							}
							if (p_hh < 0)
							{
								errmsg = e_positive;
								p_hh = 0;
							}
								/* Change window height NOW */
							if (p_wh && lastwin != firstwin)
							{
								win_equal(curwin, FALSE);
								must_redraw = CLEAR;
							}
						}
						/* (re)set last window status line */
						if ((long *)varp == &p_ls)
							last_status();
						if ((long *)varp == &p_titlelen && !starting)
							maketitle();
					}
					else if (opt_idx >= 0)					/* string */
					{
						char_u		*save_arg = NULL;
						char_u		*s, *p;
						int			new_value_alloced;	/* new string option
														   was allocated */

						/* The old value is kept until we are sure that the new
						 * value is valid. set_option_default() is therefore
						 * called with FALSE
						 */
						oldval = *(char_u **)(varp);
						if (nextchar == '&')		/* set to default val */
						{
							set_option_default(opt_idx, FALSE);
							new_value_alloced =
										 (options[opt_idx].flags & P_ALLOCED);
						}
						else
						{
							arg += len + 1;	/* jump to after the '=' or ':' */

							/*
							 * Convert 'whichwrap' number to string, for
							 * backwards compatibility with Vim 3.0.
							 * Misuse errbuf[] for the resulting string.
							 */
							if (varp == (char_u *)&p_ww && isdigit(*arg))
							{
								*errbuf = NUL;
								i = getdigits(&arg);
								if (i & 1)
									STRCAT(errbuf, "b,");
								if (i & 2)
									STRCAT(errbuf, "s,");
								if (i & 4)
									STRCAT(errbuf, "h,l,");
								if (i & 8)
									STRCAT(errbuf, "<,>,");
								if (i & 16)
									STRCAT(errbuf, "[,],");
								if (*errbuf != NUL)		/* remove trailing , */
									errbuf[STRLEN(errbuf) - 1] = NUL;
								save_arg = arg;
								arg = errbuf;
							}
							/*
							 * Remove '>' before 'dir' and 'bdir', for
							 * backwards compatibility with version 3.0
							 */
							else if (*arg == '>' && (varp == (char_u *)&p_dir ||
												   varp == (char_u *)&p_bdir))
							{
								++arg;
							}

							/*
							 * Copy the new string into allocated memory.
							 * Can't use set_string_option(), because we need
							 * to remove the backslashes.
							 */
												/* get a bit too much */
							s = alloc((unsigned)(STRLEN(arg) + 1));
							if (s == NULL)	/* out of memory, don't change */
								break;
							*(char_u **)(varp) = s;

							/*
							 * Copy the string, skip over escaped chars.
							 * For MS-DOS and WIN32 backslashes before normal
							 * file name characters are not removed.
							 */
							while (*arg && !vim_iswhite(*arg))
							{
								if (*arg == '\\' && arg[1] != NUL
#ifdef BACKSLASH_IN_FILENAME
										&& !((flags & P_EXPAND)
												&& isfilechar(arg[1])
												&& arg[1] != '\\')
#endif
																	)
									++arg;
								*s++ = *arg++;
							}
							*s = NUL;
							if (save_arg != NULL)	/* number for 'whichwrap' */
								arg = save_arg;
							new_value_alloced = TRUE;
						}

							/* expand environment variables and ~ */
						s = option_expand(opt_idx);
						if (s != NULL)
						{
							if (new_value_alloced)
								vim_free(*(char_u **)(varp));
							*(char_u **)(varp) = s;
							new_value_alloced = TRUE;
						}

						/*
						 * options that need some action
						 * to perform when changed (jw)
						 */
						if (varp == (char_u *)&term_strings[KS_NAME])
						{
							if (term_strings[KS_NAME][0] == NUL)
								errmsg = (char_u *)"Cannot set 'term' to empty string";
#ifdef USE_GUI
							if (gui.in_use)
								errmsg = (char_u *)"Cannot change term in GUI";
#endif
							else if (set_termname(term_strings[KS_NAME]) ==
																		 FAIL)
								errmsg = (char_u *)"Not found in termcap";
							else
							{
								/* Screen colors may have changed. */
								outstr(T_ME);
								updateScreen(CLEAR);
							}
						}

						else if ((varp == (char_u *)&p_bex ||
													 varp == (char_u *)&p_pm))
						{
							if (STRCMP(*p_bex == '.' ? p_bex + 1 : p_bex,
										 *p_pm == '.' ? p_pm + 1 : p_pm) == 0)
								errmsg = (char_u *)"'backupext' and 'patchmode' are equal";
						}
						/*
						 * 'isident', 'iskeyword', 'isprint or 'isfname'
						 *  option: refill chartab[]
						 * If the new option is invalid, use old value.
						 * 'lisp' option: refill chartab[] for '-' char
						 */
						else if (varp == (char_u *)&p_isi ||
								 varp == (char_u *)&(curbuf->b_p_isk) ||
								 varp == (char_u *)&p_isp ||
								 varp == (char_u *)&p_isf)
						{
							if (init_chartab() == FAIL)
								errmsg = e_invarg;		/* error in value */
						}
						else if (varp == (char_u *)&p_hl)
						{
							/* Check 'highlight' */
							for (s = p_hl; *s; )
							{
								if (vim_strchr((char_u *)"8dehmMnrstvw",
														(i = s[0])) == NULL ||
									vim_strchr((char_u *)"bsnuir",
														(i = s[1])) == NULL ||
											  ((i = s[2]) != NUL && i != ','))
								{
									illegal_char(errbuf, i);
									errmsg = errbuf;
									break;
								}
								if (s[2] == NUL)
									break;
								s = skipwhite(s + 3);
							}
						}
						else if (varp == (char_u *)&(curbuf->b_p_com))
						{
							for (s = curbuf->b_p_com; *s; )
							{
								while (*s && *s != ':')
								{
									if (vim_strchr((char_u *)COM_ALL, *s) == NULL)
									{
										errmsg = (char_u *)"Illegal flag";
										break;
									}
									++s;
								}
								if (*s++ == NUL)
									errmsg = (char_u *)"Missing colon";
								else if (*s == ',' || *s == NUL)
									errmsg = (char_u *)"Zero length string";
								if (errmsg != NULL)
									break;
								while (*s && *s != ',')
								{
									if (*s == '\\' && s[1] != NUL)
										++s;
									++s;
								}
								s = skip_to_option_part(s);
							}
						}
#ifdef VIMINFO
						else if (varp == (char_u *)&(p_viminfo))
						{
							for (s = p_viminfo; *s;)
							{
								/* Check it's a valid character */
								if (vim_strchr((char_u *)"\"'fr:/", *s) == NULL)
								{
									illegal_char(errbuf, *s);
									errmsg = errbuf;
									break;
								}
								if (*s == 'r')
								{
									while (*++s && *s != ',')
										;
								}
								else
								{
									while (isdigit(*++s))
										;

									/* Must be a number after the character */
									if (!isdigit(*(s - 1)))
									{
										sprintf((char *)errbuf,
												"Missing number after <%s>",
												transchar(*(s - 1)));
										errmsg = errbuf;
										break;
									}
								}
								s = skip_to_option_part(s);
							}
							if (*p_viminfo && errmsg == NULL
											&& get_viminfo_parameter('\'') < 0)
								errmsg = (char_u *)"Must specify a ' value";
						}
#endif /* VIMINFO */
						else if (istermoption(&options[opt_idx]) && full_screen)
						{
							ttest(FALSE);
							if (varp == (char_u *)&term_strings[KS_ME])
							{
								outstr(T_ME);
								updateScreen(CLEAR);
							}
						}
						else if (varp == (char_u *)&p_sbr)
						{
							for (s = p_sbr; *s; ++s)
								if (charsize(*s) != 1)
									errmsg = (char_u *)"contains unprintable character";
						}
#ifdef USE_GUI
						else if (varp == (char_u *)&p_guifont)
						{
							gui_init_font();
						}
#endif /* USE_GUI */
#ifdef HAVE_LANGMAP
						else if (varp == (char_u *)&p_langmap)
							langmap_set();
#endif
						else if (varp == (char_u *)&p_breakat)
							fill_breakat_flags();
						else
						{
							/*
							 * Check options that are a list of flags.
							 */
							p = NULL;
							if (varp == (char_u *)&p_ww)
								p = (char_u *)WW_ALL;
							if (varp == (char_u *)&p_shm)
								p = (char_u *)SHM_ALL;
							else if (varp == (char_u *)&(p_cpo))
								p =	(char_u *)CPO_ALL;
							else if (varp == (char_u *)&(curbuf->b_p_fo))
								p = (char_u *)FO_ALL;
							else if (varp == (char_u *)&p_mouse)
							{
#ifdef USE_MOUSE
								p = (char_u *)MOUSE_ALL;
#else
								if (*p_mouse != NUL)
									errmsg = (char_u *)"No mouse support";
#endif
							}
#ifdef USE_GUI
							else if (varp == (char_u *)&p_guioptions)
								p = (char_u *)GO_ALL;
#endif /* USE_GUI */
							if (p != NULL)
							{
								for (s = *(char_u **)(varp); *s; ++s)
									if (vim_strchr(p, *s) == NULL)
									{
										illegal_char(errbuf, *s);
										errmsg = errbuf;
										break;
									}
							}
						}
						if (errmsg != NULL)	/* error detected */
						{
							if (new_value_alloced)
								vim_free(*(char_u **)(varp));
							*(char_u **)(varp) = oldval;
							(void)init_chartab();	/* back to the old value */
							goto skip;
						}

#ifdef USE_GUI
						if (varp == (char_u *)&p_guioptions)
							gui_init_which_components(oldval);
#endif /* USE_GUI */

						/*
						 * Free string options that are in allocated memory.
						 */
						if (flags & P_ALLOCED)
							free_string_option(oldval);
						if (new_value_alloced)
							options[opt_idx].flags |= P_ALLOCED;
					}
					else			/* key code option */
					{
						char_u		name[2];
						char_u		*p;

						name[0] = KEY2TERMCAP0(key);
						name[1] = KEY2TERMCAP1(key);
						if (nextchar == '&')
						{
							if (add_termcap_entry(name, TRUE) == FAIL)
								errmsg = (char_u *)"Not found in termcap";
						}
						else
						{
							arg += len + 1;	/* jump to after the '=' or ':' */
							for(p = arg; *p && !vim_iswhite(*p); ++p)
							{
								if (*p == '\\' && *(p + 1))
									++p;
							}
							nextchar = *p;
							*p = NUL;
							add_termcode(name, arg);
							*p = nextchar;
						}
						if (full_screen)
							ttest(FALSE);
					}
				}
				if (opt_idx >= 0)
					options[opt_idx].flags |= P_WAS_SET;
			}

skip:
			/*
			 * Check the bounds for numeric options here
			 */
			if (Rows < min_rows() && full_screen)
			{
				sprintf((char *)errbuf, "Need at least %d lines", min_rows());
				errmsg = errbuf;
				Rows = min_rows();
			}
			if (Columns < MIN_COLUMNS && full_screen)
			{
				sprintf((char *)errbuf, "Need at least %d columns",
																 MIN_COLUMNS);
				errmsg = errbuf;
				Columns = MIN_COLUMNS;
			}
			/*
			 * If the screenheight has been changed, assume it is the physical
			 * screenheight.
			 */
			if ((oldRows != Rows || oldColumns != Columns) && full_screen)
			{
				mch_set_winsize();			/* try to change the window size */
				check_winsize();			/* in case 'columns' changed */
#ifdef MSDOS
				set_window();		/* active window may have changed */
#endif
			}

			if (curbuf->b_p_ts <= 0)
			{
				errmsg = e_positive;
				curbuf->b_p_ts = 8;
			}
			if (curbuf->b_p_tw < 0)
			{
				errmsg = e_positive;
				curbuf->b_p_tw = 0;
			}
			if (p_tm < 0)
			{
				errmsg = e_positive;
				p_tm = 0;
			}
			if (p_titlelen <= 0)
			{
				errmsg = e_positive;
				p_titlelen = 85;
			}
			if ((curwin->w_p_scroll <= 0 ||
						curwin->w_p_scroll > curwin->w_height) && full_screen)
			{
				if (curwin->w_p_scroll != 0)
					errmsg = e_scroll;
				win_comp_scroll(curwin);
			}
			if (p_report < 0)
			{
				errmsg = e_positive;
				p_report = 1;
			}
			if ((p_sj < 0 || p_sj >= Rows) && full_screen)
			{
				if (Rows != oldRows)		/* Rows changed, just adjust p_sj */
					p_sj = Rows / 2;
				else
				{
					errmsg = e_scroll;
					p_sj = 1;
				}
			}
			if (p_so < 0 && full_screen)
			{
				errmsg = e_scroll;
				p_so = 0;
			}
			if (p_uc < 0)
			{
				errmsg = e_positive;
				p_uc = 100;
			}
			if (p_ch < 1)
			{
				errmsg = e_positive;
				p_ch = 1;
			}
			if (p_ut < 0)
			{
				errmsg = e_positive;
				p_ut = 2000;
			}
			if (p_ss < 0)
			{
				errmsg = e_positive;
				p_ss = 0;
			}

			/*
			 * Advance to next argument.
			 * - skip until a blank found, taking care of backslashes
			 * - skip blanks
			 */
			while (*arg != NUL && !vim_iswhite(*arg))
				if (*arg++ == '\\' && *arg != NUL)
					++arg;
		}
		arg = skipwhite(arg);

		if (errmsg)
		{
			++no_wait_return;	/* wait_return done below */
#ifdef SLEEP_IN_EMSG
			++dont_sleep;		/* don't wait in emsg() */
#endif
			emsg(errmsg);		/* show error highlighted */
#ifdef SLEEP_IN_EMSG
			--dont_sleep;
#endif
			MSG_OUTSTR(": ");
								/* show argument normal */
			while (startarg < arg)
				msg_outstr(transchar(*startarg++));
			msg_end();			/* check for scrolling */
			--no_wait_return;

			++errcnt;			/* count number of errors */
			did_show = TRUE;	/* error message counts as show */
			if (sourcing_name != NULL)
				break;
		}
	}

	/*
	 * when 'updatecount' changes from zero to non-zero, open swap files
	 */
	if (p_uc && !olduc)
		ml_open_files();

	if (p_ch != oldch)				/* p_ch changed value */
		command_height();
#ifdef USE_MOUSE
	if (*p_mouse == NUL)
		mch_setmouse(FALSE);		/* switch mouse off */
	else
		setmouse();					/* in case 'mouse' changed */
#endif
	comp_col();						/* in case 'ruler' or 'showcmd' changed */
	curwin->w_set_curswant = TRUE;	/* in case 'list' changed */

	/*
	 * Update the screen in case we changed something like "tabstop" or
	 * "lines" or "list" that will change its appearance.
	 * Also update the cursor position, in case 'wrap' is changed.
	 */
	for (wp = firstwin; wp; wp = wp->w_next)
		wp->w_redr_status = TRUE;		/* mark all status lines dirty */
	if (p_ea && !oldea)
		win_equal(curwin, FALSE);
	updateScreen(CURSUPD);
	return (errcnt == 0 ? OK : FAIL);
}

	static void
illegal_char(errbuf, c)
	char_u		*errbuf;
	int			c;
{
	sprintf((char *)errbuf, "Illegal character <%s>", (char *)transchar(c));
}

/*
 * set_options_bin -  called when 'bin' changes value.
 */
	void
set_options_bin(oldval, newval)
	int		oldval;
	int		newval;
{
	/*
	 * The option values that are changed when 'bin' changes are
	 * copied when 'bin is set and restored when 'bin' is reset.
	 */
	if (newval)
	{
		if (!oldval)			/* switched on */
		{
			curbuf->b_p_tw_nobin = curbuf->b_p_tw;
			curbuf->b_p_wm_nobin = curbuf->b_p_wm;
			curbuf->b_p_tx_nobin = curbuf->b_p_tx;
			curbuf->b_p_ta_nobin = p_ta;
			curbuf->b_p_ml_nobin = curbuf->b_p_ml;
			curbuf->b_p_et_nobin = curbuf->b_p_et;
		}

		curbuf->b_p_tw = 0;		/* no automatic line wrap */
		curbuf->b_p_wm = 0;		/* no automatic line wrap */
		curbuf->b_p_tx = 0;		/* no text mode */
		p_ta		   = 0;		/* no text auto */
		curbuf->b_p_ml = 0;		/* no modelines */
		curbuf->b_p_et = 0;		/* no expandtab */
	}
	else if (oldval)			/* switched off */
	{
		curbuf->b_p_tw = curbuf->b_p_tw_nobin;
		curbuf->b_p_wm = curbuf->b_p_wm_nobin;
		curbuf->b_p_tx = curbuf->b_p_tx_nobin;
		p_ta		   = curbuf->b_p_ta_nobin;
		curbuf->b_p_ml = curbuf->b_p_ml_nobin;
		curbuf->b_p_et = curbuf->b_p_et_nobin;
	}
}

#ifdef VIMINFO
/*
 * Find the parameter represented by the given character (eg ', :, ", or /),
 * and return its associated value in the 'viminfo' string.  If the parameter
 * is not specified in the string, return -1.
 */
	int
get_viminfo_parameter(type)
	int		type;
{
	char_u	*p;

	p = vim_strchr(p_viminfo, type);
	if (p != NULL && isdigit(*++p))
		return (int)atol((char *)p);
	return -1;
}
#endif

/*
 * Expand environment variables for some string options.
 * These string options cannot be indirect!
 * Return pointer to allocated memory, or NULL when not expanded.
 */
	static char_u *
option_expand(opt_idx)
	int		opt_idx;
{
	char_u		*p;

		/* if option doesn't need expansion or is hidden: nothing to do */
	if (!(options[opt_idx].flags & P_EXPAND) || options[opt_idx].var == NULL)
		return NULL;

	p = *(char_u **)(options[opt_idx].var);

	/*
	 * Expanding this with NameBuff, expand_env() must not be passed IObuff.
	 */
	expand_env(p, NameBuff, MAXPATHL);
	if (STRCMP(NameBuff, p) == 0)	/* they are the same */
		return NULL;

	return strsave(NameBuff);
}

/*
 * Check for string options that are NULL (normally only termcap options).
 */
	void
check_options()
{
	int		opt_idx;
	char_u	**p;

	for (opt_idx = 0; options[opt_idx].fullname != NULL; opt_idx++)
		if ((options[opt_idx].flags & P_STRING) && options[opt_idx].var != NULL)
		{
			p = (char_u **)get_varp(&(options[opt_idx]));
			if (*p == NULL)
				*p = empty_option;
		}
}

/*
 * Check string options in a buffer for NULL value.
 */
	void
check_buf_options(buf)
	BUF		*buf;
{
	if (buf->b_p_fo == NULL)
		buf->b_p_fo = empty_option;
	if (buf->b_p_isk == NULL)
		buf->b_p_isk = empty_option;
	if (buf->b_p_com == NULL)
		buf->b_p_com = empty_option;
#ifdef CINDENT
	if (buf->b_p_cink == NULL)
		buf->b_p_cink = empty_option;
	if (buf->b_p_cino == NULL)
		buf->b_p_cino = empty_option;
#endif
#if defined(SMARTINDENT) || defined(CINDENT)
	if (buf->b_p_cinw == NULL)
		buf->b_p_cinw = empty_option;
#endif
}

/*
 * Free the string allocated for an option.
 * Checks for the string being empty_option. This may happen if we're out of
 * memory, strsave() returned NULL, which was replaced by empty_option by
 * check_options().
 * Does NOT check for P_ALLOCED flag!
 */
	void
free_string_option(p)
	char_u		*p;
{
	if (p != empty_option)
		vim_free(p);
}

/*
 * Set a string option to a new value.
 * The string is copied into allocated memory.
 * If 'dofree' is set, the old value may be freed.
 * if (opt_idx == -1) name is used, otherwise opt_idx is used.
 */
	void
set_string_option(name, opt_idx, val, dofree)
	char_u	*name;
	int		opt_idx;
	char_u	*val;
	int		dofree;
{
	char_u	*s;
	char_u	**varp;

	if (opt_idx == -1)			/* use name */
	{
		opt_idx = findoption(name);
		if (opt_idx == -1)		/* not found (should not happen) */
			return;
	}

	if (options[opt_idx].var == NULL)	/* don't set hidden option */
		return;

	s = strsave(val);
	if (s != NULL)
	{
		varp = (char_u **)get_varp(&(options[opt_idx]));
		if (dofree && (options[opt_idx].flags & P_ALLOCED))
			free_string_option(*varp);
		*varp = s;
			/* if 'term' option set for the first time: set default value */
		if (varp == &(term_strings[KS_NAME]) &&
										   *(options[opt_idx].def_val) == NUL)
		{
			options[opt_idx].def_val = s;
			options[opt_idx].flags |= P_DEF_ALLOCED;
		}
		else
			options[opt_idx].flags |= P_ALLOCED;
	}
}

/*
 * find index for option 'arg'
 * return -1 if not found
 */
	static int
findoption(arg)
	char_u *arg;
{
	int		opt_idx;
	char	*s;

	for (opt_idx = 0; (s = options[opt_idx].fullname) != NULL; opt_idx++)
	{
		if (STRCMP(arg, s) == 0) /* match full name */
			break;
	}
	if (s == NULL)
	{
		for (opt_idx = 0; options[opt_idx].fullname != NULL; opt_idx++)
		{
			s = options[opt_idx].shortname;
			if (s != NULL && STRCMP(arg, s) == 0) /* match short name */
				break;
			s = NULL;
		}
	}
	if (s == NULL)
		opt_idx = -1;
	return opt_idx;
}

	char_u *
get_highlight_default()
{
	int i;

	i = findoption((char_u *)"hl");
	if (i >= 0)
		return options[i].def_val;
	return (char_u *)NULL;
}

	static int
find_key_option(arg)
	char_u *arg;
{
	int			key;
	int			c;

	/* don't use get_special_key_code() for t_xx, we don't want it to call
	 * add_termcap_entry() */
	if (arg[0] == 't' && arg[1] == '_' && arg[2] && arg[3])
		key = TERMCAP2KEY(arg[2], arg[3]);

	/* <S-Tab> is a special case, because TAB isn't a special key */
	else if (vim_strnicmp(arg, (char_u *)"S-Tab", (size_t)5) == 0)
		key = K_S_TAB;
	else
	{
		/* Currently only the shift modifier is recognized */
		mod_mask = 0;
		if (TO_LOWER(arg[0]) == 's' && arg[1] == '-')
		{
			mod_mask = MOD_MASK_SHIFT;
			arg += 2;
		}
		c = get_special_key_code(arg);
		key = check_shifted_spec_key(c);
		if (mod_mask && c == key)			/* key can't be shifted */
			key = 0;
	}
	return key;
}

/*
 * if 'all' == 0: show changed options
 * if 'all' == 1: show all normal options
 * if 'all' == 2: show all terminal options
 */
	static void
showoptions(all)
	int			all;
{
	struct option   *p;
	int				col;
	int				isterm;
	char_u			*varp;
	struct option	**items;
	int				item_count;
	int				run;
	int				row, rows;
	int				cols;
	int				i;
	int				len;

#define INC	20
#define GAP 3

	items = (struct option **)alloc((unsigned)(sizeof(struct option *) *
																PARAM_COUNT));
	if (items == NULL)
		return;

	set_highlight('t');		/* Highlight title */
	start_highlight();
	if (all == 2)
		MSG_OUTSTR("\n--- Terminal codes ---");
	else
		MSG_OUTSTR("\n--- Options ---");
	stop_highlight();

	/*
	 * do the loop two times:
	 * 1. display the short items
	 * 2. display the long items (only strings and numbers)
	 */
	for (run = 1; run <= 2 && !got_int; ++run)
	{
		/*
		 * collect the items in items[]
		 */
		item_count = 0;
		for (p = &options[0]; p->fullname != NULL; p++)
		{
			isterm = istermoption(p);
			varp = get_varp(p);
			if (varp != NULL && (
				(all == 2 && isterm) ||
				(all == 1 && !isterm) ||
				(all == 0 && option_changed(p))))
			{
				if (p->flags & P_BOOL)
					len = 1;			/* a toggle option fits always */
				else
				{
					option_value2string(p);
					len = STRLEN(p->fullname) + strsize(NameBuff) + 1;
				}
				if ((len <= INC - GAP && run == 1) ||
												(len > INC - GAP && run == 2))
					items[item_count++] = p;
			}
		}

		/*
		 * display the items
		 */
		if (run == 1)
		{
			cols = (Columns + GAP - 3) / INC;
			if (cols == 0)
				cols = 1;
			rows = (item_count + cols - 1) / cols;
		}
		else	/* run == 2 */
			rows = item_count;
		for (row = 0; row < rows && !got_int; ++row)
		{
			msg_outchar('\n');						/* go to next line */
			if (got_int)							/* 'q' typed in more */
				break;
			col = 0;
			for (i = row; i < item_count; i += rows)
			{
				msg_pos(-1, col);					/* make columns */
				showoneopt(items[i]);
				col += INC;
			}
			flushbuf();
			mch_breakcheck();
		}
	}
	vim_free(items);
}

/*
 * Return TRUE if option is different from the default value
 */
	static int
option_changed(p)
	struct option	*p;
{
	char_u	*varp;

	varp = get_varp(p);
	if (varp == NULL)
		return FALSE;	/* hidden option is never changed */

	if (p->flags & P_NUM)
		return (*(long *)varp != (long)p->def_val);
	if (p->flags & P_BOOL)
						/* the cast to long is required for Manx C */
		return (*(int *)varp != (int)(long)p->def_val);
	/* P_STRING */
	return STRCMP(*(char_u **)varp, p->def_val);
}

/*
 * showoneopt: show the value of one option
 * must not be called with a hidden option!
 */
	static void
showoneopt(p)
	struct option *p;
{
	char_u			*varp;

	varp = get_varp(p);

	if ((p->flags & P_BOOL) && !*(int *)varp)
		MSG_OUTSTR("no");
	else
		MSG_OUTSTR("  ");
	MSG_OUTSTR(p->fullname);
	if (!(p->flags & P_BOOL))
	{
		msg_outchar('=');
		option_value2string(p);		/* put string of option value in NameBuff */
		msg_outtrans(NameBuff);
	}
}

/*
 * Write modified options as set command to a file.
 * Return FAIL on error, OK otherwise.
 */
	int
makeset(fd)
	FILE *fd;
{
	struct option	*p;
	char_u			*s;
	int				e;
	char_u			*varp;

	/*
	 * The options that don't have a default (terminal name, columns, lines)
	 * are never written. Terminal options are also not written.
	 */
	for (p = &options[0]; !istermoption(p); p++)
		if (!(p->flags & P_NO_MKRC) && !istermoption(p) &&
														  (option_changed(p)))
		{
			varp = get_varp(p);
			if (p->flags & P_BOOL)
				fprintf(fd, "set %s%s", *(int *)(varp) ? "" : "no",
																 p->fullname);
			else if (p->flags & P_NUM)
				fprintf(fd, "set %s=%ld", p->fullname, *(long *)(varp));
			else	/* P_STRING */
			{
				fprintf(fd, "set %s=", p->fullname);
				s = *(char_u **)(varp);
				/* some characters have to be escaped with CTRL-V or
				 * backslash */
				if (s != NULL && putescstr(fd, s, TRUE) == FAIL)
					return FAIL;
			}
#ifdef USE_CRNL
			putc('\r', fd);
#endif
				/*
				 * Only check error for this putc, should catch at least
				 * the "disk full" situation.
				 */
			e = putc('\n', fd);
			if (e < 0)
				return FAIL;
		}
	return OK;
}

/*
 * Clear all the terminal options.
 * If the option has been allocated, free the memory.
 * Terminal options are never hidden or indirect.
 */
	void
clear_termoptions()
{
	struct option   *p;

	/*
	 * Reset a few things before clearing the old options. This may cause
	 * outputting a few things that the terminal doesn't understand, but the
	 * screen will be cleared later, so this is OK.
	 */
#ifdef USE_MOUSE
	mch_setmouse(FALSE);			/* switch mouse off */
#endif
	mch_restore_title(3);			/* restore window titles */
#ifdef WIN32
	/*
	 * Check if this is allowed now.
	 */
	if (can_end_termcap_mode(FALSE) == TRUE)
#endif
		stoptermcap();					/* stop termcap mode */

	for (p = &options[0]; p->fullname != NULL; p++)
		if (istermoption(p))
		{
			if (p->flags & P_ALLOCED)
				free_string_option(*(char_u **)(p->var));
			if (p->flags & P_DEF_ALLOCED)
				free_string_option(p->def_val);
			*(char_u **)(p->var) = empty_option;
			p->def_val = empty_option;
			p->flags &= ~(P_ALLOCED|P_DEF_ALLOCED);
		}
	clear_termcodes();
}

/*
 * Set the terminal option defaults to the current value.
 * Used after setting the terminal name.
 */
	void
set_term_defaults()
{
	struct option   *p;

	for (p = &options[0]; p->fullname != NULL; p++)
		if (istermoption(p) && p->def_val != *(char_u **)(p->var))
		{
			if (p->flags & P_DEF_ALLOCED)
			{
				free_string_option(p->def_val);
				p->flags &= ~P_DEF_ALLOCED;
			}
			p->def_val = *(char_u **)(p->var);
			if (p->flags & P_ALLOCED)
			{
				p->flags |= P_DEF_ALLOCED;
				p->flags &= ~P_ALLOCED;		/* don't free the value now */
			}
		}
}

/*
 * return TRUE if 'p' starts with 't_'
 */
	static int
istermoption(p)
	struct option *p;
{
	return (p->fullname[0] == 't' && p->fullname[1] == '_');
}

/*
 * Compute columns for ruler and shown command. 'sc_col' is also used to
 * decide what the maximum length of a message on the status line can be.
 * If there is a status line for the last window, 'sc_col' is independent
 * of 'ru_col'.
 */

#define COL_RULER 17		/* columns needed by ruler */

	void
comp_col()
{
	int last_has_status = (p_ls == 2 || (p_ls == 1 && firstwin != lastwin));

	sc_col = 0;
	ru_col = 0;
	if (p_ru)
	{
		ru_col = COL_RULER + 1;
							/* no last status line, adjust sc_col */
		if (!last_has_status)
			sc_col = ru_col;
	}
	if (p_sc)
	{
		sc_col += SHOWCMD_COLS;
		if (!p_ru || last_has_status)		/* no need for separating space */
			++sc_col;
	}
	sc_col = Columns - sc_col;
	ru_col = Columns - ru_col;
	if (sc_col <= 0)			/* screen too narrow, will become a mess */
		sc_col = 1;
	if (ru_col <= 0)
		ru_col = 1;
}

	static char_u *
get_varp(p)
	struct option	*p;
{
	if (!(p->flags & P_IND) || p->var == NULL)
		return p->var;

	switch ((long)(p->var))
	{
		case PV_LIST:	return (char_u *)&(curwin->w_p_list);
		case PV_NU:		return (char_u *)&(curwin->w_p_nu);
#ifdef RIGHTLEFT
		case PV_RL:     return (char_u *)&(curwin->w_p_rl);
#endif
		case PV_SCROLL:	return (char_u *)&(curwin->w_p_scroll);
		case PV_WRAP:	return (char_u *)&(curwin->w_p_wrap);
		case PV_LBR:	return (char_u *)&(curwin->w_p_lbr);

		case PV_AI:		return (char_u *)&(curbuf->b_p_ai);
		case PV_BIN:	return (char_u *)&(curbuf->b_p_bin);
#ifdef CINDENT
		case PV_CIN:	return (char_u *)&(curbuf->b_p_cin);
		case PV_CINK:	return (char_u *)&(curbuf->b_p_cink);
		case PV_CINO:	return (char_u *)&(curbuf->b_p_cino);
#endif
#if defined(SMARTINDENT) || defined(CINDENT)
		case PV_CINW:	return (char_u *)&(curbuf->b_p_cinw);
#endif
		case PV_COM:	return (char_u *)&(curbuf->b_p_com);
		case PV_EOL:	return (char_u *)&(curbuf->b_p_eol);
		case PV_ET:		return (char_u *)&(curbuf->b_p_et);
		case PV_FO:		return (char_u *)&(curbuf->b_p_fo);
		case PV_INF:	return (char_u *)&(curbuf->b_p_inf);
		case PV_ISK:	return (char_u *)&(curbuf->b_p_isk);
		case PV_LISP:	return (char_u *)&(curbuf->b_p_lisp);
		case PV_ML:		return (char_u *)&(curbuf->b_p_ml);
		case PV_MOD:	return (char_u *)&(curbuf->b_changed);
		case PV_RO:		return (char_u *)&(curbuf->b_p_ro);
#ifdef SMARTINDENT
		case PV_SI:		return (char_u *)&(curbuf->b_p_si);
#endif
#ifndef SHORT_FNAME
		case PV_SN:		return (char_u *)&(curbuf->b_p_sn);
#endif
		case PV_SW:		return (char_u *)&(curbuf->b_p_sw);
		case PV_TS:		return (char_u *)&(curbuf->b_p_ts);
		case PV_TW:		return (char_u *)&(curbuf->b_p_tw);
		case PV_TX:		return (char_u *)&(curbuf->b_p_tx);
		case PV_WM:		return (char_u *)&(curbuf->b_p_wm);
		default:		EMSG("get_varp ERROR");
	}
	/* always return a valid pointer to avoid a crash! */
	return (char_u *)&(curbuf->b_p_wm);
}

/*
 * Copy options from one window to another.
 * Used when creating a new window.
 * The 'scroll' option is not copied, because it depends on the window height.
 */
	void
win_copy_options(wp_from, wp_to)
	WIN		*wp_from;
	WIN		*wp_to;
{
	wp_to->w_p_list = wp_from->w_p_list;
	wp_to->w_p_nu = wp_from->w_p_nu;
#ifdef RIGHTLEFT
	wp_to->w_p_rl = wp_from->w_p_rl;
#endif
	wp_to->w_p_wrap = wp_from->w_p_wrap;
	wp_to->w_p_lbr = wp_from->w_p_lbr;
}

/*
 * Copy options from one buffer to another.
 * Used when creating a new buffer and sometimes when entering a buffer.
 * When "entering" is TRUE we will enter the bp_to buffer.
 * When "always" is TRUE, always copy the options, but only set
 * b_p_initialized when appropriate.
 */
	void
buf_copy_options(bp_from, bp_to, entering, always)
	BUF		*bp_from;
	BUF		*bp_to;
	int		entering;
	int		always;
{
	int		should_copy = TRUE;

	/*
	 * Don't do anything of the "to" buffer is invalid.
	 */
	if (bp_to == NULL || !buf_valid(bp_to))
		return;

	/*
	 * Only copy if the "from" buffer is valid and "to" and "from" are
	 * different.
	 */
	if (bp_from != NULL && buf_valid(bp_from) && bp_from != bp_to)
	{
		/*
		 * Always copy when entering and 'cpo' contains 'S'.
		 * Don't copy when already initialized.
		 * Don't copy when 'cpo' contains 's' and not entering.
		 * 'S'  entering  initialized   's'  should_copy
		 * yes    yes          X         X      TRUE
		 * yes    no          yes        X      FALSE
		 * no      X          yes        X      FALSE
		 *  X     no          no        yes     FALSE
		 *  X     no          no        no      TRUE
		 * no     yes         no         X      TRUE
		 */
		if ((vim_strchr(p_cpo, CPO_BUFOPTGLOB) == NULL || !entering) &&
				(bp_to->b_p_initialized ||
				 (!entering && vim_strchr(p_cpo, CPO_BUFOPT) != NULL)))
			should_copy = FALSE;

		if (should_copy || always)
		{
			/*
			 * Always free the allocated strings.
			 * If not already initialized, set 'readonly' and copy 'textmode'.
			 */
			free_buf_options(bp_to);
			if (!bp_to->b_p_initialized)
			{
				bp_to->b_p_ro = FALSE;				/* don't copy readonly */
				bp_to->b_p_tx = bp_from->b_p_tx;
				bp_to->b_p_tx_nobin = bp_from->b_p_tx_nobin;
			}

			bp_to->b_p_ai = bp_from->b_p_ai;
			bp_to->b_p_ai_save = bp_from->b_p_ai_save;
			bp_to->b_p_sw = bp_from->b_p_sw;
			bp_to->b_p_tw = bp_from->b_p_tw;
			bp_to->b_p_tw_save = bp_from->b_p_tw_save;
			bp_to->b_p_tw_nobin = bp_from->b_p_tw_nobin;
			bp_to->b_p_wm = bp_from->b_p_wm;
			bp_to->b_p_wm_save = bp_from->b_p_wm_save;
			bp_to->b_p_wm_nobin = bp_from->b_p_wm_nobin;
			bp_to->b_p_bin = bp_from->b_p_bin;
			bp_to->b_p_et = bp_from->b_p_et;
			bp_to->b_p_et_nobin = bp_from->b_p_et_nobin;
			bp_to->b_p_ml = bp_from->b_p_ml;
			bp_to->b_p_ml_nobin = bp_from->b_p_ml_nobin;
			bp_to->b_p_inf = bp_from->b_p_inf;
#ifndef SHORT_FNAME
			bp_to->b_p_sn = bp_from->b_p_sn;
#endif
			bp_to->b_p_com = strsave(bp_from->b_p_com);
			bp_to->b_p_fo = strsave(bp_from->b_p_fo);
#ifdef SMARTINDENT
			bp_to->b_p_si = bp_from->b_p_si;
			bp_to->b_p_si_save = bp_from->b_p_si_save;
#endif
#ifdef CINDENT
			bp_to->b_p_cin = bp_from->b_p_cin;
			bp_to->b_p_cin_save = bp_from->b_p_cin_save;
			bp_to->b_p_cink = strsave(bp_from->b_p_cink);
			bp_to->b_p_cino = strsave(bp_from->b_p_cino);
#endif
#if defined(SMARTINDENT) || defined(CINDENT)
			bp_to->b_p_cinw = strsave(bp_from->b_p_cinw);
#endif
#ifdef LISPINDENT
			bp_to->b_p_lisp = bp_from->b_p_lisp;
			bp_to->b_p_lisp_save = bp_from->b_p_lisp_save;
#endif
			bp_to->b_p_ta_nobin = bp_from->b_p_ta_nobin;

			/*
			 * Don't copy the options set by do_help(), use the saved values
			 */
			if (!keep_help_flag && bp_from->b_help && help_save_isk != NULL)
			{
				bp_to->b_p_isk = strsave(help_save_isk);
				if (bp_to->b_p_isk != NULL)
					init_chartab();
				bp_to->b_p_ts = help_save_ts;
				bp_to->b_help = FALSE;
			}
			else
			{
				bp_to->b_p_isk = strsave(bp_from->b_p_isk);
				vim_memmove(bp_to->b_chartab, bp_from->b_chartab, (size_t)256);
				bp_to->b_p_ts = bp_from->b_p_ts;
				bp_to->b_help = bp_from->b_help;
			}
		}

		/*
		 * When the options should be copied (ignoring "always"), set the flag
		 * that indicates that the options have been initialized.
		 */
		if (should_copy)
			bp_to->b_p_initialized = TRUE;
	}

	check_buf_options(bp_to);		/* make sure we don't have NULLs */
}


static int expand_option_idx = -1;
static char_u expand_option_name[5] = {'t', '_', NUL, NUL, NUL};

	void
set_context_in_set_cmd(arg)
	char_u *arg;
{
	int 		nextchar;
	int 		flags = 0;		/* init for GCC */
	int			opt_idx = 0;	/* init for GCC */
	char_u		*p;
	char_u		*after_blank = NULL;
	int			is_term_option = FALSE;
	int			key;

	expand_context = EXPAND_SETTINGS;
	if (*arg == NUL)
	{
		expand_pattern = arg;
		return;
	}
	p = arg + STRLEN(arg) - 1;
	if (*p == ' ' && *(p - 1) != '\\')
	{
		expand_pattern = p + 1;
		return;
	}
	while (p != arg && (*p != ' ' || *(p - 1) == '\\'))
	{
		/* remember possible start of file name to expand */
		if ((*p == ' ' || (*p == ',' && *(p - 1) != '\\')) &&
														  after_blank == NULL)
			after_blank = p + 1;
		p--;
	}
	if (p != arg)
		p++;
	if (STRNCMP(p, "no", (size_t) 2) == 0)
	{
		expand_context = EXPAND_BOOL_SETTINGS;
		p += 2;
	}
	if (STRNCMP(p, "inv", (size_t) 3) == 0)
	{
		expand_context = EXPAND_BOOL_SETTINGS;
		p += 3;
	}
	expand_pattern = arg = p;
	if (*arg == '<')
	{
		while (*p != '>')
			if (*p++ == NUL)		/* expand terminal option name */
				return;
		key = get_special_key_code(arg + 1);
		if (key == 0)				/* unknown name */
		{
			expand_context = EXPAND_NOTHING;
			return;
		}
		nextchar = *++p;
		is_term_option = TRUE;
		expand_option_name[2] = KEY2TERMCAP0(key);
		expand_option_name[3] = KEY2TERMCAP1(key);
	}
	else
	{
		if (p[0] == 't' && p[1] == '_')
		{
			p += 2;
			if (*p != NUL)
				++p;
			if (*p == NUL)
				return;			/* expand option name */
			nextchar = *++p;
			is_term_option = TRUE;
			expand_option_name[2] = p[-2];
			expand_option_name[3] = p[-1];
		}
		else
		{
			while (isalnum(*p) || *p == '_' || *p == '*')	/* Allow * wildcard */
				p++;
			if (*p == NUL)
				return;
			nextchar = *p;
			*p = NUL;
			opt_idx = findoption(arg);
			*p = nextchar;
			if (opt_idx == -1 || options[opt_idx].var == NULL)
			{
				expand_context = EXPAND_NOTHING;
				return;
			}
			flags = options[opt_idx].flags;
			if (flags & P_BOOL)
			{
				expand_context = EXPAND_NOTHING;
				return;
			}
		}
	}
	if ((nextchar != '=' && nextchar != ':')
									|| expand_context == EXPAND_BOOL_SETTINGS)
	{
		expand_context = EXPAND_UNSUCCESSFUL;
		return;
	}
	if (expand_context != EXPAND_BOOL_SETTINGS && p[1] == NUL)
	{
		expand_context = EXPAND_OLD_SETTING;
		if (is_term_option)
			expand_option_idx = -1;
		else
			expand_option_idx = opt_idx;
		expand_pattern = p + 1;
		return;
	}
	expand_context = EXPAND_NOTHING;
	if (is_term_option || (flags & P_NUM))
		return;
	if (after_blank != NULL)
		expand_pattern = after_blank;
	else
		expand_pattern = p + 1;
	if (flags & P_EXPAND)
	{
		p = options[opt_idx].var;
		if (p == (char_u *)&p_bdir || p == (char_u *)&p_dir ||
													   p == (char_u *)&p_path)
			expand_context = EXPAND_DIRECTORIES;
		else
			expand_context = EXPAND_FILES;
	}
	return;
}

	int
ExpandSettings(prog, num_file, file)
	regexp		*prog;
	int			*num_file;
	char_u		***file;
{
	int num_normal = 0;		/* Number of matching non-term-code settings */
	int num_term = 0;		/* Number of matching terminal code settings */
	int opt_idx;
	int match;
	int count = 0;
	char_u *str;
	int	loop;
	int is_term_opt;
	char_u	name_buf[MAX_KEY_NAME_LEN];
	int	save_reg_ic;

	/* do this loop twice:
	 * loop == 0: count the number of matching options
	 * loop == 1: copy the matching options into allocated memory
	 */
	for (loop = 0; loop <= 1; ++loop)
	{
		if (expand_context != EXPAND_BOOL_SETTINGS)
		{
			if (vim_regexec(prog, (char_u *)"all", TRUE))
			{
				if (loop == 0)
					num_normal++;
				else
					(*file)[count++] = strsave((char_u *)"all");
			}
			if (vim_regexec(prog, (char_u *)"termcap", TRUE))
			{
				if (loop == 0)
					num_normal++;
				else
					(*file)[count++] = strsave((char_u *)"termcap");
			}
		}
		for (opt_idx = 0; (str = (char_u *)options[opt_idx].fullname) != NULL;
																	opt_idx++)
		{
			if (options[opt_idx].var == NULL)
				continue;
			if (expand_context == EXPAND_BOOL_SETTINGS
			  && !(options[opt_idx].flags & P_BOOL))
				continue;
			is_term_opt = istermoption(&options[opt_idx]);
			if (is_term_opt && num_normal > 0)
				continue;
			match = FALSE;
			if (vim_regexec(prog, str, TRUE) ||
										(options[opt_idx].shortname != NULL &&
						 vim_regexec(prog,
								 (char_u *)options[opt_idx].shortname, TRUE)))
				match = TRUE;
			else if (is_term_opt)
			{
				name_buf[0] = '<';
				name_buf[1] = 't';
				name_buf[2] = '_';
				name_buf[3] = str[2];
				name_buf[4] = str[3];
				name_buf[5] = '>';
				name_buf[6] = NUL;
				if (vim_regexec(prog, name_buf, TRUE))
				{
					match = TRUE;
					str = name_buf;
				}
			}
			if (match)
			{
				if (loop == 0)
				{
					if (is_term_opt)
						num_term++;
					else
						num_normal++;
				}
				else
					(*file)[count++] = strsave(str);
			}
		}
		/*
		 * Check terminal key codes, these are not in the option table
		 */
		if (expand_context != EXPAND_BOOL_SETTINGS  && num_normal == 0)
		{
			for (opt_idx = 0; (str = get_termcode(opt_idx)) != NULL; opt_idx++)
			{
				if (!isprint(str[0]) || !isprint(str[1]))
					continue;

				name_buf[0] = 't';
				name_buf[1] = '_';
				name_buf[2] = str[0];
				name_buf[3] = str[1];
				name_buf[4] = NUL;

				match = FALSE;
				if (vim_regexec(prog, name_buf, TRUE))
					match = TRUE;
				else
				{
					name_buf[0] = '<';
					name_buf[1] = 't';
					name_buf[2] = '_';
					name_buf[3] = str[0];
					name_buf[4] = str[1];
					name_buf[5] = '>';
					name_buf[6] = NUL;

					if (vim_regexec(prog, name_buf, TRUE))
						match = TRUE;
				}
				if (match)
				{
					if (loop == 0)
						num_term++;
					else
						(*file)[count++] = strsave(name_buf);
				}
			}
			/*
			 * Check special key names.
			 */
			for (opt_idx = 0; (str = get_key_name(opt_idx)) != NULL; opt_idx++)
			{
				name_buf[0] = '<';
				STRCPY(name_buf + 1, str);
				STRCAT(name_buf, ">");

				save_reg_ic = reg_ic;
				reg_ic = TRUE;					/* ignore case here */
				if (vim_regexec(prog, name_buf, TRUE))
				{
					if (loop == 0)
						num_term++;
					else
						(*file)[count++] = strsave(name_buf);
				}
				reg_ic = save_reg_ic;
			}
		}
		if (loop == 0)
		{
			if (num_normal > 0)
				*num_file = num_normal;
			else if (num_term > 0)
				*num_file = num_term;
			else
				return OK;
			*file = (char_u **) alloc((unsigned)(*num_file * sizeof(char_u *)));
			if (*file == NULL)
			{
				*file = (char_u **)"";
				return FAIL;
			}
		}
	}
	return OK;
}

	int
ExpandOldSetting(num_file, file)
	int		*num_file;
	char_u	***file;
{
	char_u	*var = NULL;		/* init for GCC */
	char_u	*buf;

	*num_file = 0;
	*file = (char_u **)alloc((unsigned)sizeof(char_u *));
	if (*file == NULL)
		return FAIL;

	/*
	 * For a terminal key code epand_option_idx is < 0.
	 */
	if (expand_option_idx < 0)
	{
		var = find_termcode(expand_option_name + 2);
		if (var == NULL)
			expand_option_idx = findoption(expand_option_name);
	}

	if (expand_option_idx >= 0)
	{
		/* put string of option value in NameBuff */
		option_value2string(&options[expand_option_idx]);
		var = NameBuff;
	}
	else if (var == NULL)
		var = (char_u *)"";

	/* A backslash is required before some characters */
	buf = strsave_escaped(var, escape_chars);

	if (buf == NULL)
	{
		vim_free(*file);
		*file = NULL;
		return FAIL;
	}

	*file[0] = buf;
	*num_file = 1;
	return OK;
}

/*
 * Get the value for the numeric or string option *op in a nice format into
 * NameBuff[].  Must not be called with a hidden option!
 */
	static void
option_value2string(op)
	struct option	*op;
{
	char_u	*varp;

	varp = get_varp(op);
	if (op->flags & P_NUM)
	{
		if ((long *)varp == &p_wc)
		{
			if (IS_SPECIAL(p_wc) || find_special_key_in_table((int)p_wc) >= 0)
				STRCPY(NameBuff, get_special_key_name((int)p_wc, 0));
			else
				STRCPY(NameBuff, transchar((int)p_wc));
		}
		else
			sprintf((char *)NameBuff, "%ld", *(long *)varp);
	}
	else	/* P_STRING */
	{
		varp = *(char_u **)(varp);
		if (varp == NULL)					/* just in case */
			NameBuff[0] = NUL;
		else if (op->flags & P_EXPAND)
			home_replace(NULL, varp, NameBuff, MAXPATHL);
		else
			STRNCPY(NameBuff, varp, MAXPATHL);
	}
}

/*
 * Convert the given pattern "pat" which has shell style wildcards in it, into
 * a regular expression, and return the result.  If there is a directory path
 * separator to be matched, then TRUE is put in allow_directories, otherwise
 * FALSE is put there -- webb.
 */
	char_u *
file_pat_to_reg_pat(pat, pat_end, allow_directories)
	char_u	*pat;
	char_u	*pat_end;				/* first char after pattern */
	int		*allow_directories;		/* Result passed back out in here */
{
	int		size;
	char_u	*endp;
	char_u	*reg_pat;
	char_u	*p;
	int		i;
	int		nested = 0;
	int		add_dollar = TRUE;

	if (allow_directories != NULL)
		*allow_directories = FALSE;

	size = 2;				/* '^' at start, '$' at end */
	for (p = pat; p < pat_end; p++)
	{
		switch (*p)
		{
			case '*':
			case '.':
			case ',':
			case '{':
			case '}':
			case '~':
#ifdef BACKSLASH_IN_FILENAME
			case '\\':
#endif
				size += 2;
				break;
			default:
				size++;
				break;
		}
	}
	reg_pat = alloc(size + 1);
	if (reg_pat == NULL)
		return NULL;
	i = 0;
	if (pat[0] == '*')
		while (pat[0] == '*' && pat < pat_end - 1)
			pat++;
	else
		reg_pat[i++] = '^';
	endp = pat_end - 1;
	if (*endp == '*')
	{
		while (endp - pat > 0 && *endp == '*')
			endp--;
		add_dollar = FALSE;
	}
	for (p = pat; *p && nested >= 0 && p <= endp; p++)
	{
		switch (*p)
		{
			case '*':
				reg_pat[i++] = '.';
				reg_pat[i++] = '*';
				break;
			case '.':
			case '~':
				reg_pat[i++] = '\\';
				reg_pat[i++] = *p;
				break;
			case '?':
				reg_pat[i++] = '.';
				break;
			case '\\':
				if (p[1] == NUL)
					break;
#ifdef BACKSLASH_IN_FILENAME
				/* translate "\x" to "\\x", "\*" to "\\.*", and "\?" to "\\." */
				if (isfilechar(p[1]) || p[1] == '*' || p[1] == '?')
				{
					reg_pat[i++] = '\\';
					reg_pat[i++] = '\\';
					if (allow_directories != NULL)
						*allow_directories = TRUE;
					break;
				}
				++p;
#else
				if (*++p == '?')
					reg_pat[i++] = '?';
				else
#endif
					 if (*p == ',')
					reg_pat[i++] = ',';
				else
				{
					if (allow_directories != NULL && ispathsep(*p))
						*allow_directories = TRUE;
					reg_pat[i++] = '\\';
					reg_pat[i++] = *p;
				}
				break;
			case '{':
				reg_pat[i++] = '\\';
				reg_pat[i++] = '(';
				nested++;
				break;
			case '}':
				reg_pat[i++] = '\\';
				reg_pat[i++] = ')';
				--nested;
				break;
			case ',':
				if (nested)
				{
					reg_pat[i++] = '\\';
					reg_pat[i++] = '|';
				}
				else
					reg_pat[i++] = ',';
				break;
			default:
				if (allow_directories != NULL && ispathsep(*p))
					*allow_directories = TRUE;
				reg_pat[i++] = *p;
				break;
		}
	}
	if (add_dollar)
		reg_pat[i++] = '$';
	reg_pat[i] = NUL;
	if (nested != 0)
	{
		if (nested < 0)
			EMSG("Missing {.");
		else
			EMSG("Missing }.");
		vim_free(reg_pat);
		reg_pat = NULL;
	}
	return reg_pat;
}

#ifdef AUTOCMD
/*
 * functions for automatic commands
 */

static void show_autocmd __ARGS((AutoPat *ap, int event));
static void del_autocmd __ARGS((AutoPat *ap));
static void del_autocmd_cmds __ARGS((AutoPat *ap));
static int event_name2nr __ARGS((char_u *start, char_u **end));
static char *event_nr2name __ARGS((int event));
static char_u *find_end_event __ARGS((char_u *arg));
static int do_autocmd_event __ARGS((int event, char_u *pat,
												   char_u *cmd, int forceit));

	static void
show_autocmd(ap, event)
	AutoPat	*ap;
	int		event;
{
	AutoCmd *ac;

	if (got_int)				/* "q" hit for "--more--" */
		return;
	msg_outchar('\n');
	if (got_int)				/* "q" hit for "--more--" */
		return;
	msg_outchar('\n');
	if (got_int)				/* "q" hit for "--more--" */
		return;
	MSG_OUTSTR(event_nr2name(event));
	MSG_OUTSTR("  ");
	msg_outstr(ap->pat);
	for (ac = ap->cmds; ac != NULL; ac = ac->next)
	{
		MSG_OUTSTR("\n    ");
		if (got_int)			/* hit "q" at "--more--" prompt */
			return;
		msg_outtrans(ac->cmd);
	}
}

/*
 * Delete an autocommand pattern.
 */
	static void
del_autocmd(ap)
	AutoPat	*ap;
{
	vim_free(ap->pat);
	vim_free(ap->reg_pat);
	del_autocmd_cmds(ap);
	vim_free(ap);
}

/*
 * Delete the commands from a pattern.
 */
	static void
del_autocmd_cmds(ap)
	AutoPat *ap;
{
	AutoCmd	*ac;

	while (ap->cmds != NULL)
	{
		ac = ap->cmds;
		ap->cmds = ac->next;
		vim_free(ac->cmd);
		vim_free(ac);
	}
}

/*
 * Return the event number for event name "start".
 * Return -1 if the event name was not found.
 * Return a pointer to the next event name in "end".
 */
	static int
event_name2nr(start, end)
	char_u	*start;
	char_u	**end;
{
	char_u		*p;
	int			i;
	int			len;

	/* the event name ends with end of line, a blank or a comma */
	for (p = start; *p && !vim_iswhite(*p) && *p != ','; ++p)
		;
	for (i = 0; event_names[i].name != NULL; ++i)
	{
		len = strlen(event_names[i].name);
		if (len == p - start &&
				   vim_strnicmp((char_u *)event_names[i].name, (char_u *)start, (size_t)len) == 0)
			break;
	}
	if (*p == ',')
		++p;
	*end = p;
	if (event_names[i].name == NULL)
		return -1;
	return event_names[i].event;
}

/*
 * Return the name for event "event".
 */
	static char *
event_nr2name(event)
	int		event;
{
	int		i;

	for (i = 0; event_names[i].name != NULL; ++i)
		if (event_names[i].event == event)
			return event_names[i].name;
	return "Unknown";
}

/*
 * Scan over the events.  "*" stands for all events.
 */
	static char_u *
find_end_event(arg)
	char_u	*arg;
{
	char_u 	*pat;
	char_u	*p;

	if (*arg == '*')
	{
		if (arg[1] && !vim_iswhite(arg[1]))
		{
			EMSG2("Illegal character after *: %s", arg);
			return NULL;
		}
		pat = arg + 1;
	}
	else
	{
		for (pat = arg; *pat && !vim_iswhite(*pat); pat = p)
		{
			if (event_name2nr(pat, &p) < 0)
			{
				EMSG2("No such event: %s", pat);
				return NULL;
			}
		}
	}
	return pat;
}

/*
 * do_autocmd() -- implements the :autocmd command.  Can be used in the
 *	following ways:
 *
 * :autocmd <event> <pat> <cmd>		Add <cmd> to the list of commands that
 *									will be automatically executed for <event>
 *									when editing a file matching <pat>.
 * :autocmd <event> <pat>			Show the auto-commands associated with
 *									<event> and <pat>.
 * :autocmd	<event>					Show the auto-commands associated with
 *									<event>.
 * :autocmd							Show all auto-commands.
 * :autocmd! <event> <pat> <cmd>	Remove all auto-commands associated with
 *									<event> and <pat>, and add the command
 *									<cmd>.
 * :autocmd! <event> <pat>			Remove all auto-commands associated with
 *									<event> and <pat>.
 * :autocmd! <event>				Remove all auto-commands associated with
 *									<event>.
 * :autocmd!						Remove ALL auto-commands.
 *
 *	Multiple events and patterns may be given separated by commas.  Here are
 *	some examples:
 * :autocmd bufread,bufenter *.c,*.h	set tw=0 smartindent noic
 * :autocmd bufleave 		 *			set tw=79 nosmartindent ic infercase
 *
 * :autocmd * *.c				show all autocommands for *.c files.
 */
	void
do_autocmd(arg, forceit)
	char_u	*arg;
	int		forceit;
{
	char_u	*pat;
	char_u	*cmd;
	int		event;

	/*
	 * Don't change autocommands while executing one.
	 */
	if (autocmd_busy)
		return;

	/*
	 * Scan over the events.
	 * If we find an illegal name, return here, don't do anything.
	 */
	pat = find_end_event(arg);
	if (pat == NULL)
		return;

	/*
	 * Scan over the pattern.  Put a NUL at the end.
	 */
	pat = skipwhite(pat);
	cmd = pat;
	while (*cmd && (!vim_iswhite(*cmd) || cmd[-1] == '\\'))
		cmd++;
	if (*cmd)
		*cmd++ = NUL;

	/*
	 * Find the start of the commands.
	 */
	cmd = skipwhite(cmd);

	/*
	 * Print header when showing autocommands.
	 */
	if (!forceit && *cmd == NUL)
	{
		set_highlight('t');		/* Highlight title */
		start_highlight();
		MSG_OUTSTR("\n--- Auto-Commands ---");
		stop_highlight();
	}

	/*
	 * Loop over the events.
	 */
	if (*arg == '*' || *arg == NUL)
	{
		for (event = 0; event < NUM_EVENTS; ++event)
			if (do_autocmd_event(event, pat, cmd, forceit) == FAIL)
				break;
	}
	else
	{
		while (*arg && !vim_iswhite(*arg))
			if (do_autocmd_event(event_name2nr(arg, &arg), pat,
														cmd, forceit) == FAIL)
				break;
	}
}

/*
 * do_autocmd() for one event.
 * If *pat == NUL do for all patterns.
 * If *cmd == NUL show entries.
 * If forceit == TRUE delete entries.
 */
	static int
do_autocmd_event(event, pat, cmd, forceit)
	int		event;
	char_u	*pat;
	char_u	*cmd;
	int		forceit;
{
	AutoPat		*ap;
	AutoPat		*ap2;
	AutoPat		**final_ap;
	AutoCmd		*ac;
	AutoCmd		**final_ac;
	int			nested;
	char_u		*endpat;
	int			len;

	/*
	 * Show or delete all patterns for an event.
	 */
	if (*pat == NUL)
	{
		for (ap = first_autopat[event]; ap != NULL; ap = ap2)
		{
			ap2 = ap->next;
			if (forceit)
				del_autocmd(ap);
			else
				show_autocmd(ap, event);
		}
		if (forceit)
			first_autopat[event] = NULL;
	}

	/*
	 * Loop through all the specified patterns.
	 */
	for ( ; *pat; pat = (*endpat == ',' ? endpat + 1 : endpat))
	{
		/*
		 * Find end of the pattern.
		 * Watch out for a comma in braces, like "*.\{obj,o\}".
		 */
		nested = 0;
		for (endpat = pat;
				*endpat && (*endpat != ',' || nested || endpat[-1] == '\\');
				++endpat)
		{
			if (*endpat == '{')
				nested++;
			else if (*endpat == '}')
				nested--;
		}
		if (pat == endpat)				/* ignore single comma */
			continue;

		/*
		 * Find entry with same pattern.
		 */
		final_ap = &first_autopat[event];
		for (ap = first_autopat[event]; ap != NULL; ap = *final_ap)
		{
			len = STRLEN(ap->pat);
			if (len == endpat - pat && STRNCMP(pat, ap->pat, len) == 0)
				break;
			final_ap = &ap->next;
		}

		/*
		 * Add a new pattern.
		 * Show and delete are ignored if pattern is not found.
		 */
		if (ap == NULL)
		{
			if (*cmd == NUL)
				continue;

			/* Add the autocmd at the end of the list */
			ap = (AutoPat *)alloc((unsigned)sizeof(AutoPat));
			if (ap == NULL)
				return FAIL;
			ap->pat = strnsave(pat, (int)(endpat - pat));
			if (ap->pat == NULL)
			{
				vim_free(ap);
				return FAIL;
			}
			ap->reg_pat = file_pat_to_reg_pat(pat, endpat,
													  &ap->allow_directories);
			if (ap->reg_pat == NULL)
			{
				vim_free(ap->pat);
				vim_free(ap);
				return FAIL;
			}
			ap->cmds = NULL;
			*final_ap = ap;
			ap->next = NULL;
		}

		/*
		 * Remove existing autocommands.
		 * If not adding any new autocmd's for this pattern, delete the
		 * pattern from the autopat list
		 */
		else if (forceit)
		{
			del_autocmd_cmds(ap);
			if (*cmd == NUL)
			{
				if (ap == first_autopat[event])
					first_autopat[event] = ap->next;
				else
				{
					for (ap2 = first_autopat[event];
							ap2->next != ap;
							ap2 = ap2->next)
						;
					ap2->next = ap->next;
				}
				del_autocmd(ap);
			}
		}

		/*
		 * Show autocmd's for this autopat
		 */
		if (*cmd == NUL && !forceit)
		{
			show_autocmd(ap, event);
		}

		/*
		 * Add the autocmd at the end if it's not already there.
		 */
		else if (*cmd != NUL)
		{
			final_ac = &(ap->cmds);
			for (ac = ap->cmds;
					ac != NULL && STRCMP(cmd, ac->cmd) != 0;
					ac = ac->next)
				final_ac = &ac->next;
			if (ac == NULL)
			{
				ac = (AutoCmd *)alloc((unsigned)sizeof(AutoCmd));
				if (ac == NULL)
					return FAIL;
				ac->cmd = strsave(cmd);
				if (ac->cmd == NULL)
				{
					vim_free(ac);
					return FAIL;
				}
				ac->next = NULL;
				*final_ac = ac;
			}
		}
	}
	return OK;
}

/*
 * Implementation of ":doautocmd event [fname]".
 */
	void
do_doautocmd(arg)
	char_u		*arg;
{
	char_u		*fname;
	int			nothing_done = TRUE;

	if (*arg == '*')
	{
		EMSG("Can't execute autocommands for ALL events");
		return;
	}

	/*
	 * Scan over the events.
	 * If we find an illegal name, return here, don't do anything.
	 */
	fname = find_end_event(arg);
	if (fname == NULL)
		return;

	fname = skipwhite(fname);

	/*
	 * Loop over the events.
	 */
	while (*arg && !vim_iswhite(*arg))
		if (apply_autocmds(event_name2nr(arg, &arg), fname, NULL))
			nothing_done = FALSE;

	if (nothing_done)
		MSG("No matching autocommands");
}

/*
 * Execute autocommands for "event" and file name "fname".
 * Return TRUE if some commands were executed.
 */
	int
apply_autocmds(event, fname, fname_io)
	int				event;
	char_u			*fname;		/* NULL or empty means use actual file name */
	char_u			*fname_io;	/* fname to use for "^Vf" on cmdline */
{
	struct regexp	*prog;
	char_u			*tail;
	AutoPat			*ap;
	AutoCmd			*ac;
	int				temp;
	int				save_changed = curbuf->b_changed;
	BUF				*old_curbuf = curbuf;
	char_u			*save_name;
	char_u			*full_fname = NULL;
	int				retval = FALSE;

	if (autocmd_busy)			/* no nesting allowed */
		return retval;
	/*
	 * Check if these autocommands are disabled.  Used when doing ":all" or
	 * ":ball".
	 */
	if (	(autocmd_no_enter &&
				(event == EVENT_WINENTER || event == EVENT_BUFENTER)) ||
			(autocmd_no_leave &&
				(event == EVENT_WINLEAVE || event == EVENT_BUFLEAVE)))
		return retval;

		/* Don't redraw while doing auto commands. */
	temp = RedrawingDisabled;
	RedrawingDisabled = TRUE;
	save_name = sourcing_name;	/* may be called from .vimrc */
	autocmd_fname = fname_io;

	/*
	 * While applying autocmds, we don't want to allow the commands
	 * :doautocmd or :autocmd.
	 */
	autocmd_busy = TRUE;

	/*
	 * When the file name is NULL or empty, use the file name of the current
	 * buffer.  Always use the full path of the file name to match with, in
	 * case "allow_directories" is set.
	 */
	if (fname == NULL || *fname == NUL)
	{
		fname = curbuf->b_filename;
		if (fname == NULL)
			fname = (char_u *)"";
	}
	else
	{
		full_fname = FullName_save(fname);
		fname = full_fname;
	}

	tail = gettail(fname);

	for (ap = first_autopat[event]; ap != NULL && !got_int; ap = ap->next)
	{
#ifdef CASE_INSENSITIVE_FILENAME
		reg_ic = TRUE;		/* Always ignore case */
#else
		reg_ic = FALSE;		/* Don't ever ignore case */
#endif
		reg_magic = TRUE;	/* Always use magic */
		prog = vim_regcomp(ap->reg_pat);

		if (prog != NULL &&
			((ap->allow_directories && vim_regexec(prog, fname, TRUE)) ||
			(!ap->allow_directories && vim_regexec(prog, tail, TRUE))))
		{
			sprintf((char *)IObuff, "%s Auto commands for \"%s\"",
									   event_nr2name(event), (char *)ap->pat);
			sourcing_name = strsave(IObuff);
			for (ac = ap->cmds; ac != NULL; ac = ac->next)
			{
				do_cmdline(ac->cmd, TRUE, TRUE);
				retval = TRUE;
			}
			vim_free(sourcing_name);
		}
		vim_free(prog);
		mch_breakcheck();
	}
	RedrawingDisabled = temp;
	autocmd_busy = FALSE;
	sourcing_name = save_name;
	autocmd_fname = NULL;
	vim_free(full_fname);

	/*
	 * Some events don't set or reset the Changed flag.
	 * Check if still in the same buffer!
	 */
	if (curbuf == old_curbuf &&
			(event == EVENT_BUFREADPOST || event == EVENT_BUFWRITEPOST ||
			event == EVENT_FILEAPPENDPOST || event == EVENT_VIMLEAVE))
		curbuf->b_changed = save_changed;

	return retval;
}

	char_u	*
set_context_in_autocmd(arg, doautocmd)
	char_u	*arg;
	int		doautocmd;		/* TRUE for :doautocmd, FALSE for :autocmd */
{
	char_u	*p;

	/* skip over event name */
	for (p = arg; *p && !vim_iswhite(*p); ++p)
		if (*p == ',')
			arg = p + 1;
	if (*p == NUL)
	{
		expand_context = EXPAND_EVENTS;		/* expand event name */
		expand_pattern = arg;
		return NULL;
	}

	/* skip over pattern */
	arg = skipwhite(p);
	while (*arg && (!vim_iswhite(*arg) || arg[-1] == '\\'))
		arg++;
	if (*arg)
		return arg;							/* expand (next) command */

	if (doautocmd)
		expand_context = EXPAND_FILES;		/* expand file names */
	else
		expand_context = EXPAND_NOTHING;	/* pattern is not expanded */
	return NULL;
}

	int
ExpandEvents(prog, num_file, file)
	regexp		*prog;
	int			*num_file;
	char_u		***file;
{
	int		i;
	int		count;
	int		round;

	/*
	 * round == 1: Count the matches.
	 * round == 2: Save the matches into the array.
	 */
	for (round = 1; round <= 2; ++round)
	{
		count = 0;
		for (i = 0; event_names[i].name != NULL; i++)
			if (vim_regexec(prog, (char_u *)event_names[i].name, TRUE))
			{
				if (round == 1)
					count++;
				else
					(*file)[count++] = strsave((char_u *)event_names[i].name);
			}
		if (round == 1)
		{
			*num_file = count;
			if (count == 0 || (*file = (char_u **)
						 alloc((unsigned)(count * sizeof(char_u *)))) == NULL)
				return FAIL;
		}
	}
	return OK;
}

#endif  /* AUTOCMD */

#ifdef HAVE_LANGMAP
/*
 * Any character has an equivalent character.  This is used for keyboards that
 * have a special language mode that sends characters above 128 (although
 * other characters can be translated too).
 */

/* 
 * char_u langmap_mapchar[256];
 * Normally maps each of the 128 upper chars to an <128 ascii char; used to
 * "translate" native lang chars in normal mode or some cases of
 * insert mode without having to tediously switch lang mode back&forth.
 */

	static void 
langmap_init()
{
	int i;

	for (i = 0; i < 256; i++)			/* we init with a-one-to one map */
		langmap_mapchar[i] = i;
}

/*
 * Called when langmap option is set; the language map can be
 * changed at any time!
 */
	static void
langmap_set()
{
	char_u	*p;
	char_u	*p2;
	int		from, to;

	langmap_init();							/* back to one-to-one map first */

	for (p = p_langmap; p[0]; )
	{
		for (p2 = p; p2[0] && p2[0] != ',' && p2[0] != ';'; ++p2)
			if (p2[0] == '\\' && p2[1])
				++p2;
		if (p2[0] == ';')
			++p2;			/* abcd;ABCD form, p2 points to A */
		else
			p2 = NULL;		/* aAbBcCdD form, p2 is NULL */
		while (p[0])
		{
			if (p[0] == '\\' && p[1])
				++p;
			from = p[0];
			if (p2 == NULL)
			{
				if (p[1] == '\\')
					++p;
				to = p[1];
			}
			else
			{
				if (p2[0] == '\\')
					++p2;
				to = p2[0];
			}
			if (to == NUL)
			{
				EMSG2("'langmap': Matching character missing for %s",
															 transchar(from));
				return;
			}
			langmap_mapchar[from] = to;

			/* Advance to next pair */
			if (p2 == NULL)
			{
				p += 2;
				if (p[0] == ',')
				{
					++p;
					break;
				}
			}
			else
			{
				++p;
				++p2;
				if (*p == ';')
				{
					p = p2;
					if (p[0])
					{
						if (p[0] != ',')
						{
							EMSG2("'langmap': Extra characters after semicolon: %s", p);
							return;
						}
						++p;
					}
					break;
				}
			}
		}
	}
}
#endif

/*
 * Return TRUE if format option 'x' is in effect.
 * Take care of no formatting when 'paste' is set.
 */
	int
has_format_option(x)
	int		x;
{
	if (p_paste)
		return FALSE;
	return (vim_strchr(curbuf->b_p_fo, x) != NULL);
}

/*
 * Return TRUE if "x" is present in 'shortmess' option, or
 * 'shortmess' contains 'a' and "x" is present in SHM_A.
 */
	int
shortmess(x)
	int		x;
{
	return (vim_strchr(p_shm, x) != NULL || (vim_strchr(p_shm, 'a') != NULL &&
									   vim_strchr((char_u *)SHM_A, x) != NULL));
}

/*
 * set_paste_option() - Called after p_paste was set or reset.
 */
	static void
paste_option_changed()
{
	static int		old_p_paste = FALSE;
	static int		save_sm = 0;
	static int		save_ru = 0;
#ifdef RIGHTLEFT
	static int		save_ri = 0;
	static int		save_hkmap = 0;
#endif
	BUF				*buf;

	if (p_paste)
	{
		/*
		 * Paste switched from off to on.
		 * Save the current values, so they can be restored later.
		 */
		if (!old_p_paste)
		{
			/* save options for each buffer */
			for (buf = firstbuf; buf != NULL; buf = buf->b_next)
			{
				buf->b_p_tw_save = buf->b_p_tw;
				buf->b_p_wm_save = buf->b_p_wm;
				buf->b_p_ai_save = buf->b_p_ai;
#ifdef SMARTINDENT
				buf->b_p_si_save = buf->b_p_si;
#endif
#ifdef CINDENT
				buf->b_p_cin_save = buf->b_p_cin;
#endif
#ifdef LISPINDENT
				buf->b_p_lisp_save = buf->b_p_lisp;
#endif
			}

			/* save global options */
			save_sm = p_sm;
			save_ru = p_ru;
#ifdef RIGHTLEFT
			save_ri = p_ri;
			save_hkmap = p_hkmap;
#endif
		}

		/*
		 * Always set the option values, also when 'paste' is set when it is
		 * already on.
		 */
		/* set options for each buffer */
		for (buf = firstbuf; buf != NULL; buf = buf->b_next)
		{
			buf->b_p_tw = 0;		/* textwidth is 0 */
			buf->b_p_wm = 0;		/* wrapmargin is 0 */
			buf->b_p_ai = 0;		/* no auto-indent */
#ifdef SMARTINDENT
			buf->b_p_si = 0;		/* no smart-indent */
#endif
#ifdef CINDENT
			buf->b_p_cin = 0;		/* no c indenting */
#endif
#ifdef LISPINDENT
			buf->b_p_lisp = 0;		/* no lisp indenting */
#endif
		}

		/* set global options */
		p_sm = 0;					/* no showmatch */
		p_ru = 0;					/* no ruler */
#ifdef RIGHTLEFT
		p_ri = 0;					/* no reverse insert */
		p_hkmap = 0;				/* no Hebrew keyboard */
#endif
	}

	/*
	 * Paste switched from on to off: Restore saved values.
	 */
	else if (old_p_paste)
	{
		/* restore options for each buffer */
		for (buf = firstbuf; buf != NULL; buf = buf->b_next)
		{
			buf->b_p_tw = buf->b_p_tw_save;
			buf->b_p_wm = buf->b_p_wm_save;
			buf->b_p_ai = buf->b_p_ai_save;
#ifdef SMARTINDENT
			buf->b_p_si = buf->b_p_si_save;
#endif
#ifdef CINDENT
			buf->b_p_cin = buf->b_p_cin_save;
#endif
#ifdef LISPINDENT
			buf->b_p_lisp = buf->b_p_lisp_save;
#endif
		}

		/* restore global options */
		p_sm = save_sm;
		p_ru = save_ru;
#ifdef RIGHTLEFT
		p_ri = save_ri;
		p_hkmap = save_hkmap;
#endif
	}

	old_p_paste = p_paste;
}

/*
 * p_compatible_set() - Called when p_cp has been set.
 */
	static void
p_compatible_set()
{
	p_bs = 0;						/* normal backspace */
									/* backspace and space do not wrap */
	set_string_option((char_u *)"ww", -1, (char_u *)"", TRUE);
	p_bk = 0;						/* no backup file */
					/* Use textwidth for formatting, don't format comments */
	set_string_option((char_u *)"fo", -1, (char_u *)FO_DFLT_VI, TRUE);
									/* all compatible flags on */
	set_string_option((char_u *)"cpo", -1, (char_u *)CPO_ALL, TRUE);
	set_string_option((char_u *)"isk", -1, (char_u *)"@,48-57,_", TRUE);
									/* no short messages */
	set_string_option((char_u *)"shm", -1, (char_u *)"", TRUE);
#ifdef DIGRAPHS
	p_dg = 0;						/* no digraphs */
#endif /* DIGRAPHS */
	p_ek = 0;						/* no ESC keys in insert mode */
	curbuf->b_p_et = 0;				/* no expansion of tabs */
	p_gd = 0;						/* /g is not default for :s */
	p_hi = 0; 						/* no history */
	p_scs = 0;						/* no ignore case switch */
	p_im = 0;						/* do not start in insert mode */
	p_js = 1;						/* insert 2 spaces after period */
	curbuf->b_p_ml = 0;				/* no modelines */
	p_more = 0;						/* no -- more -- for listings */
	p_ru = 0;						/* no ruler */
#ifdef RIGHTLEFT
	p_ri = 0;						/* no reverse insert */
	p_hkmap = 0;					/* no Hebrew keyboard mapping */
#endif
	p_sj = 1;						/* no scrolljump */
	p_so = 0;						/* no scrolloff */
	p_sr = 0;						/* do not round indent to shiftwidth */
	p_sc = 0;						/* no showcommand */
	p_smd = 0;						/* no showmode */
#ifdef SMARTINDENT
	curbuf->b_p_si = 0;				/* no smartindent */
#endif
#ifdef CINDENT
	curbuf->b_p_cin = 0;			/* no C indenting */
#endif
	p_sta = 0;						/* no smarttab */
	p_sol = TRUE;					/* Move cursor to start-of-line */
	p_ta = 0;						/* no automatic textmode detection */
	curbuf->b_p_tw = 0;				/* no automatic line wrap */
	p_to = 0;						/* no tilde operator */
	p_ttimeout = 0;					/* no terminal timeout */
	p_tr = 0;						/* tag file names not relative */
	p_ul = 0;						/* no multilevel undo */
	p_uc = 0;						/* no autoscript file */
	p_wb = 0;						/* no backup file */
	if (p_wc == TAB)
		p_wc = Ctrl('E');			/* normal use for TAB */
	init_chartab();					/* make b_p_isk take effect */
}

/*
 * fill_breakat_flags() -- called when 'breakat' changes value.
 */
	static void
fill_breakat_flags()
{
	char_u		*c;
	int			i;

	for (i = 0; i < 256; i++) 
		breakat_flags[i] = FALSE;

	if (p_breakat != NULL)
		for (c = p_breakat; *c; c++) 
			breakat_flags[*c] = TRUE;
}
