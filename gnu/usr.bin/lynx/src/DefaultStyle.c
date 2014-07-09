/*
 * $LynxId: DefaultStyle.c,v 1.20 2009/11/27 13:04:27 tom Exp $
 *
 *	A real style sheet for the Character Grid browser
 *
 *	The dimensions are all in characters!
 */

#include <HTUtils.h>
#include <HTFont.h>
#include <HTStyle.h>

#include <LYGlobalDefs.h>
#include <LYLeaks.h>

/*	Tab arrays:
*/
static const HTTabStop tabs_8[] =
{
    {0, 8},
    {0, 16},
    {0, 24},
    {0, 32},
    {0, 40},
    {0, 48},
    {0, 56},
    {0, 64},
    {0, 72},
    {0, 80},
    {0, 88},
    {0, 96},
    {0, 104},
    {0, 112},
    {0, 120},
    {0, 128},
    {0, 136},
    {0, 144},
    {0, 152},
    {0, 160},
    {0, 168},
    {0, 176},
    {0, 0}			/* Terminate */
};

/* Template:
 *	link to next, name, name id (enum), tag,
 *	font, size, colour, superscript, anchor id,
 *	indents: 1st, left, right, alignment	lineheight, descent,	tabs,
 *	word wrap, free format, space: before, after, flags.
 */

static HTStyle HTStyleNormal =
HTStyleInit(
	       0, Normal, "P",
	       HT_FONT, 1, HT_BLACK, 0, 0,
	       3, 3, 6, HT_LEFT, 1, 0, tabs_8,
	       YES, YES, 1, 0, 0);

static HTStyle HTStyleDivCenter =
HTStyleInit(
	       &HTStyleNormal, DivCenter, "DCENTER",
	       HT_FONT, 1, HT_BLACK, 0, 0,
	       3, 3, 6, HT_CENTER, 1, 0, tabs_8,
	       YES, YES, 1, 0, 0);

static HTStyle HTStyleDivLeft =
HTStyleInit(
	       &HTStyleDivCenter, DivLeft, "DLEFT",
	       HT_FONT, 1, HT_BLACK, 0, 0,
	       3, 3, 6, HT_LEFT, 1, 0, tabs_8,
	       YES, YES, 1, 0, 0);

static HTStyle HTStyleDivRight =
HTStyleInit(
	       &HTStyleDivLeft, DivRight, "DRIGHT",
	       HT_FONT, 1, HT_BLACK, 0, 0,
	       3, 3, 6, HT_RIGHT, 1, 0, tabs_8,
	       YES, YES, 1, 0, 0);

static HTStyle HTStyleBanner =
HTStyleInit(
	       &HTStyleDivRight, Banner, "BANNER",
	       HT_FONT, 1, HT_BLACK, 0, 0,
	       3, 3, 6, HT_LEFT, 1, 0, tabs_8,
	       YES, YES, 1, 0, 0);

static HTStyle HTStyleBlockquote =
HTStyleInit(
	       &HTStyleBanner, Blockquote, "BLOCKQUOTE",
	       HT_FONT, 1, HT_BLACK, 0, 0,
	       5, 5, 7, HT_LEFT, 1, 0, tabs_8,
	       YES, YES, 1, 0, 0);

static HTStyle HTStyleBq =
HTStyleInit(			/* HTML 3.0 BLOCKQUOTE - FM */
	       &HTStyleBlockquote, Bq, "BQ",
	       HT_FONT, 1, HT_BLACK, 0, 0,
	       5, 5, 7, HT_LEFT, 1, 0, tabs_8,
	       YES, YES, 1, 0, 0);

static HTStyle HTStyleFootnote =
HTStyleInit(			/* HTML 3.0 FN - FM */
	       &HTStyleBq, Footnote, "FN",
	       HT_FONT, 1, HT_BLACK, 0, 0,
	       5, 5, 7, HT_LEFT, 1, 0, tabs_8,
	       YES, YES, 1, 0, 0);

static HTStyle HTStyleList =
HTStyleInit(
	       &HTStyleFootnote, List, "UL",
	       HT_FONT, 1, HT_BLACK, 0, 0,
	       3, 7, 6, HT_LEFT, 1, 0, 0,
	       YES, YES, 0, 0, 0);

static HTStyle HTStyleList1 =
HTStyleInit(
	       &HTStyleList, List1, "UL",
	       HT_FONT, 1, HT_BLACK, 0, 0,
	       8, 12, 6, HT_LEFT, 1, 0, 0,
	       YES, YES, 0, 0, 0);

static HTStyle HTStyleList2 =
HTStyleInit(
	       &HTStyleList1, List2, "UL",
	       HT_FONT, 1, HT_BLACK, 0, 0,
	       13, 17, 6, HT_LEFT, 1, 0, 0,
	       YES, YES, 0, 0, 0);

static HTStyle HTStyleList3 =
HTStyleInit(
	       &HTStyleList2, List3, "UL",
	       HT_FONT, 1, HT_BLACK, 0, 0,
	       18, 22, 6, HT_LEFT, 1, 0, 0,
	       YES, YES, 0, 0, 0);

static HTStyle HTStyleList4 =
HTStyleInit(
	       &HTStyleList3, List4, "UL",
	       HT_FONT, 1, HT_BLACK, 0, 0,
	       23, 27, 6, HT_LEFT, 1, 0, 0,
	       YES, YES, 0, 0, 0);

static HTStyle HTStyleList5 =
HTStyleInit(
	       &HTStyleList4, List5, "UL",
	       HT_FONT, 1, HT_BLACK, 0, 0,
	       28, 32, 6, HT_LEFT, 1, 0, 0,
	       YES, YES, 0, 0, 0);

static HTStyle HTStyleList6 =
HTStyleInit(
	       &HTStyleList5, List6, "UL",
	       HT_FONT, 1, HT_BLACK, 0, 0,
	       33, 37, 6, HT_LEFT, 1, 0, 0,
	       YES, YES, 0, 0, 0);

static HTStyle HTStyleMenu =
HTStyleInit(
	       &HTStyleList6, Menu, "MENU",
	       HT_FONT, 1, HT_BLACK, 0, 0,
	       3, 7, 6, HT_LEFT, 1, 0, 0,
	       YES, YES, 0, 0, 0
);

static HTStyle HTStyleMenu1 =
HTStyleInit(
	       &HTStyleMenu, Menu1, "MENU",
	       HT_FONT, 1, HT_BLACK, 0, 0,
	       8, 12, 6, HT_LEFT, 1, 0, 0,
	       YES, YES, 0, 0, 0
);

static HTStyle HTStyleMenu2 =
HTStyleInit(
	       &HTStyleMenu1, Menu2, "MENU",
	       HT_FONT, 1, HT_BLACK, 0, 0,
	       13, 17, 6, HT_LEFT, 1, 0, 0,
	       YES, YES, 0, 0, 0
);

static HTStyle HTStyleMenu3 =
HTStyleInit(
	       &HTStyleMenu2, Menu3, "MENU",
	       HT_FONT, 1, HT_BLACK, 0, 0,
	       18, 22, 6, HT_LEFT, 1, 0, 0,
	       YES, YES, 0, 0, 0
);

static HTStyle HTStyleMenu4 =
HTStyleInit(
	       &HTStyleMenu3, Menu4, "MENU",
	       HT_FONT, 1, HT_BLACK, 0, 0,
	       23, 27, 6, HT_LEFT, 1, 0, 0,
	       YES, YES, 0, 0, 0
);

static HTStyle HTStyleMenu5 =
HTStyleInit(
	       &HTStyleMenu4, Menu5, "MENU",
	       HT_FONT, 1, HT_BLACK, 0, 0,
	       28, 33, 6, HT_LEFT, 1, 0, 0,
	       YES, YES, 0, 0, 0
);

static HTStyle HTStyleMenu6 =
HTStyleInit(
	       &HTStyleMenu5, Menu6, "MENU",
	       HT_FONT, 1, HT_BLACK, 0, 0,
	       33, 38, 6, HT_LEFT, 1, 0, 0,
	       YES, YES, 0, 0, 0
);

static HTStyle HTStyleGlossary =
HTStyleInit(
	       &HTStyleMenu6, Glossary, "DL",
	       HT_FONT, 1, HT_BLACK, 0, 0,
	       3, 10, 6, HT_LEFT, 1, 0, 0,
	       YES, YES, 1, 1, 0
);

static HTStyle HTStyleGlossary1 =
HTStyleInit(
	       &HTStyleGlossary, Glossary1, "DL",
	       HT_FONT, 1, HT_BLACK, 0, 0,
	       8, 16, 6, HT_LEFT, 1, 0, 0,
	       YES, YES, 1, 1, 0
);

static HTStyle HTStyleGlossary2 =
HTStyleInit(
	       &HTStyleGlossary1, Glossary2, "DL",
	       HT_FONT, 1, HT_BLACK, 0, 0,
	       14, 22, 6, HT_LEFT, 1, 0, 0,
	       YES, YES, 1, 1, 0
);

static HTStyle HTStyleGlossary3 =
HTStyleInit(
	       &HTStyleGlossary2, Glossary3, "DL",
	       HT_FONT, 1, HT_BLACK, 0, 0,
	       20, 28, 6, HT_LEFT, 1, 0, 0,
	       YES, YES, 1, 1, 0
);

static HTStyle HTStyleGlossary4 =
HTStyleInit(
	       &HTStyleGlossary3, Glossary4, "DL",
	       HT_FONT, 1, HT_BLACK, 0, 0,
	       26, 34, 6, HT_LEFT, 1, 0, 0,
	       YES, YES, 1, 1, 0
);

static HTStyle HTStyleGlossary5 =
HTStyleInit(
	       &HTStyleGlossary4, Glossary5, "DL",
	       HT_FONT, 1, HT_BLACK, 0, 0,
	       32, 40, 6, HT_LEFT, 1, 0, 0,
	       YES, YES, 1, 1, 0
);

static HTStyle HTStyleGlossary6 =
HTStyleInit(
	       &HTStyleGlossary5, Glossary6, "DL",
	       HT_FONT, 1, HT_BLACK, 0, 0,
	       38, 46, 6, HT_LEFT, 1, 0, 0,
	       YES, YES, 1, 1, 0
);

static HTStyle HTStyleGlossaryCompact =
HTStyleInit(
	       &HTStyleGlossary6, GlossaryCompact, "DLC",
	       HT_FONT, 1, HT_BLACK, 0, 0,
	       3, 10, 6, HT_LEFT, 1, 0, 0,
	       YES, YES, 0, 0, 0
);

static HTStyle HTStyleGlossaryCompact1 =
HTStyleInit(
	       &HTStyleGlossaryCompact,
	       GlossaryCompact1, "DLC",
	       HT_FONT, 1, HT_BLACK, 0, 0,
	       8, 15, 6, HT_LEFT, 1, 0, 0,
	       YES, YES, 0, 0, 0
);

static HTStyle HTStyleGlossaryCompact2 =
HTStyleInit(
	       &HTStyleGlossaryCompact1,
	       GlossaryCompact2, "DLC",
	       HT_FONT, 1, HT_BLACK, 0, 0,
	       13, 20, 6, HT_LEFT, 1, 0, 0,
	       YES, YES, 0, 0, 0
);

static HTStyle HTStyleGlossaryCompact3 =
HTStyleInit(
	       &HTStyleGlossaryCompact2,
	       GlossaryCompact3, "DLC",
	       HT_FONT, 1, HT_BLACK, 0, 0,
	       18, 25, 6, HT_LEFT, 1, 0, 0,
	       YES, YES, 0, 0, 0
);

static HTStyle HTStyleGlossaryCompact4 =
HTStyleInit(
	       &HTStyleGlossaryCompact3,
	       GlossaryCompact4, "DLC",
	       HT_FONT, 1, HT_BLACK, 0, 0,
	       23, 30, 6, HT_LEFT, 1, 0, 0,
	       YES, YES, 0, 0, 0
);

static HTStyle HTStyleGlossaryCompact5 =
HTStyleInit(
	       &HTStyleGlossaryCompact4,
	       GlossaryCompact5, "DLC",
	       HT_FONT, 1, HT_BLACK, 0, 0,
	       28, 35, 6, HT_LEFT, 1, 0, 0,
	       YES, YES, 0, 0, 0
);

static HTStyle HTStyleGlossaryCompact6 =
HTStyleInit(
	       &HTStyleGlossaryCompact5,
	       GlossaryCompact6, "DLC",
	       HT_FONT, 1, HT_BLACK, 0, 0,
	       33, 40, 6, HT_LEFT, 1, 0, 0,
	       YES, YES, 0, 0, 0
);

static HTStyle HTStyleExample =
HTStyleInit(
	       &HTStyleGlossaryCompact6,
	       Example, "XMP",
	       HT_FONT, 1, HT_BLACK, 0, 0,
	       0, 0, 0, HT_LEFT, 1, 0, tabs_8,
	       NO, NO, 0, 0, 0
);

static HTStyle HTStylePreformatted =
HTStyleInit(
	       &HTStyleExample,
	       Preformatted, "PRE",
	       HT_FONT, 1, HT_BLACK, 0, 0,
	       0, 0, 0, HT_LEFT, 1, 0, tabs_8,
	       NO, NO, 0, 0, 0
);

static HTStyle HTStyleListing =
HTStyleInit(
	       &HTStylePreformatted, Listing, "LISTING",
	       HT_FONT, 1, HT_BLACK, 0, 0,
	       0, 0, 0, HT_LEFT, 1, 0, tabs_8,
	       NO, NO, 0, 0, 0);

static HTStyle HTStyleAddress =
HTStyleInit(
	       &HTStyleListing, Address, "ADDRESS",
	       HT_FONT, 1, HT_BLACK, 0, 0,
	       4, 4, 7, HT_LEFT, 1, 0, tabs_8,
	       YES, YES, 2, 0, 0);

static HTStyle HTStyleNote =
HTStyleInit(			/* HTML 3.0 NOTE - FM */
	       &HTStyleAddress, Note, "NOTE",
	       HT_FONT, 1, HT_BLACK, 0, 0,
	       5, 5, 7, HT_LEFT, 1, 0, tabs_8,
	       YES, YES, 1, 0, 0);

static HTStyle HTStyleHeading1 =
HTStyleInit(
	       &HTStyleNote, Heading1, "H1",
	       HT_FONT + HT_BOLD, 1, HT_BLACK, 0, 0,
	       0, 0, 0, HT_CENTER, 1, 0, 0,
	       YES, YES, 1, 1, 0);

static HTStyle HTStyleHeading2 =
HTStyleInit(
	       &HTStyleHeading1, Heading2, "H2",
	       HT_FONT + HT_BOLD, 1, HT_BLACK, 0, 0,
	       0, 0, 0, HT_LEFT, 1, 0, 0,
	       YES, YES, 1, 1, 0);

static HTStyle HTStyleHeading3 =
HTStyleInit(
	       &HTStyleHeading2, Heading3, "H3",
	       HT_FONT + HT_BOLD, 1, HT_BLACK, 0, 0,
	       2, 2, 0, HT_LEFT, 1, 0, 0,
	       YES, YES, 1, 0, 0);

static HTStyle HTStyleHeading4 =
HTStyleInit(
	       &HTStyleHeading3, Heading4, "H4",
	       HT_FONT + HT_BOLD, 1, HT_BLACK, 0, 0,
	       4, 4, 0, HT_LEFT, 1, 0, 0,
	       YES, YES, 1, 0, 0);

static HTStyle HTStyleHeading5 =
HTStyleInit(
	       &HTStyleHeading4, Heading5, "H5",
	       HT_FONT + HT_BOLD, 1, HT_BLACK, 0, 0,
	       6, 6, 0, HT_LEFT, 1, 0, 0,
	       YES, YES, 1, 0, 0);

static HTStyle HTStyleHeading6 =
HTStyleInit(
	       &HTStyleHeading5, Heading6, "H6",
	       HT_FONT + HT_BOLD, 1, HT_BLACK, 0, 0,
	       8, 8, 0, HT_LEFT, 1, 0, 0,
	       YES, YES, 1, 0, 0);

static HTStyle HTStyleHeadingCenter =
HTStyleInit(
	       &HTStyleHeading6, HeadingCenter, "HCENTER",
	       HT_FONT + HT_BOLD, 1, HT_BLACK, 0, 0,
	       0, 0, 3, HT_CENTER, 1, 0, tabs_8,
	       YES, YES, 1, 0, 0);

static HTStyle HTStyleHeadingLeft =
HTStyleInit(
	       &HTStyleHeadingCenter, HeadingLeft, "HLEFT",
	       HT_FONT + HT_BOLD, 1, HT_BLACK, 0, 0,
	       0, 0, 3, HT_LEFT, 1, 0, tabs_8,
	       YES, YES, 1, 0, 0);

static HTStyle HTStyleHeadingRight =
HTStyleInit(
	       &HTStyleHeadingLeft, HeadingRight, "HRIGHT",
	       HT_FONT + HT_BOLD, 1, HT_BLACK, 0, 0,
	       0, 0, 3, HT_RIGHT, 1, 0, tabs_8,
	       YES, YES, 1, 0, 0);

/* Style sheet points to the last in the list:
*/
static HTStyleSheet sheet =
{"default.style",
 &HTStyleHeadingRight};		/* sheet */

static HTStyle *st_array[ST_HeadingRight + 1] =
{NULL};

static HTStyleSheet *result = NULL;

#ifdef LY_FIND_LEAKS
static void FreeDefaultStyle(void)
{
    HTStyle *style;

    while ((style = result->styles) != 0) {
	result->styles = style->next;
	FREE(style);
    }
    FREE(result);
}
#endif /* LY_FIND_LEAKS */

HTStyleSheet *DefaultStyle(HTStyle ***result_array)
{
    HTStyle *p, *q;

    /*
     * The first time we're called, allocate a copy of the 'sheet' linked
     * list.  Thereafter, simply copy the data from 'sheet' into our copy
     * (preserving the copy's linked-list pointers).  We do this to reset the
     * parameters of a style that might be altered while processing a page.
     */
    if (result == 0) {		/* allocate & copy */
	result = HTStyleSheetNew();
	*result = sheet;
	result->styles = 0;
#ifdef LY_FIND_LEAKS
	atexit(FreeDefaultStyle);
#endif
	for (p = sheet.styles; p != 0; p = p->next) {
	    q = HTStyleNew();
	    *q = *p;
	    if (no_margins) {
		q->indent1st = 0;
		q->leftIndent = 0;
		q->rightIndent = 0;
	    }
	    st_array[q->id] = q;
	    q->next = result->styles;
	    result->styles = q;
	}
    } else {			/* recopy the data */
	for (q = result->styles, p = sheet.styles;
	     p != 0 && q != 0;
	     p = p->next, q = q->next) {
	    HTStyle *r = q->next;

	    *q = *p;
	    if (no_margins) {
		q->indent1st = 0;
		q->leftIndent = 0;
		q->rightIndent = 0;
	    }
	    st_array[q->id] = q;
	    q->next = r;
	}
    }
    *result_array = st_array;
    return result;
}
