/*	$OpenBSD: keymap.c,v 1.5 2001/01/29 01:58:08 niklas Exp $	*/

/*
 * Keyboard maps.  This is character set dependent.  The terminal specific 
 * parts of building the keymap has been moved to a better place.
 */

#include	"def.h"
#include	"kbd.h"

static int	 name_fent	__P((char *, int));

/*
 * initial keymap declarations, deepest first 
 */

#ifndef NO_HELP
static PF cHcG[] = {
	ctrlg,			/* ^G */
	help_help,		/* ^H */
};

static PF cHa[] = {
	apropos_command,	/* a */
	wallchart,		/* b */
	desckey,		/* c */
};

static struct KEYMAPE (2 + IMAPEXT) helpmap = {
	2,
	2 + IMAPEXT,
	rescan,
	{
		{
			CCHR('G'), CCHR('H'), cHcG, (KEYMAP *)NULL
		},
		{
			'a', 'c', cHa, (KEYMAP *)NULL
		},
	}
};
#endif /* !NO_HELP */

static struct KEYMAPE (1 + IMAPEXT) extramap1 = {
	0,
	1 + IMAPEXT,
	rescan
};

static struct KEYMAPE (1 + IMAPEXT) extramap2 = {
	0,
	1 + IMAPEXT,
	rescan
};

static struct KEYMAPE (1 + IMAPEXT) extramap3 = {
	0,
	1 + IMAPEXT,
	rescan
};

static struct KEYMAPE (1 + IMAPEXT) extramap4 = {
	0,
	1 + IMAPEXT,
	rescan
};

static struct KEYMAPE (1 + IMAPEXT) extramap5 = {
	0,
	1 + IMAPEXT,
	rescan
};

static PF cX4cF[] = {
	poptofile,		/* ^f */
	ctrlg,			/* ^g */
};
static PF cX4b[] = {
	poptobuffer,		/* b */
	rescan,			/* c */
	rescan,			/* d */
	rescan,			/* e */
	poptofile,		/* f */
};
static struct KEYMAPE (2 + IMAPEXT) cX4map = {
	2,
	2 + IMAPEXT,
	rescan,
	{
		{
			CCHR('F'), CCHR('G'), cX4cF, (KEYMAP *)NULL
		},
		{
			'b', 'f', cX4b, (KEYMAP *)NULL
		},
	}
};

static PF cXcB[] = {
	listbuffers,		/* ^B */
	quit,			/* ^C */
	rescan,			/* ^D */
	rescan,			/* ^E */
	filevisit,		/* ^F */
	ctrlg,			/* ^G */
};

static PF cXcL[] = {
	lowerregion,		/* ^L */
	rescan,			/* ^M */
	rescan,			/* ^N */
	deblank,		/* ^O */
	rescan,			/* ^P */
	rescan,			/* ^Q */
	rescan,			/* ^R */
	filesave,		/* ^S */
	rescan,			/* ^T */
	upperregion,		/* ^U */
	rescan,			/* ^V */
	filewrite,		/* ^W */
	swapmark,		/* ^X */
};

#ifndef NO_MACRO
static PF cXlp[] = {
	definemacro,		/* ( */
	finishmacro,		/* ) */
};
#endif /* !NO_MACRO */

static PF cX0[] = {
	delwind,		/* 0 */
	onlywind,		/* 1 */
	splitwind,		/* 2 */
	rescan,			/* 3 */
	prefix,			/* 4 */
};

static PF cXeq[] = {
	showcpos,		/* = */
};

static PF cXcar[] = {
	enlargewind,		/* ^ */
	rescan,			/* _ */
	rescan,			/* ` */
	rescan,			/* a */
	usebuffer,		/* b */
	rescan,			/* c */
#ifndef NO_DIRED
	dired,			/* d */
#else /* !NO_DIRED */
	rescan,			/* d */
#endif /* !NO_DIRED */
#ifndef NO_MACRO
	executemacro,		/* e */
#else /* !NO_MACRO */
	rescan,			/* e */
#endif /* !NO_MACRO */
	setfillcol,		/* f */
	rescan,			/* g */
	rescan,			/* h */
	fileinsert,		/* i */
	rescan,			/* j */
	killbuffer,		/* k */
	rescan,			/* l */
	rescan,			/* m */
	rescan,			/* n */
	nextwind,		/* o */
	rescan,			/* p */
	rescan,			/* q */
	rescan,			/* r */
	savebuffers,		/* s */
};

#ifndef NO_MACRO
static struct KEYMAPE (6 + IMAPEXT) cXmap = {
	6,
	6 + IMAPEXT,
#else /* !NO_MACRO */
static struct KEYMAPE (5 + IMAPEXT) cXmap = {
	5,
	5 + IMAPEXT,
#endif /* !NO_MACRO */
	rescan,
	{
		{
			CCHR('B'), CCHR('G'), cXcB, (KEYMAP *)NULL
		},
		{
			CCHR('L'), CCHR('X'), cXcL, (KEYMAP *)NULL
		},
#ifndef NO_MACRO
		{
			'(', ')', cXlp, (KEYMAP *)NULL
		},
#endif /* !NO_MACRO */
		{
			'0', '4', cX0, (KEYMAP *) & cX4map
		},
		{
			'=', '=', cXeq, (KEYMAP *)NULL
		},
		{
			'^', 's', cXcar, (KEYMAP *)NULL
		},
	}
};

static PF metacG[] = {
	ctrlg,			/* ^G */
};

static PF metacV[] = {
	pagenext,		/* ^V */
};

static PF metasp[] = {
	justone,		/* space */
};

static PF metapct[] = {
	queryrepl,		/* % */
};

static PF metami[] = {
	negative_argument,	/* - */
	rescan,			/* . */
	rescan,			/* / */
	digit_argument,		/* 0 */
	digit_argument,		/* 1 */
	digit_argument,		/* 2 */
	digit_argument,		/* 3 */
	digit_argument,		/* 4 */
	digit_argument,		/* 5 */
	digit_argument,		/* 6 */
	digit_argument,		/* 7 */
	digit_argument,		/* 8 */
	digit_argument,		/* 9 */
	rescan,			/* : */
	rescan,			/* ; */
	gotobob,		/* < */
	rescan,			/* = */
	gotoeob,		/* > */
};

static PF metalb[] = {
	gotobop,		/* [ */
	delwhite,		/* \ */
	gotoeop,		/* ] */
	rescan,			/* ^ */
	rescan,			/* _ */
	rescan,			/* ` */
	rescan,			/* a */
	backword,		/* b */
	capword,		/* c */
	delfword,		/* d */
	rescan,			/* e */
	forwword,		/* f */
};

static PF metal[] = {
	lowerword,		/* l */
	rescan,			/* m */
	rescan,			/* n */
	rescan,			/* o */
	rescan,			/* p */
	fillpara,		/* q */
	backsearch,		/* r */
	forwsearch,		/* s */
	rescan,			/* t */
	upperword,		/* u */
	backpage,		/* v */
	copyregion,		/* w */
	extend,			/* x */
};

static PF metatilde[] = {
	notmodified,		/* ~ */
	delbword,		/* DEL */
};

static struct KEYMAPE (8 + IMAPEXT) metamap = {
	8,
	8 + IMAPEXT,
	rescan,
	{
		{
			CCHR('G'), CCHR('G'), metacG, (KEYMAP *)NULL
		},
		{
			CCHR('V'), CCHR('V'), metacV, (KEYMAP *)NULL
		},
		{
			' ', ' ', metasp, (KEYMAP *)NULL
		},
		{
			'%', '%', metapct, (KEYMAP *)NULL
		},
		{
			'-', '>', metami, (KEYMAP *)NULL
		},
		{
			'[', 'f', metalb, (KEYMAP *)NULL
		},
		{
			'l', 'x', metal, (KEYMAP *) NULL
		},
		{
			'~', CCHR('?'), metatilde, (KEYMAP *)NULL
		},
	}
};

static PF fund_at[] = {
	setmark,		/* ^@ */
	gotobol,		/* ^A */
	backchar,		/* ^B */
	rescan,			/* ^C */
	forwdel,		/* ^D */
	gotoeol,		/* ^E */
	forwchar,		/* ^F */
	ctrlg,			/* ^G */
#ifndef NO_HELP
	prefix,			/* ^H */
#else /* !NO_HELP */
	rescan,			/* ^H */
#endif /* !NO_HELP */
};

/* ^I is selfinsert */
static PF fund_CJ[] = {
	indent,			/* ^J */
	killline,		/* ^K */
	reposition,		/* ^L */
	newline,		/* ^M */
	forwline,		/* ^N */
	openline,		/* ^O */
	backline,		/* ^P */
	quote,			/* ^Q */
	backisearch,		/* ^R */
	forwisearch,		/* ^S */
	twiddle,		/* ^T */
	universal_argument,	/* ^U */
	forwpage,		/* ^V */
	killregion,		/* ^W */
	prefix,			/* ^X */
	yank,			/* ^Y */
	spawncli,		/* ^Z */
};

static PF fund_esc[] = {
	prefix,			/* esc */
	rescan,			/* ^\ selfinsert is default on fundamental */
	rescan,			/* ^] */
	rescan,			/* ^^ */
	rescan,			/* ^_ */
};

static PF fund_del[] = {
	backdel,		/* DEL */
};

#ifndef	FUND_XMAPS
#define NFUND_XMAPS	0	/* extra map sections after normal ones */
#endif

static struct KEYMAPE (4 + NFUND_XMAPS + IMAPEXT) fundmap = {
	4 + NFUND_XMAPS,
	4 + NFUND_XMAPS + IMAPEXT,
	selfinsert,
	{
#ifndef NO_HELP
		{
			CCHR('@'), CCHR('H'), fund_at, (KEYMAP *) & helpmap
		},
#else /* !NO_HELP */
		{
			CCHR('@'), CCHR('H'), fund_at, (KEYMAP *)NULL
		},
#endif /* !NO_HELP */
		{
			CCHR('J'), CCHR('Z'), fund_CJ, (KEYMAP *) & cXmap
		},
		{
			CCHR('['), CCHR('_'), fund_esc, (KEYMAP *) & metamap
		},
		{
			CCHR('?'), CCHR('?'), fund_del, (KEYMAP *)NULL
		},
#ifdef FUND_XMAPS
		FUND_XMAPS,
#endif /* FUND_XMAPS */
	}
};

static PF fill_sp[] = {
	fillword,		/* ' ' */
};

static struct KEYMAPE (1 + IMAPEXT) fillmap = {
	1,
	1 + IMAPEXT,
	rescan,
	{
		{
			' ', ' ', fill_sp, (KEYMAP *)NULL
		},
	}
};

static PF indent_lf[] = {
	newline,		/* ^J */
	rescan,			/* ^K */
	rescan,			/* ^L */
	indent,			/* ^M */
};

static struct KEYMAPE (1 + IMAPEXT) indntmap = {
	1,
	1 + IMAPEXT,
	rescan,
	{
		{
			CCHR('J'), CCHR('M'), indent_lf, (KEYMAP *)NULL
		},
	}
};

static PF blink_rp[] = {
	showmatch,		/* ) */
};

static struct KEYMAPE (1 + IMAPEXT) blinkmap = {
	1,
	1 + IMAPEXT,
	rescan,
	{
		{
			')', ')', blink_rp, (KEYMAP *)NULL
		},
	}
};

#ifdef NOTAB
static PF notab_tab[] = {
	space_to_tabstop,	/* ^I */
};

static struct KEYMAPE (1 + IMAPEXT) notabmap = {
	1,
	1 + IMAPEXT,
	rescan,
	{
		{
			CCHR('I'), CCHR('I'), notab_tab, (KEYMAP *)NULL
		},
	}
};
#endif /* NOTAB */

static struct KEYMAPE (1 + IMAPEXT) overwmap = {
	0,
	1 + IMAPEXT,		/* 1 to avoid 0 sized array */
	rescan,
	{
		/* unused dummy entry for VMS C */
		{
			(KCHAR)0, (KCHAR)0, (PF *)NULL, (KEYMAP *)NULL
		},
	}
};

#ifndef NO_DIRED
static PF dirednul[] = {
	setmark,		/* ^@ */
	gotobol,		/* ^A */
	backchar,		/* ^B */
	rescan,			/* ^C */
	d_del,			/* ^D */
	gotoeol,		/* ^E */
	forwchar,		/* ^F */
	ctrlg,			/* ^G */
#ifndef NO_HELP
	prefix,			/* ^H */
#endif /* !NO_HELP */
};

static PF diredcl[] = {
	reposition,		/* ^L */
	forwline,		/* ^M */
	forwline,		/* ^N */
	rescan,			/* ^O */
	backline,		/* ^P */
	rescan,			/* ^Q */
	backisearch,		/* ^R */
	forwisearch,		/* ^S */
	rescan,			/* ^T */
	universal_argument,	/* ^U */
	forwpage,		/* ^V */
	rescan,			/* ^W */
	prefix,			/* ^X */
};

static PF diredcz[] = {
	spawncli,		/* ^Z */
	prefix,			/* esc */
	rescan,			/* ^\ */
	rescan,			/* ^] */
	rescan,			/* ^^ */
	rescan,			/* ^_ */
	forwline,		/* SP */
};

static PF diredc[] = {
	d_copy,			/* c */
	d_del,			/* d */
	d_findfile,		/* e */
	d_findfile,		/* f */
};

static PF diredn[] = {
	forwline,		/* n */
	d_ffotherwindow,	/* o */
	backline,		/* p */
	rescan,			/* q */
	d_rename,		/* r */
	rescan,			/* s */
	rescan,			/* t */
	d_undel,		/* u */
	rescan,			/* v */
	rescan,			/* w */
	d_expunge,		/* x */
};

static PF direddl[] = {
	d_undelbak,		/* del */
};

#ifndef	DIRED_XMAPS
#define	NDIRED_XMAPS	0	/* number of extra map sections */
#endif /* DIRED_XMAPS */

static struct KEYMAPE (6 + NDIRED_XMAPS + IMAPEXT) diredmap = {
	6 + NDIRED_XMAPS,
	6 + NDIRED_XMAPS + IMAPEXT,
	rescan,
	{
#ifndef NO_HELP
		{
			CCHR('@'), CCHR('H'), dirednul, (KEYMAP *) & helpmap
		},
#else /* !NO_HELP */
		{
			CCHR('@'), CCHR('G'), dirednul, (KEYMAP *)NULL
		},
#endif /* !NO_HELP */
		{
			CCHR('L'), CCHR('X'), diredcl, (KEYMAP *) & cXmap
		},
		{
			CCHR('Z'), ' ', diredcz, (KEYMAP *) & metamap
		},
		{
			'c', 'f', diredc, (KEYMAP *)NULL
		},
		{
			'n', 'x', diredn, (KEYMAP *)NULL
		},
		{
			CCHR('?'), CCHR('?'), direddl, (KEYMAP *)NULL
		},
#ifdef	DIRED_XMAPS
		DIRED_XMAPS,	/* map sections for dired mode keys	 */
#endif /* DIRED_XMAPS */
	}
};
#endif /* !NO_DIRED */

/*
 * give names to the maps, for use by help etc. If the map is to be bindable,
 * it must also be listed in the function name table below with the same
 * name. Maps created dynamicly currently don't get added here, thus are
 * unnamed. Modes are just named keymaps with functions to add/subtract them
 * from a buffer's list of modes.  If you change a mode name, change it in
 * modes.c also.
 */

MAPS map_table[] = {
	/* fundamental map MUST be first entry */
	{(KEYMAP *) & fundmap, "fundamental"},
	{(KEYMAP *) & fillmap, "fill"},
	{(KEYMAP *) & indntmap, "indent"},
	{(KEYMAP *) & blinkmap, "blink"},
#ifdef NOTAB
	{(KEYMAP *) & notabmap, "notab"},
#endif /* NOTAB */
	{(KEYMAP *) & overwmap, "overwrite"},
	{(KEYMAP *) & metamap, "esc prefix"},
	{(KEYMAP *) & cXmap, "c-x prefix"},
	{(KEYMAP *) & cX4map, "c-x 4 prefix"},
	{(KEYMAP *) & extramap1, "extra prefix 1"},
	{(KEYMAP *) & extramap2, "extra prefix 2"},
	{(KEYMAP *) & extramap3, "extra prefix 3"},
	{(KEYMAP *) & extramap4, "extra prefix 4"},
	{(KEYMAP *) & extramap5, "extra prefix 5"},
#ifndef NO_HELP
	{(KEYMAP *) & helpmap, "help"},
#endif
#ifndef NO_DIRED
	{(KEYMAP *) & diredmap, "dired"},
#endif
};

#define NMAPS	(sizeof map_table/sizeof(MAPS))
int	 nmaps = NMAPS;		/* for use by rebind in extend.c */

char *
map_name(map)
	KEYMAP *map;
{
	MAPS	*mp = &map_table[0];

	do {
		if (mp->p_map == map)
			return mp->p_name;
	} while (++mp < &map_table[NMAPS]);
	return (char *)NULL;
}

MAPS *
name_mode(name)
	char *name;
{
	MAPS	*mp = &map_table[0];

	do {
		if (strcmp(mp->p_name, name) == 0)
			return mp;
	} while (++mp < &map_table[NMAPS]);
	return (MAPS *)NULL;
}

KEYMAP *
name_map(name)
	char *name;
{
	MAPS	*mp;
	return (mp = name_mode(name)) == NULL ? (KEYMAP *)NULL : mp->p_map;
}

/*
 * Warning: functnames MUST be in alphabetical order!  (due to binary search
 * in name_function.)  If the function is prefix, it must be listed with the
 * same name in the map_table above.
 */
FUNCTNAMES functnames[] = {
#ifndef	NO_HELP
	{apropos_command, "apropos"},
#endif /* !NO_HELP */
	{fillmode, "auto-fill-mode"},
	{indentmode, "auto-indent-mode"},
	{backchar, "backward-char"},
	{delbword, "backward-kill-word"},
	{gotobop, "backward-paragraph"},
	{backword, "backward-word"},
	{gotobob, "beginning-of-buffer"},
	{gotobol, "beginning-of-line"},
	{blinkparen, "blink-matching-paren"},
	{showmatch, "blink-matching-paren-hack"},
#ifdef BSMAP
	{bsmap, "bsmap-mode"},
#endif /* BSMAP */
	{prefix, "c-x 4 prefix"},
	{prefix, "c-x prefix"},
#ifndef NO_MACRO
	{executemacro, "call-last-kbd-macro"},
#endif /* !NO_MACRO */
	{capword, "capitalize-word"},
#ifndef NO_DIR
	{changedir, "cd"},
#endif /* !NO_DIR */
	{copyregion, "copy-region-as-kill"},
#ifdef	REGEX
	{cntmatchlines, "count-matches"},
	{cntnonmatchlines, "count-non-matches"},
#endif /* REGEX */
	{define_key, "define-key"},
	{backdel, "delete-backward-char"},
	{deblank, "delete-blank-lines"},
	{forwdel, "delete-char"},
	{delwhite, "delete-horizontal-space"},
#ifdef	REGEX
	{delmatchlines, "delete-matching-lines"},
	{delnonmatchlines, "delete-non-matching-lines"},
#endif /* REGEX */
	{onlywind, "delete-other-windows"},
	{delwind, "delete-window"},
#ifndef NO_HELP
	{wallchart, "describe-bindings"},
	{desckey, "describe-key-briefly"},
#endif /* !NO_HELP */
	{digit_argument, "digit-argument"},
#ifndef NO_DIRED
	{dired, "dired"},
	{d_undelbak, "dired-backup-unflag"},
	{d_copy, "dired-copy-file"},
	{d_expunge, "dired-do-deletions"},
	{d_findfile, "dired-find-file"},
	{d_ffotherwindow, "dired-find-file-other-window"},
	{d_del, "dired-flag-file-deleted"},
	{d_otherwindow, "dired-other-window"},
	{d_rename, "dired-rename-file"},
	{d_undel, "dired-unflag"},
#endif /* !NO_DIRED */
	{lowerregion, "downcase-region"},
	{lowerword, "downcase-word"},
	{showversion, "emacs-version"},
#ifndef NO_MACRO
	{finishmacro, "end-kbd-macro"},
#endif /* !NO_MACRO */
	{gotoeob, "end-of-buffer"},
	{gotoeol, "end-of-line"},
	{enlargewind, "enlarge-window"},
	{prefix, "esc prefix"},
#ifndef NO_STARTUP
	{evalbuffer, "eval-current-buffer"},
	{evalexpr, "eval-expression"},
#endif /* !NO_STARTUP */
	{swapmark, "exchange-point-and-mark"},
	{extend, "execute-extended-command"},
	{prefix, "extra prefix 1"},
	{prefix, "extra prefix 2"},
	{prefix, "extra prefix 3"},
	{prefix, "extra prefix 4"},
	{prefix, "extra prefix 5"},
	{fillpara, "fill-paragraph"},
	{filevisit, "find-file"},
	{poptofile, "find-file-other-window"},
	{forwchar, "forward-char"},
	{gotoeop, "forward-paragraph"},
	{forwword, "forward-word"},
	{bindtokey, "global-set-key"},
	{unbindtokey, "global-unset-key"},
	{gotoline, "goto-line"},
#ifndef NO_HELP
	{prefix, "help"},
	{help_help, "help-help"},
#endif /* !NO_HELP */
	{insert, "insert"},
	{bufferinsert, "insert-buffer"},
	{fileinsert, "insert-file"},
	{fillword, "insert-with-wrap"},
	{backisearch, "isearch-backward"},
	{forwisearch, "isearch-forward"},
	{justone, "just-one-space"},
	{ctrlg, "keyboard-quit"},
	{killbuffer, "kill-buffer"},
	{killline, "kill-line"},
	{killpara, "kill-paragraph"},
	{killregion, "kill-region"},
	{delfword, "kill-word"},
	{listbuffers, "list-buffers"},
#ifndef NO_STARTUP
	{evalfile, "load"},
#endif /* !NO_STARTUP */
	{localbind, "local-set-key"},
	{localunbind, "local-unset-key"},
#ifndef NO_BACKUP
	{makebkfile, "make-backup-files"},
#endif /* !NO_BACKUP */
#ifdef DO_METAKEY
	{do_meta, "meta-key-mode"},	/* better name, anyone? */
#endif /* DO_METAKEY */
	{negative_argument, "negative-argument"},
	{newline, "newline"},
	{indent, "newline-and-indent"},
	{forwline, "next-line"},
#ifdef NOTAB
	{notabmode, "no-tab-mode"},
#endif /* NOTAB */
	{notmodified, "not-modified"},
	{openline, "open-line"},
	{nextwind, "other-window"},
	{overwrite, "overwrite-mode"},
#ifdef PREFIXREGION
	{prefixregion, "prefix-region"},
#endif /* PREFIXREGION */
	{backline, "previous-line"},
#ifdef GOSMACS
	{prevwind, "previous-window"},
#endif /* GOSEMACS */
	{spawncli, "push-shell"},
#ifndef NO_DIR
	{showcwdir, "pwd"},
#endif /* !NO_DIR */
	{queryrepl, "query-replace"},
#ifdef REGEX
	{re_queryrepl, "query-replace-regexp"},
#endif /* REGEX */
	{quote, "quoted-insert"},
#ifdef REGEX
	{re_searchagain, "re-search-again"},
	{re_backsearch, "re-search-backward"},
	{re_forwsearch, "re-search-forward"},
#endif /* REGEX */
	{reposition, "recenter"},
	{refresh, "redraw-display"},
	{filesave, "save-buffer"},
	{quit, "save-buffers-kill-emacs"},
	{savebuffers, "save-some-buffers"},
	{backpage, "scroll-down"},
#ifdef GOSMACS
	{back1page, "scroll-one-line-down"},
	{forw1page, "scroll-one-line-up"},
#endif /* GOSMACS */
	{pagenext, "scroll-other-window"},
	{forwpage, "scroll-up"},
	{searchagain, "search-again"},
	{backsearch, "search-backward"},
	{forwsearch, "search-forward"},
	{selfinsert, "self-insert-command"},
#ifdef REGEX
	{setcasefold, "set-case-fold-search"},
#endif /* REGEX */
	{set_default_mode, "set-default-mode"},
	{setfillcol, "set-fill-column"},
	{setmark, "set-mark-command"},
#ifdef PREFIXREGION
	{setprefix, "set-prefix-string"},
#endif /* PREFIXREGION */
	{shrinkwind, "shrink-window"},
#ifdef NOTAB
	{space_to_tabstop, "space-to-tabstop"},
#endif /* NOTAB */
	{splitwind, "split-window-vertically"},
#ifndef NO_MACRO
	{definemacro, "start-kbd-macro"},
#endif /* !NO_MACRO */
	{spawncli, "suspend-emacs"},
	{usebuffer, "switch-to-buffer"},
	{poptobuffer, "switch-to-buffer-other-window"},
	{twiddle, "transpose-chars"},
	{universal_argument, "universal-argument"},
	{upperregion, "upcase-region"},
	{upperword, "upcase-word"},
	{showcpos, "what-cursor-position"},
	{filewrite, "write-file"},
	{yank, "yank"},
};

#define NFUNCT	(sizeof(functnames)/sizeof(FUNCTNAMES))

int	 nfunct = NFUNCT;	/* used by help.c */

/*
 * The general-purpose version of ROUND2 blows osk C (2.0) out of the water.
 * (reboot required)  If you need to build a version of mg with less than 32
 * or more than 511 functions, something better must be done.
 * The version that should work, but doesn't is:
 * #define ROUND2(x) (1+((x>>1)|(x>>2)|(x>>3)|(x>>4)|(x>>5)|(x>>6)|(x>>7)|\
 *	(x>>8)|(x>>9)|(x>>10)|(x>>11)|(x>>12)|(x>>13)|(x>>14)|(x>>15)))
 */
#define ROUND2(x) (x<128?(x<64?32:64):(x<256?128:256))

static int
name_fent(fname, flag)
	char *fname;
	int   flag;
{
	int	 try, notit;
	int	 x = ROUND2(NFUNCT);
	int	 base = 0;

	do {
		/* + can be used instead of | here if more efficent.	 */
		if ((try = base | x) < NFUNCT) {
			if ((notit = strcmp(fname, functnames[try].n_name)) 
			    >= 0) {
				if (!notit)
					return try;
				base = try;
			}
		}
	/* try 0 once if needed */
	} while ((x >>= 1) || (try == 1 && base == 0));
	return flag ? base : -1;
}

/*
 * Translate from function name to function pointer, using binary search.
 */

PF
name_function(fname)
	char *fname;
{
	int	 i;
	if ((i = name_fent(fname, FALSE)) >= 0)
		return functnames[i].n_funct;
	return (PF)NULL;
}

/*
 * complete function name
 */
int
complete_function(fname, c)
	char *fname;
	int   c;
{
	int	i, j, k, l, oj;

	i = name_fent(fname, TRUE);
	for (j = 0; (l = fname[j]) && functnames[i].n_name[j] == l; j++) {
	}
	if (fname[j] != '\0') {
		if (++i >= NFUNCT)
			/* no match */
			return -2;
		for (j = 0; (l = fname[j]) && functnames[i].n_name[j] == l; 
		    j++);
		if (fname[j] != '\0')
			/* no match */
			return -2;
	}
	if (c == CCHR('M') && functnames[i].n_name[j] == '\0')
		return -1;
	/* find last match */
	for (k = i + 1; k < NFUNCT; k++) {
		for (l = 0; functnames[k].n_name[l] == fname[l]; l++);
		if (l < j)
			break;
	}
	k--;
	oj = j;

	if (k > i) {
		/* multiple matches */
		while ((l = functnames[i].n_name[j]) == 
		    functnames[k].n_name[j]) {
			fname[j++] = l;
			if (l == '-' && c == ' ')
				break;
		}
		if (j == oj)
			/* ambiguous */
			return -3;
	} else {
		/* single match */
		while ((l = functnames[i].n_name[j])) {
			fname[j++] = l;
			if (l == '-' && c == ' ')
				break;
		}
	}
	fname[j] = '\0';
	return j - oj;
}

/*
 * list possible function name completions.
 */
LIST *
complete_function_list(fname, c)
	char *fname;
	int   c;
{
	int	 i, j, k, l;
	LIST	*current, *last;

	i = name_fent(fname, TRUE);
	for (j = 0; (l = fname[j]) && functnames[i].n_name[j] == l; j++);
	if (fname[j] != '\0') {
		if (++i >= NFUNCT)
			/* no match */
			return NULL;
		for (j = 0; (l = fname[j]) && functnames[i].n_name[j] == l; 
		    j++);
		if (fname[j] != '\0')
			/* no match */
			return NULL;
	}
	/*
	 * if(c==CCHR('M') && functnames[i].n_name[j]=='\0') return -1;
	 */
	/* find last match */
	for (k = i + 1; k < NFUNCT; k++) {
		for (l = 0; functnames[k].n_name[l] == fname[l]; l++);
		if (l < j)
			break;
	}
	k--;
	last = NULL;
	for (; k >= i; k--) {
		current = (LIST *)malloc(sizeof(LIST));
		current->l_next = last;
		current->l_name = functnames[k].n_name;
		last = current;
	}
	return (last);
}

/*
 * translate from function pointer to function name.
 */
char *
function_name(fpoint)
	PF fpoint;
{
	FUNCTNAMES	*fnp = &functnames[0];

	if (fpoint == prefix)
		/* ambiguous */
		return (char *)NULL;
	do {
		if (fnp->n_funct == fpoint)
			return fnp->n_name;
	} while (++fnp < &functnames[NFUNCT]);
	return (char *)NULL;
}
