/* character level styles for Lynx
 * (c) 1996 Rob Partington -- donated to the Lyncei (if they want it :-)
 * $Id: LYStyle.c,v 1.4 2004/06/22 04:01:50 avsm Exp $
 */
#include <HTUtils.h>
#include <HTML.h>
#include <LYGlobalDefs.h>

#include <LYStructs.h>
#include <LYReadCFG.h>
#include <LYCurses.h>
#include <LYCharUtils.h>
#include <LYUtils.h>		/* defines TABLESIZE */
#include <AttrList.h>
#include <SGML.h>
#include <HTMLDTD.h>

/* Hash table definitions */
#include <LYHash.h>
#include <LYStyle.h>

#include <LYexit.h>
#include <LYLeaks.h>
#include <LYStrings.h>

#ifdef USE_COLOR_STYLE

PRIVATE void style_initialiseHashTable NOPARAMS;

/* stack of attributes during page rendering */
PUBLIC int last_styles[128] = { 0 };
PUBLIC int last_colorattr_ptr = 0;

PUBLIC bucket hashStyles[CSHASHSIZE];
PUBLIC bucket special_bucket =
{
    "<special>", /* in order something to be in trace. */
    0, 0, 0, 0, NULL
};
PUBLIC bucket nostyle_bucket =
{
    "<NOSTYLE>", /* in order something to be in trace. */
    0, 0, 0, 0, NULL
};

PUBLIC int cached_tag_styles[HTML_ELEMENTS];
PUBLIC int current_tag_style;
PUBLIC BOOL force_current_tag_style = FALSE;
PUBLIC char* forced_classname;
PUBLIC BOOL force_classname;

/* Remember the hash codes for common elements */
PUBLIC int s_a			= NOSTYLE;
PUBLIC int s_aedit		= NOSTYLE;
PUBLIC int s_aedit_arr		= NOSTYLE;
PUBLIC int s_aedit_pad		= NOSTYLE;
PUBLIC int s_aedit_sel		= NOSTYLE;
PUBLIC int s_alert		= NOSTYLE;
PUBLIC int s_alink		= NOSTYLE;
PUBLIC int s_curedit		= NOSTYLE;
PUBLIC int s_forw_backw		= NOSTYLE;
PUBLIC int s_hot_paste		= NOSTYLE;
PUBLIC int s_menu_active	= NOSTYLE;
PUBLIC int s_menu_bg		= NOSTYLE;
PUBLIC int s_menu_entry		= NOSTYLE;
PUBLIC int s_menu_frame		= NOSTYLE;
PUBLIC int s_menu_number	= NOSTYLE;
PUBLIC int s_menu_sb		= NOSTYLE;
PUBLIC int s_normal		= NOSTYLE;
PUBLIC int s_prompt_edit	= NOSTYLE;
PUBLIC int s_prompt_edit_arr	= NOSTYLE;
PUBLIC int s_prompt_edit_pad	= NOSTYLE;
PUBLIC int s_prompt_sel		= NOSTYLE;
PUBLIC int s_status		= NOSTYLE;
PUBLIC int s_title		= NOSTYLE;
PUBLIC int s_whereis		= NOSTYLE;

#ifdef USE_SCROLLBAR
PUBLIC int s_sb_aa		= NOSTYLE;
PUBLIC int s_sb_bar		= NOSTYLE;
PUBLIC int s_sb_bg		= NOSTYLE;
PUBLIC int s_sb_naa		= NOSTYLE;
#endif

/* start somewhere safe */
#define MAX_COLOR 16
PRIVATE int colorPairs = 0;

#ifdef USE_BLINK
#  define MAX_BLINK	2
#  define M_BLINK	A_BLINK
#else
#  define MAX_BLINK	1
#  define M_BLINK	0
#endif

PRIVATE unsigned char our_pairs[2]
				[MAX_BLINK]
				[MAX_COLOR + 1]
				[MAX_COLOR + 1];

/*
 * Parse a string containing a combination of video attributes and color.
 */
PRIVATE void parse_either ARGS4(
    char *,	attrs,
    int,	dft_color,
    int *,	monop,
    int *,	colorp)
{
    int value;

    while (*attrs != '\0') {
	char *next = strchr(attrs, '+');
	char save = (next != NULL) ? *next : '\0';
	if (next == NULL)
	    next = attrs + strlen(attrs);

	if (save != 0)	/* attrs might be a constant string */
	    *next = '\0';
	if ((value = string_to_attr(attrs)) != 0)
	    *monop |= value;
	else if (colorp != 0
	 && (value = check_color(attrs, dft_color)) != ERR_COLOR)
	    *colorp = value;

	attrs = next;
	if (save != '\0')
	    *attrs++ = save;
    }
}

/* icky parsing of the style options */
PRIVATE void parse_attributes ARGS5(
    char *,	mono,
    char *,	fg,
    char *,	bg,
    int,	style,
    char *,	element)
{
    int mA = A_NORMAL;
    int fA = default_fg;
    int bA = default_bg;
    int cA = A_NORMAL;
    int newstyle = hash_code(element);

    CTRACE2(TRACE_STYLE, (tfp, "CSS(PA):style d=%d / h=%d, e=%s\n", style, newstyle, element));

    parse_either(mono, ERR_COLOR, &mA, (int *)0);
    parse_either(bg, default_bg, &cA, &bA);
    parse_either(fg, default_fg, &cA, &fA);

    if (style == -1) {			/* default */
	CTRACE2(TRACE_STYLE, (tfp, "CSS(DEF):default_fg=%d, default_bg=%d\n", fA, bA));
	default_fg = fA;
	default_bg = bA;
	default_color_reset = TRUE;
	return;
    }
    if (fA == NO_COLOR) {
	bA = NO_COLOR;
    } else if (COLORS) {
#ifdef USE_BLINK
	if (term_blink_is_boldbg) {
	    if (fA >= COLORS)
		cA = A_BOLD;
	    if (bA >= COLORS)
		cA |= M_BLINK;
	} else
#endif
	if (fA >= COLORS || bA >= COLORS)
	    cA = A_BOLD;
	if (fA >= COLORS)
	    fA %= COLORS;
	if (bA >= COLORS)
	    bA %= COLORS;
    } else {
	cA = A_BOLD;
	fA = NO_COLOR;
	bA = NO_COLOR;
    }

    /*
     * If we have colour, and space to create a new colour attribute,
     * and we have a valid colour description, then add this style
     */
    if (lynx_has_color && colorPairs < COLOR_PAIRS-1 && fA != NO_COLOR) {
	int curPair = 0;
	int iFg = (1 + (fA >= 0 ? fA : 0));
	int iBg = (1 + (bA >= 0 ? bA : 0));
	int iBold = !!(cA & A_BOLD);
	int iBlink = !!(cA & M_BLINK);

	CTRACE2(TRACE_STYLE, (tfp, "parse_attributes %d/%d %d/%d %#x\n", fA, default_fg, bA, default_bg, cA));
	if (fA < MAX_COLOR
	 && bA < MAX_COLOR
#ifdef USE_CURSES_PAIR_0
	 && (cA != A_NORMAL || fA != default_fg || bA != default_bg)
#endif
	 && curPair < 255) {
	    if (our_pairs[iBold][iBlink][iFg][iBg] != 0) {
		curPair = our_pairs[iBold][iBlink][iFg][iBg];
	    } else {
		curPair = ++colorPairs;
		init_pair((short)curPair, (short)fA, (short)bA);
		our_pairs[iBold][iBlink][iFg][iBg] = curPair;
	    }
	}
	CTRACE2(TRACE_STYLE, (tfp, "CSS(CURPAIR):%d\n", curPair));
	if (style < DSTYLE_ELEMENTS)
	    setStyle(style, COLOR_PAIR(curPair)|cA, cA, mA);
	setHashStyle(newstyle, COLOR_PAIR(curPair)|cA, cA, mA, element);
    } else {
	if (lynx_has_color && fA != NO_COLOR) {
	    CTRACE2(TRACE_STYLE, (tfp, "CSS(NC): maximum of %d colorpairs exhausted\n", COLOR_PAIRS - 1));
	}
	/* only mono is set */
	if (style < DSTYLE_ELEMENTS)
	    setStyle(style, -1, -1, mA);
	setHashStyle(newstyle, -1, -1, mA, element);
    }
}

/* parse a style option of the format
 * STYLE:<OBJECT>:FG:BG
 */
PRIVATE void parse_style ARGS1(char*, param)
{
    static struct {
	char *name;
	int style;
	int *set_hash;
    } table[] = {
	{ "default",		-1,			0 }, /* default fg/bg */
	{ "alink",		DSTYLE_ALINK,		0 }, /* active link */
	{ "a",			DSTYLE_LINK,		0 }, /* normal link */
	{ "a",			HTML_A,			0 }, /* normal link */
	{ "status",		DSTYLE_STATUS,		0 }, /* status bar */
	{ "label",		DSTYLE_OPTION,		0 }, /* [INLINE]'s */
	{ "value",		DSTYLE_VALUE,		0 }, /* [INLINE]'s */
	{ "high",		DSTYLE_HIGH,		0 }, /* [INLINE]'s */
	{ "normal",		DSTYLE_NORMAL,		0 },
	{ "candy",		DSTYLE_CANDY,		0 }, /* [INLINE]'s */
	{ "whereis",		DSTYLE_WHEREIS,		&s_whereis },
	{ "edit.active.pad",	DSTYLE_ELEMENTS,	&s_aedit_pad },
	{ "edit.active.arrow",	DSTYLE_ELEMENTS,	&s_aedit_arr },
	{ "edit.active.marked",	DSTYLE_ELEMENTS,	&s_aedit_sel },
	{ "edit.active",	DSTYLE_ELEMENTS,	&s_aedit },
	{ "edit.current",	DSTYLE_ELEMENTS,	&s_curedit },
	{ "edit.prompt.pad",	DSTYLE_ELEMENTS,	&s_prompt_edit_pad },
	{ "edit.prompt.arrow",	DSTYLE_ELEMENTS,	&s_prompt_edit_arr },
	{ "edit.prompt.marked",	DSTYLE_ELEMENTS,	&s_prompt_sel },
	{ "edit.prompt",	DSTYLE_ELEMENTS,	&s_prompt_edit },
	{ "forwbackw.arrow",	DSTYLE_ELEMENTS,	&s_forw_backw },
	{ "hot.paste",		DSTYLE_ELEMENTS,	&s_hot_paste },
	{ "menu.frame",		DSTYLE_ELEMENTS,	&s_menu_frame },
	{ "menu.bg",		DSTYLE_ELEMENTS,	&s_menu_bg },
	{ "menu.n",		DSTYLE_ELEMENTS,	&s_menu_number },
	{ "menu.entry",		DSTYLE_ELEMENTS,	&s_menu_entry },
	{ "menu.active",	DSTYLE_ELEMENTS,	&s_menu_active },
	{ "menu.sb",		DSTYLE_ELEMENTS,	&s_menu_sb },
    };
    unsigned n;
    BOOL found = FALSE;

    char *buffer = 0;
    char *tmp = 0;
    char *element, *mono, *fg, *bg;

    if (param == 0)
	return;
    CTRACE2(TRACE_STYLE, (tfp, "parse_style(%s)\n", param));
    StrAllocCopy(buffer, param);
    if (buffer == 0)
	return;

    if ((tmp = strchr(buffer, ':')) == 0) {
	fprintf (stderr, gettext("\
Syntax Error parsing style in lss file:\n\
[%s]\n\
The line must be of the form:\n\
OBJECT:MONO:COLOR (ie em:bold:brightblue:white)\n\
where OBJECT is one of EM,STRONG,B,I,U,BLINK etc.\n\n"), buffer);
	if (!dump_output_immediately) {
	    exit_immediately(EXIT_FAILURE);
	}
	exit(1);
    }
    strtolower(buffer);
    *tmp = '\0';
    element = buffer;

    mono = tmp + 1;
    tmp = strchr(mono, ':');

    if (!tmp)
    {
	fg = "nocolor";
	bg = "nocolor";
    }
    else
    {
	*tmp = '\0';
	fg = tmp+1;
	tmp = strchr(fg, ':');
	if (!tmp)
	    bg = "default";
	else
	{
	    *tmp = '\0';
	    bg = tmp + 1;
	}
    }

    CTRACE2(TRACE_STYLE, (tfp, "CSSPARSE:%s => %d %s\n",
		element, hash_code(element),
		(hashStyles[hash_code(element)].name ? "used" : "")));

    strtolower(element);

    /*
    * We use some pseudo-elements, so catch these first
    */
    for (n = 0; n < TABLESIZE(table); n++) {
	if (!strcasecomp(element, table[n].name)) {
	    parse_attributes(mono, fg, bg, table[n].style, table[n].name);
	    if (table[n].set_hash != 0)
		*(table[n].set_hash) = hash_code(table[n].name);
	    found = TRUE;
	    break;
	}
    }

    if (found) {
	;
    }
    else if (!strcasecomp(element, "normal")) /* added - kw */
    {
	parse_attributes(mono,fg,bg,DSTYLE_NORMAL,"html");
	s_normal  = hash_code("html"); /* rather bizarre... - kw */
    }
    /* Ok, it must be a HTML element, so look through the list until we
    * find it
    */
    else
    {
	int element_number = -1;
	HTTag * t = SGMLFindTag(&HTML_dtd, element);
	if (t && t->name) {
	    element_number = t - HTML_dtd.tags;
	}
	if (element_number >= HTML_A &&
	    element_number < HTML_ELEMENTS)
	    parse_attributes(mono,fg,bg, element_number+STARTAT,element);
	else
	    parse_attributes(mono,fg,bg, DSTYLE_ELEMENTS,element);
    }
    FREE(buffer);
}

#ifdef LY_FIND_LEAKS
PRIVATE void free_colorstylestuff NOARGS
{
    style_initialiseHashTable();
    style_deleteStyleList();
}
#endif

/*
 * initialise the default style sheet
 * This should be able to be read from a file in CSS format :-)
 */
PRIVATE void initialise_default_stylesheet NOARGS
{
    static CONST char *table[] = {
	"a:bold:green",
	"alert:bold:yellow:red",
	"alink:reverse:yellow:black",
	"label:normal:magenta",
	"status:reverse:yellow:blue",
	"title:normal:magenta",
	"whereis:reverse+underline:magenta:cyan"
    };
    unsigned n;
    char temp[80];
    CTRACE((tfp, "initialize_default_stylesheet\n"));
    for (n = 0; n < TABLESIZE(table); n++) {
	parse_style(strcpy(temp, table[n]));
    }
}

/* Set all the buckets in the hash table to be empty */
PRIVATE void style_initialiseHashTable NOARGS
{
    int i;
    static int firsttime = 1;

    for (i = 0; i <CSHASHSIZE; i++)
    {
	if (firsttime)
	    hashStyles[i].name = NULL;
	else
	    FREE(hashStyles[i].name);
	hashStyles[i].color = 0;
	hashStyles[i].cattr = 0;
	hashStyles[i].mono  = 0;
    }
    if (firsttime) {
	firsttime = 0;
#ifdef LY_FIND_LEAKS
	atexit(free_colorstylestuff);
#endif
    }
    s_alink  = hash_code("alink");
    s_a      = hash_code("a");
    s_status = hash_code("status");
    s_alert  = hash_code("alert");
    s_title  = hash_code("title");
#ifdef USE_SCROLLBAR
    s_sb_bar = hash_code("scroll.bar");
    s_sb_bg  = hash_code("scroll.back");
    s_sb_aa  = hash_code("scroll.arrow");
    s_sb_naa = hash_code("scroll.noarrow");
#endif
}

/* because curses isn't started when we parse the config file, we
 * need to remember the STYLE: lines we encounter and parse them
 * after curses has started
 */
PRIVATE HTList *lss_styles = NULL;

PUBLIC void parse_userstyles NOARGS
{
    char *name;
    HTList *cur = lss_styles;

    colorPairs = 0;
    style_initialiseHashTable();

    /* set our styles to be the same as vanilla-curses-lynx */
    if (HTList_isEmpty(cur)) {
	initialise_default_stylesheet();
    } else {
	while ((name = HTList_nextObject(cur)) != NULL) {
	    CTRACE2(TRACE_STYLE, (tfp, "LSS:%s\n", name ? name : "!?! empty !?!"));
	    if (name != NULL)
		parse_style(name);
	}
    }

#define dft_style(a,b) if (a == NOSTYLE) a = b

    dft_style(s_prompt_edit,		s_normal);
    dft_style(s_prompt_edit_arr,	s_prompt_edit);
    dft_style(s_prompt_edit_pad,	s_prompt_edit);
    dft_style(s_prompt_sel,		s_prompt_edit);
    dft_style(s_aedit,			s_alink);
    dft_style(s_aedit_arr,		s_aedit);
    dft_style(s_aedit_pad,		s_aedit);
    dft_style(s_curedit,		s_aedit);
    dft_style(s_aedit_sel,		s_aedit);
    dft_style(s_menu_bg,		s_normal);
    dft_style(s_menu_entry,		s_menu_bg);
    dft_style(s_menu_frame,		s_menu_bg);
    dft_style(s_menu_number,		s_menu_bg);
    dft_style(s_menu_active,		s_alink);
}


/* Add a STYLE: option line to our list.  Process "default:" early
   for it to have the same semantic as other lines: works at any place
   of the style file, the first line overrides the later ones. */
PRIVATE void HStyle_addStyle ARGS1(char*, buffer)
{
    char *name = NULL;

    CTRACE((tfp, "HStyle_addStyle(%s)\n", buffer));
    StrAllocCopy(name, buffer);
    if (lss_styles == NULL)
	lss_styles = HTList_new();
    strtolower(name);
    if (!strncasecomp(name, "default:", 8)) /* default fg/bg */
    {
	CTRACE2(TRACE_STYLE, (tfp, "READCSS.default%s:%s\n",
		 (default_color_reset ? ".ignore" : ""),
		 name ? name : "!?! empty !?!"));
	if (!default_color_reset)
	    parse_style(name);
	return;				/* do not need to process it again */
    }
    CTRACE2(TRACE_STYLE, (tfp, "READCSS:%s\n", name ? name : "!?! empty !?!"));
    HTList_addObject (lss_styles, name);
}

PUBLIC void style_deleteStyleList NOARGS
{
    char *name;
    while ((name = HTList_removeLastObject(lss_styles)) != NULL)
	FREE(name);
    HTList_delete (lss_styles);
    lss_styles = NULL;
}

PRIVATE int style_readFromFileREC ARGS2(
    char *,	lss_filename,
    char *,	parent_filename)
{
    FILE *fh;
    char *buffer = NULL;
    int len;

    CTRACE2(TRACE_STYLE, (tfp, "CSS:Reading styles from file: %s\n", lss_filename ? lss_filename : "?!? empty ?!?"));
    if (isEmpty(lss_filename))
	return -1;
    if ((fh = LYOpenCFG(lss_filename, parent_filename, LYNX_LSS_FILE)) == 0) {
	/* this should probably be an alert or something */
	CTRACE2(TRACE_STYLE, (tfp, "CSS:Can't open style file '%s', using defaults\n", lss_filename));
	return -1;
    }

    if (parent_filename == 0) {
	style_initialiseHashTable();
	style_deleteStyleList();
    }

    while (LYSafeGets(&buffer, fh) != NULL) {
	LYTrimTrailing(buffer);
	LYTrimTail(buffer);
	LYTrimHead(buffer);
	if (!strncasecomp(buffer,"include:",8))
	    style_readFromFileREC(buffer+8, lss_filename);
	else if (buffer[0] != '#' && (len = strlen(buffer)) > 0)
	    HStyle_addStyle(buffer);
    }

    LYCloseInput (fh);
    if ((parent_filename == 0) && LYCursesON)
	parse_userstyles();
    return 0;
}

PUBLIC int style_readFromFile ARGS1(char*, filename)
{
    return style_readFromFileREC(filename, (char *)0);
}

/* Used in HTStructured methods: - kw */

PUBLIC void TrimColorClass ARGS3(
    CONST char *,	tagname,
    char *,		styleclassname,
    int *,		phcode)
{
    char *end, *start=NULL, *lookfrom;
    char tmp[64];

    sprintf(tmp, ";%.*s", (int) sizeof(tmp) - 3, tagname);
    strtolower(tmp);

    if ((lookfrom = styleclassname) != 0) {
	do {
	    end = start;
	    start = strstr(lookfrom, tmp);
	    if (start)
		lookfrom = start + 1;
	}
	while (start);
	/* trim the last matching element off the end
	** - should match classes here as well (rp)
	*/
	if (end)
	    *end='\0';
    }
    *phcode = hash_code(lookfrom && *lookfrom ? lookfrom : &tmp[1]);
}

/* This function is designed as faster analog to TrimColorClass.
   It assumes that tag_name is present in stylename! -HV
*/
PUBLIC void FastTrimColorClass ARGS5 (
	    CONST char*,	 tag_name,
	    int,		 name_len,
	    char*,		 stylename,
	    char**,		 pstylename_end,/*will be modified*/
	    int*,		 phcode)	/*will be modified*/
{
    char* tag_start = *pstylename_end;
    BOOLEAN found = FALSE;

    CTRACE2(TRACE_STYLE,
	    (tfp, "STYLE.fast-trim: [%s] from [%s]: ",
		  tag_name, stylename));
    while (tag_start >= stylename)
    {
	for (; (tag_start >= stylename) && (*tag_start != ';') ; --tag_start)
	    ;
	if ( !strncasecomp(tag_start+1, tag_name, name_len) ) {
	    found = TRUE;
	    break;
	}
	--tag_start;
    }
    if (found) {
	*tag_start = '\0';
	*pstylename_end = tag_start;
    }
    CTRACE2(TRACE_STYLE, (tfp, found ? "success.\n" : "failed.\n"));
    *phcode = hash_code(tag_start+1);
}

 /* This is called each time lss styles are read. It will fill
    each elt of 'cached_tag_styles' -HV
 */
PUBLIC void cache_tag_styles NOARGS
{
    char buf[200];
    int i;

    for (i = 0; i < HTML_ELEMENTS; ++i)
    {
	LYstrncpy(buf, HTML_dtd.tags[i].name, sizeof(buf)-1);
	LYLowerCase(buf);
	cached_tag_styles[i] = hash_code(buf);
    }
}

#endif /* USE_COLOR_STYLE */
