/* character level styles for Lynx
 * (c) 1996 Rob Partington -- donated to the Lyncei (if they want it :-)
 * $Id: LYStyle.c,v 1.5 2009/05/31 09:16:52 avsm Exp $
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

static void style_initialiseHashTable(void);

/* because curses isn't started when we parse the config file, we
 * need to remember the STYLE: lines we encounter and parse them
 * after curses has started
 */
static HTList *lss_styles = NULL;

/* stack of attributes during page rendering */
int last_styles[MAX_LAST_STYLES] =
{0};
int last_colorattr_ptr = 0;

bucket hashStyles[CSHASHSIZE];
bucket special_bucket =
{
    "<special>",		/* in order something to be in trace. */
    0, 0, 0, 0, NULL
};
bucket nostyle_bucket =
{
    "<NOSTYLE>",		/* in order something to be in trace. */
    0, 0, 0, 0, NULL
};

int cached_tag_styles[HTML_ELEMENTS];
int current_tag_style;
BOOL force_current_tag_style = FALSE;
char *forced_classname;
BOOL force_classname;

/* Remember the hash codes for common elements */
int s_a = NOSTYLE;
int s_aedit = NOSTYLE;
int s_aedit_arr = NOSTYLE;
int s_aedit_pad = NOSTYLE;
int s_aedit_sel = NOSTYLE;
int s_alert = NOSTYLE;
int s_alink = NOSTYLE;
int s_curedit = NOSTYLE;
int s_forw_backw = NOSTYLE;
int s_hot_paste = NOSTYLE;
int s_menu_active = NOSTYLE;
int s_menu_bg = NOSTYLE;
int s_menu_entry = NOSTYLE;
int s_menu_frame = NOSTYLE;
int s_menu_number = NOSTYLE;
int s_menu_sb = NOSTYLE;
int s_normal = NOSTYLE;
int s_prompt_edit = NOSTYLE;
int s_prompt_edit_arr = NOSTYLE;
int s_prompt_edit_pad = NOSTYLE;
int s_prompt_sel = NOSTYLE;
int s_status = NOSTYLE;
int s_title = NOSTYLE;
int s_whereis = NOSTYLE;

#ifdef USE_SCROLLBAR
int s_sb_aa = NOSTYLE;
int s_sb_bar = NOSTYLE;
int s_sb_bg = NOSTYLE;
int s_sb_naa = NOSTYLE;
#endif

/* start somewhere safe */
#define MAX_COLOR 16
static int colorPairs = 0;

#ifdef USE_BLINK
#  define MAX_BLINK	2
#  define M_BLINK	A_BLINK
#else
#  define MAX_BLINK	1
#  define M_BLINK	0
#endif

#define MAX_PAIR 255		/* because our_pairs[] type is unsigned-char */
static unsigned char our_pairs[2]
[MAX_BLINK]
[MAX_COLOR + 1]
[MAX_COLOR + 1];

static char *TrimLowercase(char *buffer)
{
    LYRemoveBlanks(buffer);
    strtolower(buffer);
    return buffer;
}

/*
 * Parse a string containing a combination of video attributes and color.
 */
static void parse_either(char *attrs,
			 int dft_color,
			 int *monop,
			 int *colorp)
{
    int value;

    while (*attrs != '\0') {
	char *next = strchr(attrs, '+');
	char save = (next != NULL) ? *next : '\0';

	if (next == NULL)
	    next = attrs + strlen(attrs);

	if (save != 0)		/* attrs might be a constant string */
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
static void parse_attributes(char *mono,
			     char *fg,
			     char *bg,
			     int style,
			     char *element)
{
    int mA = A_NORMAL;
    int fA = default_fg;
    int bA = default_bg;
    int cA = A_NORMAL;
    int newstyle = hash_code(element);

    CTRACE2(TRACE_STYLE, (tfp, "CSS(PA):style d=%d / h=%d, e=%s\n",
			  style, newstyle, element));

    parse_either(mono, ERR_COLOR, &mA, (int *) 0);
    parse_either(bg, default_bg, &cA, &bA);
    parse_either(fg, default_fg, &cA, &fA);

    if (style == -1) {		/* default */
	CTRACE2(TRACE_STYLE, (tfp, "CSS(DEF):default_fg=%d, default_bg=%d\n",
			      fA, bA));
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
    if (lynx_has_color && colorPairs < COLOR_PAIRS - 1 && fA != NO_COLOR) {
	int curPair = 0;
	int iFg = (1 + (fA >= 0 ? fA : 0));
	int iBg = (1 + (bA >= 0 ? bA : 0));
	int iBold = !!(cA & A_BOLD);
	int iBlink = !!(cA & M_BLINK);

	CTRACE2(TRACE_STYLE, (tfp, "parse_attributes %d/%d %d/%d %#x\n",
			      fA, default_fg, bA, default_bg, cA));
	if (fA < MAX_COLOR
	    && bA < MAX_COLOR
#ifdef USE_CURSES_PAIR_0
	    && (cA != A_NORMAL || fA != default_fg || bA != default_bg)
#endif
	    && curPair < MAX_PAIR) {
	    if (our_pairs[iBold][iBlink][iFg][iBg] != 0) {
		curPair = our_pairs[iBold][iBlink][iFg][iBg];
	    } else {
		curPair = ++colorPairs;
		init_pair((short) curPair, (short) fA, (short) bA);
		our_pairs[iBold][iBlink][iFg][iBg] = curPair;
	    }
	}
	CTRACE2(TRACE_STYLE, (tfp, "CSS(CURPAIR):%d\n", curPair));
	if (style < DSTYLE_ELEMENTS)
	    setStyle(style, COLOR_PAIR(curPair) | cA, cA, mA);
	setHashStyle(newstyle, COLOR_PAIR(curPair) | cA, cA, mA, element);
    } else {
	if (lynx_has_color && fA != NO_COLOR) {
	    CTRACE2(TRACE_STYLE,
		    (tfp, "CSS(NC): maximum of %d colorpairs exhausted\n",
		     COLOR_PAIRS - 1));
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
static void parse_style(char *param)
{
    /* *INDENT-OFF* */
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
    /* *INDENT-ON* */

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

    TrimLowercase(buffer);
    if ((tmp = strchr(buffer, ':')) == 0) {
	fprintf(stderr, gettext("\
Syntax Error parsing style in lss file:\n\
[%s]\n\
The line must be of the form:\n\
OBJECT:MONO:COLOR (ie em:bold:brightblue:white)\n\
where OBJECT is one of EM,STRONG,B,I,U,BLINK etc.\n\n"), buffer);
	exit_immediately(EXIT_FAILURE);
    }
    *tmp = '\0';
    element = buffer;

    mono = tmp + 1;
    tmp = strchr(mono, ':');

    if (!tmp) {
	fg = "nocolor";
	bg = "nocolor";
    } else {
	*tmp = '\0';
	fg = tmp + 1;
	tmp = strchr(fg, ':');
	if (!tmp)
	    bg = "default";
	else {
	    *tmp = '\0';
	    bg = tmp + 1;
	}
    }

    CTRACE2(TRACE_STYLE, (tfp, "CSSPARSE:%s => %d %s\n",
			  element, hash_code(element),
			  (hashStyles[hash_code(element)].name ? "used" : "")));

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
	if (!strcasecomp(element, "normal")) {
	    /* added - kw */
	    parse_attributes(mono, fg, bg, DSTYLE_NORMAL, "html");
	    s_normal = hash_code("html");	/* rather bizarre... - kw */

	    LYnormalColor();
	}
    } else {
	/* It must be a HTML element, so look through the list until we find it. */
	int element_number = -1;
	HTTag *t = SGMLFindTag(&HTML_dtd, element);

	if (t && t->name) {
	    element_number = t - HTML_dtd.tags;
	}
	if (element_number >= HTML_A &&
	    element_number < HTML_ELEMENTS) {
	    parse_attributes(mono, fg, bg, element_number + STARTAT, element);
	} else {
	    parse_attributes(mono, fg, bg, DSTYLE_ELEMENTS, element);
	}
    }
    FREE(buffer);
}

static void style_deleteStyleList(void)
{
    LYFreeStringList(lss_styles);
    lss_styles = NULL;
}

static void free_colorstylestuff(void)
{
    style_initialiseHashTable();
    style_deleteStyleList();
    memset(our_pairs, 0, sizeof(our_pairs));
}

/*
 * Initialise the default style sheet to match the vanilla-curses lynx.
 */
static void initialise_default_stylesheet(void)
{
    /* Use the data setup in USE_COLOR_TABLE */
    /* *INDENT-OFF* */
    static const struct {
	int		color;	/* index into lynx_color_pairs[] */
	const char	*type;
    } table2[] = {
	/*
	 * non-color-style colors encode bold/reverse/underline as a 0-7
	 * index like this:
	 *  b,r,u 0
	 *  b,r,U 1
	 *  b,R,u 2
	 *  b,R,U 3
	 *  B,r,u 4
	 *  B,r,U 5
	 *  B,R,u 6
	 *  B,R,U 7
	 */
	{ 0,	"normal" },
	{ 1,	"a" },
	{ 2,	"status" },
	{ 4,	"b" },
	{ 4,	"blink" },
	{ 4,	"cite" },
	{ 4,	"del" },
	{ 4,	"em" },
	{ 4,	"i" },
	{ 4,	"ins" },
	{ 4,	"strike" },
	{ 4,	"strong" },
	{ 4,	"u" },
#if 0
	{ 5,	"a.b" },
	{ 5,	"b.a" },
	{ 5,	"var.a" },
#endif
	{ 6,	"alink" },
	{ 7,	"whereis" },
#if 0
	{ 0,	"h2.link" },
	{ 0,	"link.h2" },
#endif
#ifdef USE_PRETTYSRC
	/* FIXME: HTL_tagspecs_defaults[] has similar info */
	{ 4,	"span.htmlsrc_comment" },
	{ 4,	"span.htmlsrc_tag" },
	{ 4,	"span.htmlsrc_attrib" },
	{ 4,	"span.htmlsrc_attrval" },
	{ 4,	"span.htmlsrc_abracket" },
	{ 4,	"span.htmlsrc_entity" },
	{ 4,	"span.htmlsrc_href" },
	{ 4,	"span.htmlsrc_entire" },
	{ 4,	"span.htmlsrc_badseq" },
	{ 4,	"span.htmlsrc_badtag" },
	{ 4,	"span.htmlsrc_badattr" },
	{ 4,	"span.htmlsrc_sgmlspecial" },
#endif
    };
    /* *INDENT-ON* */

    unsigned n;
    char *normal = LYgetTableString(0);
    char *strong = LYgetTableString(4);

    CTRACE((tfp, "initialise_default_stylesheet\n"));

    /*
     * For debugging this function, create hash codes for all of the tags.
     * That makes it simpler to find the cases that are overlooked in the
     * table.
     */
    for (n = 0; n < (unsigned) HTML_dtd.number_of_tags; ++n) {
	char *name = 0;

	HTSprintf0(&name, "%s:%s", HTML_dtd.tags[n].name, normal);
	parse_style(name);
	FREE(name);
    }

    for (n = 0; n < TABLESIZE(table2); ++n) {
	int code = table2[n].color;
	char *name = 0;
	char *value = 0;

	switch (code) {
	case 0:
	    value = normal;
	    break;
	case 4:
	    value = strong;
	    break;
	default:
	    value = LYgetTableString(code);
	    break;
	}
	HTSprintf0(&name, "%s:%s", table2[n].type, value);
	parse_style(name);
	FREE(name);
	if (value != normal && value != strong && value != 0)
	    free(value);
    }
    FREE(normal);
    FREE(strong);
}

/* Set all the buckets in the hash table to be empty */
static void style_initialiseHashTable(void)
{
    int i;
    static int firsttime = 1;

    for (i = 0; i < CSHASHSIZE; i++) {
	if (firsttime)
	    hashStyles[i].name = NULL;
	else
	    FREE(hashStyles[i].name);
	hashStyles[i].color = 0;
	hashStyles[i].cattr = 0;
	hashStyles[i].mono = 0;
    }
    if (firsttime) {
	firsttime = 0;
#ifdef LY_FIND_LEAKS
	atexit(free_colorstylestuff);
#endif
    }
    s_alink = hash_code("alink");
    s_a = hash_code("a");
    s_status = hash_code("status");
    s_alert = hash_code("alert");
    s_title = hash_code("title");
#ifdef USE_SCROLLBAR
    s_sb_bar = hash_code("scroll.bar");
    s_sb_bg = hash_code("scroll.back");
    s_sb_aa = hash_code("scroll.arrow");
    s_sb_naa = hash_code("scroll.noarrow");
#endif
}

void parse_userstyles(void)
{
    char *name;
    HTList *cur = lss_styles;

    colorPairs = 0;
    style_initialiseHashTable();

    if (HTList_isEmpty(cur)) {
	initialise_default_stylesheet();
    } else {
	while ((name = (char *) HTList_nextObject(cur)) != NULL) {
	    CTRACE2(TRACE_STYLE, (tfp, "LSS:%s\n",
				  (name
				   ? name
				   : "!?! empty !?!")));
	    if (name != NULL)
		parse_style(name);
	}
    }

#define dft_style(a,b) if (a == NOSTYLE) a = b
    /* *INDENT-OFF* */
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
    /* *INDENT-ON* */

}

/* Add a STYLE: option line to our list.  Process "default:" early
 * for it to have the same semantic as other lines: works at any place
 * of the style file, the first line overrides the later ones.
 */
static void HStyle_addStyle(char *buffer)
{
    char *name = NULL;

    CTRACE((tfp, "HStyle_addStyle(%s)\n", buffer));

    StrAllocCopy(name, buffer);
    TrimLowercase(name);

    if (lss_styles == NULL)
	lss_styles = HTList_new();

    if (!strncasecomp(name, "default:", 8)) {
	/* default fg/bg */
	CTRACE2(TRACE_STYLE, (tfp, "READCSS.default%s:%s\n",
			      (default_color_reset ? ".ignore" : ""),
			      name ? name : "!?! empty !?!"));
	if (!default_color_reset)
	    parse_style(name);
	return;			/* do not need to process it again */
    }
    CTRACE2(TRACE_STYLE, (tfp, "READCSS:%s\n", name ? name : "!?! empty !?!"));
    HTList_addObject(lss_styles, name);
}

static int style_readFromFileREC(char *lss_filename,
				 char *parent_filename)
{
    FILE *fh;
    char *buffer = NULL;
    int len;

    CTRACE2(TRACE_STYLE, (tfp, "CSS:Reading styles from file: %s\n",
			  lss_filename ? lss_filename : "?!? empty ?!?"));
    if (isEmpty(lss_filename))
	return -1;
    if ((fh = LYOpenCFG(lss_filename, parent_filename, LYNX_LSS_FILE)) == 0) {
	/* this should probably be an alert or something */
	CTRACE2(TRACE_STYLE, (tfp,
			      "CSS:Can't open style file '%s', using defaults\n", lss_filename));
	return -1;
    }

    if (parent_filename == 0) {
	free_colorstylestuff();
    }

    while (LYSafeGets(&buffer, fh) != NULL) {
	LYTrimTrailing(buffer);
	LYTrimTail(buffer);
	LYTrimHead(buffer);
	if (!strncasecomp(buffer, "include:", 8))
	    style_readFromFileREC(LYSkipBlanks(buffer + 8), lss_filename);
	else if (buffer[0] != '#' && (len = strlen(buffer)) > 0)
	    HStyle_addStyle(buffer);
    }

    LYCloseInput(fh);
    if ((parent_filename == 0) && LYCursesON)
	parse_userstyles();
    return 0;
}

int style_readFromFile(char *filename)
{
    return style_readFromFileREC(filename, (char *) 0);
}

/* Used in HTStructured methods: - kw */

void TrimColorClass(const char *tagname,
		    char *styleclassname,
		    int *phcode)
{
    char *end, *start = NULL, *lookfrom;
    char tmp[64];

    sprintf(tmp, ";%.*s", (int) sizeof(tmp) - 3, tagname);
    TrimLowercase(tmp);

    if ((lookfrom = styleclassname) != 0) {
	do {
	    end = start;
	    start = strstr(lookfrom, tmp);
	    if (start)
		lookfrom = start + 1;
	}
	while (start);
	/* trim the last matching element off the end
	 * - should match classes here as well (rp)
	 */
	if (end)
	    *end = '\0';
    }
    *phcode = hash_code(lookfrom && *lookfrom ? lookfrom : &tmp[1]);
}

/* This function is designed as faster analog to TrimColorClass.
 * It assumes that tag_name is present in stylename! -HV
 */
void FastTrimColorClass(const char *tag_name,
			int name_len,
			char *stylename,
			char **pstylename_end,	/*will be modified */
			int *phcode)	/*will be modified */
{
    char *tag_start = *pstylename_end;
    BOOLEAN found = FALSE;

    CTRACE2(TRACE_STYLE,
	    (tfp, "STYLE.fast-trim: [%s] from [%s]: ",
	     tag_name, stylename));
    while (tag_start >= stylename) {
	for (; (tag_start >= stylename) && (*tag_start != ';'); --tag_start) ;
	if (!strncasecomp(tag_start + 1, tag_name, name_len)) {
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
    *phcode = hash_code(tag_start + 1);
}

 /* This is called each time lss styles are read. It will fill
  * each elt of 'cached_tag_styles' -HV
  */
void cache_tag_styles(void)
{
    char buf[200];
    int i;

    for (i = 0; i < HTML_ELEMENTS; ++i) {
	LYstrncpy(buf, HTML_dtd.tags[i].name, sizeof(buf) - 1);
	LYLowerCase(buf);
	cached_tag_styles[i] = hash_code(buf);
    }
}

#endif /* USE_COLOR_STYLE */
