/* character level styles for Lynx
 * (c) 1996 Rob Partington -- donated to the Lyncei (if they want it :-)
 * $Id: LYStyle.c,v 1.2 2000/03/25 18:17:12 maja Exp $
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

/* stack of attributes during page rendering */
PUBLIC int last_styles[128];
PUBLIC int last_colorattr_ptr=0;

PUBLIC bucket hashStyles[CSHASHSIZE];
PUBLIC bucket special_bucket =
{
 "<special>" /* in order something to be in trace. */
};
PUBLIC bucket nostyle_bucket =
{
 "<NOSTYLE>" /* in order something to be in trace. */
};

PUBLIC int cached_tag_styles[HTML_ELEMENTS];
PUBLIC int current_tag_style;
PUBLIC BOOL force_current_tag_style=FALSE;
PUBLIC char* forced_classname;
PUBLIC BOOL force_classname;

/* definitions for the mono attributes we can use */
static int ncursesMono[7] = {
 A_NORMAL, A_BOLD, A_REVERSE, A_UNDERLINE, A_STANDOUT, A_BLINK, A_DIM
};

/*
 * If these strings don't match the meanings of the above attributes,
 * you'll confuse the hell out of people, so make them the same. - RP
 */
static char *Mono_Strings[7] =
{
 "normal", "bold", "reverse", "underline", "standout", "blink", "dim"
};

/* Remember the hash codes for common elements */
PUBLIC int	s_alink  = NOSTYLE, s_a     = NOSTYLE, s_status = NOSTYLE,
		s_label  = NOSTYLE, s_value = NOSTYLE, s_high   = NOSTYLE,
		s_normal = NOSTYLE, s_alert = NOSTYLE, s_title  = NOSTYLE,
		s_whereis= NOSTYLE;

/* start somewhere safe */
PRIVATE int colorPairs = 0;
PRIVATE int last_fA = COLOR_WHITE, last_bA = COLOR_BLACK;

/* icky parsing of the style options */
PRIVATE void parse_attributes ARGS5(char*,mono,char*,fg,char*,bg,int,style,char*,element)
{
    int i;
    int mA = 0, fA = default_fg, bA = default_bg, cA = A_NORMAL;
    int newstyle = hash_code(element);

    CTRACE(tfp, "CSS(PA):style d=%d / h=%d, e=%s\n", style, newstyle,element);

    for (i = 0; i < (int)TABLESIZE(Mono_Strings); i++)
    {
	if (!strcasecomp(Mono_Strings[i], mono))
	{
	    mA = ncursesMono[i];
	}
    }
    if (!mA) {
	/*
	 *  Not found directly yet, see whether we have a combination
	 *  of several mono attributes separated by '+' - kw
	 */
	char *cp0 = mono;
	char csep = '+';
	char *cp = strchr(mono, csep);
	while (cp) {
	    *cp = '\0';
	    for (i = 0; i < (int)TABLESIZE(Mono_Strings); i++)
	    {
		if (!strcasecomp(Mono_Strings[i], cp0))
		{
		    mA |= ncursesMono[i];
		}
	    }
	    if (!csep)
		break;
	    *cp = csep;
	    cp0 = cp + 1;
	    cp = strchr(cp0, csep);
	    if (!cp) {
		cp = cp0 + strlen(cp0);
		csep = '\0';
	    }
	}
    }
    CTRACE(tfp, "CSS(CP):%d\n", colorPairs);

    fA = check_color(fg, default_fg);
    bA = check_color(bg, default_bg);
    if (fA == NO_COLOR) {
	bA = NO_COLOR;
    } else if (COLORS) {
	if (fA >= COLORS || bA >= COLORS)
	    cA = A_BOLD;
	if (fA >= COLORS)
	    fA %= COLORS;
	if (bA > COLORS)
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
    if (lynx_has_color && colorPairs < COLOR_PAIRS-1 && fA != NO_COLOR)
    {
	if (colorPairs <= 0 || fA != last_fA || bA != last_bA) {
	    colorPairs++;
	    init_pair(colorPairs, fA, bA);
	    last_fA = fA;
	    last_bA = bA;
	}
	if (style < DSTYLE_ELEMENTS)
	    setStyle(style, COLOR_PAIR(colorPairs)|cA, cA, mA);
	setHashStyle(newstyle, COLOR_PAIR(colorPairs)|cA, cA, mA, element);
    }
    else
    {
    /* only mono is set */
	if (style < DSTYLE_ELEMENTS)
	    setStyle(style, -1, -1, mA);
	setHashStyle(newstyle, -1, -1, mA, element);
    }
}

/* parse a style option of the format
 * STYLE:<OBJECT>:FG:BG
 */
PRIVATE void parse_style ARGS1(char*,buffer)
{
    char *tmp = strchr(buffer, ':');
    char *element, *mono, *fg, *bg;

    if(!tmp)
    {
	fprintf (stderr, gettext("\
Syntax Error parsing style in lss file:\n\
[%s]\n\
The line must be of the form:\n\
OBJECT:MONO:COLOR (ie em:bold:brightblue:white)\n\
where OBJECT is one of EM,STRONG,B,I,U,BLINK etc.\n\n"), buffer);
	if (!dump_output_immediately) {
	    exit_immediately(-1);
	}
	exit(1);
    }
    {
	char *i;
	for (i = buffer; *i; i++)
	    *i = tolower(*i);
    }
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

    CTRACE(tfp, "CSSPARSE:%s => %d %s\n",
		element, hash_code(element),
		(hashStyles[hash_code(element)].name ? "used" : ""));

    strtolower(element);

    /*
    * We use some pseudo-elements, so catch these first
    */
    if (!strncasecomp(element, "alink", 5)) /* active link */
    {
	parse_attributes(mono,fg,bg,DSTYLE_ALINK,"alink");
    }
    else if (!strcasecomp(element, "a")) /* normal link */
    {
	parse_attributes(mono,fg,bg, DSTYLE_LINK,"a");
	parse_attributes(mono,fg,bg, HTML_A,"a");
    }
    else if (!strncasecomp(element, "status", 4)) /* status bar */
    {
	parse_attributes(mono,fg,bg, DSTYLE_STATUS,"status");
    }
    else if (!strncasecomp(element, "label", 6)) /* [INLINE]'s */
    {
	parse_attributes(mono,fg,bg,DSTYLE_OPTION,"label");
    }
    else if (!strncasecomp(element, "value", 5)) /* [INLINE]'s */
    {
	parse_attributes(mono,fg,bg,DSTYLE_VALUE,"value");
    }
    else if (!strncasecomp(element, "high", 4)) /* [INLINE]'s */
    {
	parse_attributes(mono,fg,bg,DSTYLE_HIGH,"high");
    }
    else if (!strcasecomp(element, "normal")) /* added - kw */
    {
	parse_attributes(mono,fg,bg,DSTYLE_NORMAL,"html");
	s_normal  = hash_code("html"); /* rather bizarre... - kw */
    }
    /* this may vanish */
    else if (!strncasecomp(element, "candy", 5)) /* [INLINE]'s */
    {
	parse_attributes(mono,fg,bg,DSTYLE_CANDY,"candy");
    }
    /* added for whereis search target - kw */
    else if (!strncasecomp(element, "whereis", 7))
    {
	parse_attributes(mono,fg,bg,DSTYLE_WHEREIS,"whereis");
	s_whereis  = hash_code("whereis");
    }
    /* Ok, it must be a HTML element, so look through the list until we
    * find it
    */
    else
    {
#if !defined(USE_HASH)
	int i;
	for (i = 0; i <HTML_ELEMENTS; i++)
	{
	    if (!strcasecomp (HTML_dtd.tags[i].name, element))
	    {
		CTRACE(tfp, "PARSECSS:applying style <%s,%s,%s> for HTML_%s\n",mono,fg,bg,HTML_dtd.tags[i].name);
			parse_attributes(mono,fg,bg,i+STARTAT,element);
		break;
	    }
	}
#else
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
#endif
    }
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
}

/* Set all the buckets in the hash table to be empty */
PUBLIC void style_initialiseHashTable NOARGS
{
	int i;
	static int firsttime = 1;

	for (i = 0; i <CSHASHSIZE; i++)
	{
	    if (firsttime)
		hashStyles[i].name = NULL;
	    else
		FREE(hashStyles[i].name);
	    hashStyles[i].color = -1;
	    hashStyles[i].cattr = -1;
	    hashStyles[i].mono  = -1;
	}
	if (firsttime) {
	    firsttime = 0;
#ifdef LY_FIND_LEAKS
	    atexit(free_colorstylestuff);
#endif
	}
	s_high   = hash_code("high");
	s_alink  = hash_code("alink");
	s_value  = hash_code("value");
	s_label  = hash_code("label");
	s_a      = hash_code("a");
	s_status = hash_code("status");
	s_alert  = hash_code("alert");
	s_title  = hash_code("title");
}

/* because curses isn't started when we parse the config file, we
 * need to remember the STYLE: lines we encounter and parse them
 * after curses has started
 */
HTList *lss_styles = NULL;

PUBLIC void parse_userstyles NOARGS
{
	char *name;
	HTList *cur = lss_styles;
	colorPairs = 0;
	style_initialiseHashTable();

	/* set our styles to be the same as vanilla-curses-lynx */
	initialise_default_stylesheet();

	while ((name = HTList_nextObject(cur)) != NULL)
	{
		CTRACE(tfp, "LSS:%s\n", name ? name : "!?! empty !?!");
		if (name != NULL)
		    parse_style(name);
	}
}


/* Add a STYLE: option line to our list */
PUBLIC void HStyle_addStyle ARGS1(char*,buffer)
{
	char *name = NULL;
	StrAllocCopy(name, buffer);
	if (lss_styles == NULL)
		lss_styles = HTList_new();
	strtolower(name);
	CTRACE(tfp, "READCSS:%s\n", name ? name : "!?! empty !?!");
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

char* default_stylesheet[] = {
	"a:bold", "em:bold", "strong:bold", "b:bold", "i:bold",
	"alink:reverse", "status:reverse", NULL
};

PUBLIC void style_defaultStyleSheet NOARGS
{
	int i;
	for (i = 0; default_stylesheet[i]; i++)
		HStyle_addStyle(default_stylesheet[i]);
}

PUBLIC int style_readFromFile ARGS1(char*, file)
{
    FILE *fh;
    char *buffer = NULL;
    int len;

    CTRACE(tfp, "CSS:Reading styles from file: %s\n", file ? file : "?!? empty ?!?");
    if (file == NULL || *file == '\0')
	return -1;
    fh = fopen(file, "r");
    if (!fh)
    {
	/* this should probably be an alert or something */
	CTRACE(tfp, "CSS:Can't open style file %s, using defaults\n", file);
	return -1;
    }

    style_initialiseHashTable();
    style_deleteStyleList();

    while (LYSafeGets(&buffer, fh) != NULL)
    {
	LYTrimTrailing(buffer);
	LYTrimTail(buffer);
	LYTrimHead(buffer);
	if (buffer[0] != '#' && (len = strlen(buffer)) > 0)
	    HStyle_addStyle(buffer);
    }
    /* the default styles are added after the user styles in order
    ** that they come before them  <grin>  RP
    */
    /*	style_defaultStyleSheet(); */

    fclose (fh);
    if (LYCursesON)
	parse_userstyles();
    return 0;
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
    CTRACE(tfp, "CSS:%s (trimmed %s)\n",
	   (styleclassname ? styleclassname : "<null>"), tmp);
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
	strcpy(buf, HTML_dtd.tags[i].name);
	LYLowerCase(buf);
	cached_tag_styles[i] =hash_code(buf);
    }
}

#endif /* USE_COLOR_STYLE */
