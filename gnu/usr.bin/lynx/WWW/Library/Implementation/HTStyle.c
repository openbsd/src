/*	Style Implementation for Hypertext			HTStyle.c
**	==================================
**
**	Styles allow the translation between a logical property
**	of a piece of text and its physical representation.
**
**	A StyleSheet is a collection of styles, defining the
**	translation necessary to
**	represent a document.  It is a linked list of styles.
*/

#include <HTUtils.h>
#include <HTStyle.h>

#include <LYLeaks.h>

/*	Create a new style
*/
PUBLIC HTStyle* HTStyleNew NOARGS
{
    HTStyle * self = (HTStyle *)malloc(sizeof(*self));
    if (self == NULL)
	outofmem(__FILE__, "HTStyleNew");
    memset((void *)self, 0, sizeof(*self));
    return self;
}

/*	Create a new style with a name
*/
PUBLIC HTStyle* HTStyleNewNamed ARGS1 (CONST char *,name)
{
    HTStyle * self = HTStyleNew();
    StrAllocCopy(self->name, name);
    return self;
}


/*	Free a style
*/
PUBLIC HTStyle * HTStyleFree ARGS1 (HTStyle *,self)
{
    FREE(self->name);
    FREE(self->SGMLTag);
    FREE(self);
    return NULL;
}


#ifdef SUPPRESS  /* Only on the NeXT */
/*	Read a style from a stream	(without its name)
**	--------------------------
**
**	Reads a style with paragraph information from a stream.
**	The style name is not read or written by these routines.
*/
#define NONE_STRING "(None)"
#define HTStream NXStream

HTStyle * HTStyleRead (HTStyle * style, HTStream * stream)
{
    char myTag[STYLE_NAME_LENGTH];
    char fontName[STYLE_NAME_LENGTH];
    NXTextStyle *p;
    int tab;
    int gotpara;		/* flag: have we got a paragraph definition? */

    NXScanf(stream, "%s%s%f%d",
	myTag,
	fontName,
	&style->fontSize,
	&gotpara);
    if (gotpara) {
	if (!style->paragraph) {
	    style->paragraph = malloc(sizeof(*(style->paragraph)));
	    if (!style->paragraph)
		outofmem(__FILE__, "HTStyleRead");
	    style->paragraph->tabs = 0;
	}
	p = style->paragraph;
	NXScanf(stream, "%f%f%f%f%hd%f%f%hd",
	    &p->indent1st,
	    &p->indent2nd,
	    &p->lineHt,
	    &p->descentLine,
	    &p->alignment,
	    &style->spaceBefore,
	    &style->spaceAfter,
	    &p->numTabs);
	FREE(p->tabs);
	p->tabs = malloc(p->numTabs * sizeof(p->tabs[0]));
	if (!p->tabs)
	    outofmem(__FILE__, "HTStyleRead");
	for (tab=0; tab < p->numTabs; tab++) {
	    NXScanf(stream, "%hd%f",
		    &p->tabs[tab].kind,
		    &p->tabs[tab].x);
	}
    } else { /* No paragraph */
	FREE(style->paragraph);
    } /* if no paragraph */
    StrAllocCopy(style->SGMLTag, myTag);
    if (strcmp(fontName, NONE_STRING)==0)
	style->font = 0;
    else
	style->font = [Font newFont:fontName size:style->fontSize];
    return NULL;
}


/*	Write a style to a stream in a compatible way
*/
HTStyle * HTStyleWrite (HTStyle * style, NXStream * stream)
{
    int tab;
    NXTextStyle *p = style->paragraph;
    NXPrintf(stream, "%s %s %f %d\n",
	style->SGMLTag,
	style->font ? [style->font name] : NONE_STRING,
	style->fontSize,
	p!=0);

    if (p) {
	NXPrintf(stream, "\t%f %f %f %f %d %f %f\t%d\n",
	    p->indent1st,
	    p->indent2nd,
	    p->lineHt,
	    p->descentLine,
	    p->alignment,
	    style->spaceBefore,
	    style->spaceAfter,
	    p->numTabs);

	for (tab=0; tab < p->numTabs; tab++)
	    NXPrintf(stream, "\t%d %f\n",
		    p->tabs[tab].kind,
		    p->tabs[tab].x);
	}
    return style;
}


/*	Write a style to stdout for diagnostics
*/
HTStyle * HTStyleDump (HTStyle * style)
{
    int tab;
    NXTextStyle *p = style->paragraph;
    printf(STYLE_DUMP_FONT,
	style,
	style->name,
	style->SGMLTag,
	[style->font name],
	style->fontSize);
    if (p) {
	printf(STYLE_DUMP_IDENT,
	    p->indent1st,
	    p->indent2nd,
	    p->lineHt,
	    p->descentLine);
	printf(STYLE_DUMP_ALIGN,
	    p->alignment,
	    p->numTabs,
	    style->spaceBefore,
	    style->spaceAfter);

	for (tab=0; tab < p->numTabs; tab++) {
	    printf(STYLE_DUMP_TAB,
		    p->tabs[tab].kind,
		    p->tabs[tab].x);
	}
	printf("\n");
    } /* if paragraph */
    return style;
}
#endif /* SUPPRESS */


/*			StyleSheet Functions
**			====================
*/

/*	Searching for styles:
*/
HTStyle * HTStyleNamed ARGS2 (HTStyleSheet *,self, CONST char *,name)
{
    HTStyle * scan;
    for (scan=self->styles; scan; scan=scan->next)
	if (0==strcmp(scan->name, name)) return scan;
    CTRACE((tfp, "StyleSheet: No style named `%s'\n", name));
    return NULL;
}

#ifdef NEXT_SUPRESS		/* Not in general common code */

HTStyle * HTStyleMatching (HTStyleSheet * self, HTStyle *style)
{
    HTStyle * scan;
    for (scan=self->styles; scan; scan=scan->next)
	if (scan->paragraph == para) return scan;
    return NULL;
}

/*	Find the style which best fits a given run
**	------------------------------------------
**
**	This heuristic is used for guessing the style for a run of
**	text which has been pasted in.  In order, we try:
**
**	A style whose paragraph structure is actually used by the run.
**	A style matching in font
**	A style matching in paragraph style exactly
**	A style matching in paragraph to a degree
*/

HTStyle * HTStyleForRun (HTStyleSheet *self, NXRun *run)
{
    HTStyle * scan;
    HTStyle * best = 0;
    int bestMatch = 0;
    NXTextStyle * rp = run->paraStyle;
    for (scan=self->styles; scan; scan=scan->next)
	if (scan->paragraph == run->paraStyle) return scan;	/* Exact */

    for (scan=self->styles; scan; scan=scan->next){
	NXTextStyle * sp = scan->paragraph;
	if (sp) {
	    int match = 0;
	    if (sp->indent1st ==	rp->indent1st)	match = match+1;
	    if (sp->indent2nd ==	rp->indent2nd)	match = match+2;
	    if (sp->lineHt ==		rp->lineHt)	match = match+1;
	    if (sp->numTabs ==		rp->numTabs)	match = match+1;
	    if (sp->alignment ==	rp->alignment)	match = match+3;
	    if (scan->font ==		run->font)	match = match+10;
	    if (match>bestMatch) {
		    best=scan;
		    bestMatch=match;
	    }
	}
    }
    CTRACE((tfp, "HTStyleForRun: Best match for style is %d out of 18\n",
		 bestMatch));
    return best;
}
#endif /* NEXT_SUPRESS */


/*	Add a style to a sheet
**	----------------------
*/
HTStyleSheet * HTStyleSheetAddStyle ARGS2
  (HTStyleSheet *,self, HTStyle *,style)
{
    style->next = 0;		/* The style will go on the end */
    if (!self->styles) {
	self->styles = style;
    } else {
	HTStyle * scan;
	for(scan=self->styles; scan->next; scan=scan->next); /* Find end */
	scan->next=style;
    }
    return self;
}


/*	Remove the given object from a style sheet if it exists
*/
HTStyleSheet * HTStyleSheetRemoveStyle ARGS2
  (HTStyleSheet *,self, HTStyle *,style)
{
    if (self->styles == style) {
	self->styles = style->next;
	return self;
    } else {
	HTStyle * scan;
	for(scan = self->styles; scan; scan = scan->next) {
	    if (scan->next == style) {
		scan->next = style->next;
		return self;
	    }
	}
    }
    return NULL;
}

/*	Create new style sheet
*/

HTStyleSheet * HTStyleSheetNew NOARGS
{
    HTStyleSheet * self = (HTStyleSheet *)malloc(sizeof(*self));
    if (self == NULL)
	outofmem(__FILE__, "HTStyleSheetNew");

    memset((void*)self, 0, sizeof(*self));	/* ANSI */
/* Harbison c ref man says (char*)self
   but k&r ansii and abc books and Think_C say (void*) */

/*    bzero(self, sizeof(*self)); */		/* BSD */
    return self;
}


/*	Free off a style sheet pointer
*/
HTStyleSheet * HTStyleSheetFree ARGS1 (HTStyleSheet *,self)
{
    HTStyle * style;
    while((style=self->styles)!=0) {
	self->styles = style->next;
	HTStyleFree(style);
    }
    FREE(self);
    return NULL;
}


/*	Read a stylesheet from a typed stream
**	-------------------------------------
**
**	Reads a style sheet from a stream.  If new styles have the same names
**	as existing styles, they replace the old ones without changing the ids.
*/

#ifdef NEXT_SUPRESS  /* Only on the NeXT */
HTStyleSheet * HTStyleSheetRead(HTStyleSheet * self, NXStream * stream)
{
    int numStyles;
    int i;
    HTStyle * style;
    char styleName[80];
    NXScanf(stream, " %d ", &numStyles);
    CTRACE((tfp, "Stylesheet: Reading %d styles\n", numStyles));
    for (i=0; i<numStyles; i++) {
	NXScanf(stream, "%s", styleName);
	style = HTStyleNamed(self, styleName);
	if (!style) {
	    style = HTStyleNewNamed(styleName);
	    (void) HTStyleSheetAddStyle(self, style);
	}
	(void) HTStyleRead(style, stream);
	if (TRACE) HTStyleDump(style);
    }
    return self;
}

/*	Write a stylesheet to a typed stream
**	------------------------------------
**
**	Writes a style sheet to a stream.
*/

HTStyleSheet * HTStyleSheetWrite(HTStyleSheet * self, NXStream * stream)
{
    int numStyles = 0;
    HTStyle * style;

    for(style=self->styles; style; style=style->next) numStyles++;
    NXPrintf(stream, "%d\n", numStyles);

    CTRACE((tfp, "StyleSheet: Writing %d styles\n", numStyles));
    for (style=self->styles; style; style=style->next) {
	NXPrintf(stream, "%s ", style->name);
	(void) HTStyleWrite(style, stream);
    }
    return self;
}
#endif /* NEXT_SUPRESS */
