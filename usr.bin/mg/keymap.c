/*	$OpenBSD: keymap.c,v 1.55 2015/03/19 21:48:05 bcallah Exp $	*/

/* This file is in the public domain. */

/*
 * Keyboard maps.  This is character set dependent.  The terminal specific
 * parts of building the keymap has been moved to a better place.
 */

#include <sys/queue.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "def.h"
#include "kbd.h"

/*
 * initial keymap declarations, deepest first
 */

static PF cHcG[] = {
	ctrlg,			/* ^G */
	help_help		/* ^H */
};

static PF cHa[] = {
	apropos_command,	/* a */
	wallchart,		/* b */
	desckey			/* c */
};

struct KEYMAPE (2) helpmap = {
	2,
	2,
	rescan,
	{
		{
			CCHR('G'), CCHR('H'), cHcG, NULL
		},
		{
			'a', 'c', cHa, NULL
		}
	}
};

static PF cCsc[] = {
	cscallerfuncs,		/* c */
	csdefinition,		/* d */
	csegrep,		/* e */
	csfindfile,		/* f */
	rescan,			/* g */
	rescan,			/* h */
	csfindinc,		/* i */
	rescan,			/* j */
	rescan,			/* k */
	rescan,			/* l */
	rescan,			/* m */
	csnextmatch,		/* n */
	rescan,			/* o */
	csprevmatch,		/* p */
	rescan,			/* q */
	rescan, 		/* r */	
	cssymbol,		/* s */
	csfindtext		/* t */
};

static struct KEYMAPE (1) cCsmap = {
	1,
	1,
	rescan,
	{
		{
			'c', 't', cCsc, NULL
		}
	}
};

static PF cCs[] = {
	NULL			/* s */
};

struct KEYMAPE (2) ccmap = {
	2,
	2,
	rescan,
	{
		{
			CCHR('@'), CCHR('@'), (PF[]){ rescan }, NULL
		},
		{
			's', 's', cCs, (KEYMAP *) & cCsmap
		}
	}
};

static PF cX4cF[] = {
	poptofile,		/* ^f */
	ctrlg			/* ^g */
};
static PF cX4b[] = {
	poptobuffer,		/* b */
	rescan,			/* c */
	rescan,			/* d */
	rescan,			/* e */
	poptofile		/* f */
};
static struct KEYMAPE (2) cX4map = {
	2,
	2,
	rescan,
	{
		{
			CCHR('F'), CCHR('G'), cX4cF, NULL
		},
		{
			'b', 'f', cX4b, NULL
		}
	}
};

static PF cXcB[] = {
	listbuffers,		/* ^B */
	quit,			/* ^C */
	rescan,			/* ^D */
	rescan,			/* ^E */
	filevisit,		/* ^F */
	ctrlg			/* ^G */
};

static PF cXcL[] = {
	lowerregion,		/* ^L */
	rescan,			/* ^M */
	rescan,			/* ^N */
	deblank,		/* ^O */
	rescan,			/* ^P */
	togglereadonly,		/* ^Q */
	filevisitro,		/* ^R */
	filesave,		/* ^S */
	rescan,			/* ^T */
	upperregion,		/* ^U */
	filevisitalt,		/* ^V */
	filewrite,		/* ^W */
	swapmark		/* ^X */
};

static PF cXlp[] = {
	definemacro,		/* ( */
	finishmacro		/* ) */
};

static PF cX0[] = {
	delwind,		/* 0 */
	onlywind,		/* 1 */
	splitwind,		/* 2 */
	rescan,			/* 3 */
	NULL			/* 4 */
};

static PF cXeq[] = {
	showcpos		/* = */
};

static PF cXcar[] = {
	enlargewind,		/* ^ */
	rescan,			/* _ */
	next_error,		/* ` */
	rescan,			/* a */
	usebuffer,		/* b */
	rescan,			/* c */
	rescan,			/* d */
	executemacro,		/* e */
	setfillcol,		/* f */
	gotoline,		/* g */
	markbuffer,		/* h */
	fileinsert,		/* i */
	rescan,			/* j */
	killbuffer_cmd,		/* k */
	rescan,			/* l */
	rescan,			/* m */
	nextwind,		/* n */
	nextwind,		/* o */
	prevwind,		/* p */
	rescan,			/* q */
	rescan,			/* r */
	savebuffers,		/* s */
	rescan,			/* t */
	undo			/* u */
};

struct KEYMAPE (6) cXmap = {
	6,
	6,
	rescan,
	{
		{
			CCHR('B'), CCHR('G'), cXcB, NULL
		},
		{
			CCHR('L'), CCHR('X'), cXcL, NULL
		},
		{
			'(', ')', cXlp, NULL
		},
		{
			'0', '4', cX0, (KEYMAP *) & cX4map
		},
		{
			'=', '=', cXeq, NULL
		},
		{
			'^', 'u', cXcar, NULL
		}
	}
};

static PF metacG[] = {
	ctrlg			/* ^G */
};

static PF metacV[] = {
	pagenext		/* ^V */
};

static PF metaspex[] = {
	justone,		/* space */
	shellcommand		/* ! */
};

static PF metapct[] = {
	queryrepl		/* % */
};

static PF metami[] = {
	poptag,                 /* * */
	rescan,                 /* + */
	rescan,                 /* , */
	negative_argument,	/* - */
	findtag,		/* . */
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
	gotoeob			/* > */
};

static PF metasqf[] = {
	NULL,			/* [ */
	delwhite,		/* \ */
	rescan,			/* ] */
	joinline,		/* ^ */
	rescan,			/* _ */
	rescan,			/* ` */
	rescan,			/* a */
	backword,		/* b */
	capword,		/* c */
	delfword,		/* d */
	rescan,			/* e */
	forwword		/* f */
};

static PF metal[] = {
	lowerword,		/* l */
	backtoindent,		/* m */
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
	rescan,			/* y */
	rescan,			/* z */
	gotobop,		/* { */
	piperegion,		/* | */
	gotoeop			/* } */
};

static PF metasqlZ[] = {
	rescan			/* Z */
};

static PF metatilde[] = {
	notmodified,		/* ~ */
	delbword		/* DEL */
};

struct KEYMAPE (1) metasqlmap = {
	1,
	1,
	rescan,
	{
		{
			'Z', 'Z', metasqlZ, NULL
		}
	}
};

struct KEYMAPE (8) metamap = {
	8,
	8,
	rescan,
	{
		{
			CCHR('G'), CCHR('G'), metacG, NULL
		},
		{
			CCHR('V'), CCHR('V'), metacV, NULL
		},
		{
			' ', '!', metaspex, NULL
		},
		{
			'%', '%', metapct, NULL
		},
		{
			'*', '>', metami, NULL
		},
		{
			'[', 'f', metasqf, (KEYMAP *) &metasqlmap
		},
		{
			'l', '}', metal, NULL
		},
		{
			'~', CCHR('?'), metatilde, NULL
		}
	}
};

static PF fund_at[] = {
	setmark,		/* ^@ */
	gotobol,		/* ^A */
	backchar,		/* ^B */
	NULL,			/* ^C */
	forwdel,		/* ^D */
	gotoeol,		/* ^E */
	forwchar,		/* ^F */
	ctrlg,			/* ^G */
};

static PF fund_h[] = {
	NULL,			/* ^H */
};


/* ^I is selfinsert */
static PF fund_CJ[] = {
	lfindent,		/* ^J */
	killline,		/* ^K */
	reposition,		/* ^L */
	enewline,		/* ^M */
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
	spawncli		/* ^Z */
};

static PF fund_esc[] = {
	NULL,			/* esc */
	rescan,			/* ^\ selfinsert is default on fundamental */
	rescan,			/* ^] */
	rescan,			/* ^^ */
	undo			/* ^_ */
};

static PF fund_del[] = {
	backdel			/* DEL */
};

static PF fund_cb[] = {
	showmatch		/* ) ] }  */
};

static struct KEYMAPE (8) fundmap = {
	8,
	8,
	selfinsert,
	{
		{
			CCHR('@'), CCHR('G'), fund_at, (KEYMAP *) & ccmap
		},
		{
			CCHR('H'), CCHR('H'), fund_h, (KEYMAP *) & helpmap
		},
		{
			CCHR('J'), CCHR('Z'), fund_CJ, (KEYMAP *) & cXmap
		},
		{
			CCHR('['), CCHR('_'), fund_esc, (KEYMAP *) & metamap
		},
		{
			')', ')', fund_cb, NULL
		},
		{
			']', ']', fund_cb, NULL
		},
		{
			'}', '}', fund_cb, NULL
		},
		{
			CCHR('?'), CCHR('?'), fund_del, NULL
		},
	}
};

static PF fill_sp[] = {
	fillword		/* ' ' */
};

static struct KEYMAPE (1) fillmap = {
	1,
	1,
	rescan,
	{
		{ ' ', ' ', fill_sp, NULL }
	}
};

static PF indent_lf[] = {
	enewline,		/* ^J */
	rescan,			/* ^K */
	rescan,			/* ^L */
	lfindent		/* ^M */
};

static struct KEYMAPE (1) indntmap = {
	1,
	1,
	rescan,
	{
		{
			CCHR('J'), CCHR('M'), indent_lf, NULL
		}
	}
};

#ifdef NOTAB
static PF notab_tab[] = {
	space_to_tabstop	/* ^I */
};

static struct KEYMAPE (1) notabmap = {
	1,
	1,
	rescan,
	{
		{
			CCHR('I'), CCHR('I'), notab_tab, NULL
		}
	}
};
#endif /* NOTAB */

static struct KEYMAPE (1) overwmap = {
	0,
	1,		/* 1 to avoid 0 sized array */
	rescan,
	{
		/* unused dummy entry for VMS C */
		{
			(KCHAR)0, (KCHAR)0, NULL, NULL
		}
	}
};


/*
 * The basic (root) keyboard map
 */
struct maps_s	fundamental_mode = { (KEYMAP *)&fundmap, "fundamental" };

/*
 * give names to the maps, for use by help etc. If the map is to be bindable,
 * it must also be listed in the function name table below with the same
 * name. Maps created dynamically currently don't get added here, thus are
 * unnamed. Modes are just named keymaps with functions to add/subtract them
 * from a buffer's list of modes.  If you change a mode name, change it in
 * modes.c also.
 */

static struct maps_s map_table[] = {
	{(KEYMAP *) &fillmap, "fill",},
	{(KEYMAP *) &indntmap, "indent",},
#ifdef NOTAB
	{(KEYMAP *) &notabmap, "notab",},
#endif /* NOTAB */
	{(KEYMAP *) &overwmap, "overwrite",},
	{(KEYMAP *) &metamap, "esc prefix",},
	{(KEYMAP *) &cXmap, "c-x prefix",},
	{(KEYMAP *) &cX4map, "c-x 4 prefix",},
	{(KEYMAP *) &helpmap, "help",},
	{NULL, NULL}
};

struct maps_s *maps;

void
maps_init(void)
{
	int	 i;
	struct maps_s	*mp;

	maps = &fundamental_mode;
	for (i = 0; map_table[i].p_name != NULL; i++) {
		mp = &map_table[i];
		mp->p_next = maps;
		maps = mp;
	}
}

/*
 * Insert a new (named) keymap at the head of the keymap list.
 */
int
maps_add(KEYMAP *map, const char *name)
{
	struct maps_s	*mp;

	if ((mp = malloc(sizeof(*mp))) == NULL)
		return (FALSE);

	mp->p_name = name;
	mp->p_map = map;
	mp->p_next = maps;
	maps = mp;

	return (TRUE);
}

struct maps_s *
name_mode(const char *name)
{
	struct maps_s	*mp;

	for (mp = maps; mp != NULL; mp = mp->p_next)
		if (strcmp(mp->p_name, name) == 0)
			return (mp);
	return (NULL);
}

KEYMAP *
name_map(const char *name)
{
	struct maps_s	*mp;

	return ((mp = name_mode(name)) == NULL ? NULL : mp->p_map);
}
