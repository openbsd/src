/*	$OpenBSD: keymap.c,v 1.24 2002/09/26 10:12:26 deraadt Exp $	*/

/*
 * Keyboard maps.  This is character set dependent.  The terminal specific
 * parts of building the keymap has been moved to a better place.
 */

#include	"def.h"
#include	"kbd.h"

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
			CCHR('G'), CCHR('H'), cHcG, NULL
		},
		{
			'a', 'c', cHa, NULL
		},
	}
};
#endif /* !NO_HELP */

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
			CCHR('F'), CCHR('G'), cX4cF, NULL
		},
		{
			'b', 'f', cX4b, NULL
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
	NULL,			/* 4 */
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
	nextwind,		/* n */
	nextwind,		/* o */
	prevwind,		/* p */
	rescan,			/* q */
	rescan,			/* r */
	savebuffers,		/* s */
	NULL,			/* t */
	undo			/* u */
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
			CCHR('B'), CCHR('G'), cXcB, NULL
		},
		{
			CCHR('L'), CCHR('X'), cXcL, NULL
		},
#ifndef NO_MACRO
		{
			'(', ')', cXlp, NULL
		},
#endif /* !NO_MACRO */
		{
			'0', '4', cX0, (KEYMAP *) & cX4map
		},
		{
			'=', '=', cXeq, NULL
		},
		{
			'^', 'u', cXcar, NULL
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
			CCHR('G'), CCHR('G'), metacG, NULL
		},
		{
			CCHR('V'), CCHR('V'), metacV, NULL
		},
		{
			' ', ' ', metasp, NULL
		},
		{
			'%', '%', metapct, NULL
		},
		{
			'-', '>', metami, NULL
		},
		{
			'[', 'f', metalb, NULL
		},
		{
			'l', 'x', metal, NULL
		},
		{
			'~', CCHR('?'), metatilde, NULL
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
	NULL,			/* ^H */
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
	NULL,			/* ^X */
	yank,			/* ^Y */
	spawncli,		/* ^Z */
};

static PF fund_esc[] = {
	NULL,			/* esc */
	rescan,			/* ^\ selfinsert is default on fundamental */
	rescan,			/* ^] */
	rescan,			/* ^^ */
	undo,			/* ^_ */
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
			CCHR('@'), CCHR('H'), fund_at, NULL
		},
#endif /* !NO_HELP */
		{
			CCHR('J'), CCHR('Z'), fund_CJ, (KEYMAP *) & cXmap
		},
		{
			CCHR('['), CCHR('_'), fund_esc, (KEYMAP *) & metamap
		},
		{
			CCHR('?'), CCHR('?'), fund_del, NULL
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
			' ', ' ', fill_sp, NULL
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
			CCHR('J'), CCHR('M'), indent_lf, NULL
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
			')', ')', blink_rp, NULL
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
			CCHR('I'), CCHR('I'), notab_tab, NULL
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
			(KCHAR)0, (KCHAR)0, NULL, NULL
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
	NULL,			/* ^H */
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
	NULL,			/* ^X */
};

static PF diredcz[] = {
	spawncli,		/* ^Z */
	NULL,			/* esc */
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
			CCHR('@'), CCHR('G'), dirednul, NULL
		},
#endif /* !NO_HELP */
		{
			CCHR('L'), CCHR('X'), diredcl, (KEYMAP *) & cXmap
		},
		{
			CCHR('Z'), ' ', diredcz, (KEYMAP *) & metamap
		},
		{
			'c', 'f', diredc, NULL
		},
		{
			'n', 'x', diredn, NULL
		},
		{
			CCHR('?'), CCHR('?'), direddl, NULL
		},
#ifdef	DIRED_XMAPS
		DIRED_XMAPS,	/* map sections for dired mode keys	 */
#endif /* DIRED_XMAPS */
	}
};
#endif /* !NO_DIRED */

MAPS	fundamental_mode = { (KEYMAP *)&fundmap, "fundamental", };

/*
 * give names to the maps, for use by help etc. If the map is to be bindable,
 * it must also be listed in the function name table below with the same
 * name. Maps created dynamicly currently don't get added here, thus are
 * unnamed. Modes are just named keymaps with functions to add/subtract them
 * from a buffer's list of modes.  If you change a mode name, change it in
 * modes.c also.
 */

static MAPS map_table[] = {
	{(KEYMAP *) &fillmap, "fill",},
	{(KEYMAP *) &indntmap, "indent",},
	{(KEYMAP *) &blinkmap, "blink",},
#ifdef NOTAB
	{(KEYMAP *) &notabmap, "notab",},
#endif /* NOTAB */
	{(KEYMAP *) &overwmap, "overwrite",},
	{(KEYMAP *) &metamap, "esc prefix",},
	{(KEYMAP *) &cXmap, "c-x prefix",},
	{(KEYMAP *) &cX4map, "c-x 4 prefix",},
#ifndef NO_HELP
	{(KEYMAP *) &helpmap, "help",},
#endif
#ifndef NO_DIRED
	{(KEYMAP *) &diredmap, "dired",},
#endif
	{NULL, NULL},
};

MAPS *maps;

void
maps_init(void)
{
	int i;
	MAPS *mp;

	maps = &fundamental_mode;
	for (i = 0; map_table[i].p_name != NULL; i++) {
		mp = &map_table[i];
		mp->p_next = maps;
		maps = mp;
	}
}

int
maps_add(KEYMAP *map, const char *name)
{
	MAPS *mp;

	if ((mp = malloc(sizeof(*mp))) == NULL)
		return FALSE;

	mp->p_name = name;
	mp->p_map = map;
	mp->p_next = maps;
	maps = mp;

	return TRUE;
}

const char *
map_name(KEYMAP *map)
{
	MAPS *mp;

	for (mp = maps; mp != NULL; mp = mp->p_next)
		if (mp->p_map == map)
			return mp->p_name;
	return NULL;
}

MAPS *
name_mode(const char *name)
{
	MAPS *mp;

	for (mp = maps; mp != NULL; mp = mp->p_next)
		if (strcmp(mp->p_name, name) == 0)
			return mp;
	return NULL;
}

KEYMAP *
name_map(const char *name)
{
	MAPS	*mp;
	return (mp = name_mode(name)) == NULL ? NULL : mp->p_map;
}

