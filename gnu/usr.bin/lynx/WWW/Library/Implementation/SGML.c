/*			General SGML Parser code		SGML.c
**			========================
**
**	This module implements an HTStream object. To parse an
**	SGML file, create this object which is a parser. The object
**	is (currently) created by being passed a DTD structure,
**	and a target HTStructured oject at which to throw the parsed stuff.
**
**	 6 Feb 93  Binary seraches used. Intreface modified.
*/

#include "HTUtils.h"
#include "tcp.h"		/* For FROMASCII */

/* Remove the following to disable the experimental HTML DTD parsing.
   Currently only used in this source file. - kw */

#ifndef NO_EXTENDED_HTMLDTD
#define EXTENDED_HTMLDTD
#endif

#include "SGML.h"
#include "HTMLDTD.h"
#include "HTCJK.h"
#include "UCMap.h"
#include "UCDefs.h"
#include "UCAux.h"

#include <ctype.h>
/*#include <stdio.h> included in HTUtils.h -- FM */
#include "HTChunk.h"

#include "LYCharSets.h"
#include "LYLeaks.h"

#define INVALID (-1)

#define FREE(x) if (x) {free(x); x = NULL;}

PUBLIC HTCJKlang HTCJK = NOCJK; 	/* CJK enum value.		*/
PUBLIC BOOL HTPassEightBitRaw = FALSE;	/* Pass 161-172,174-255 raw.	*/
PUBLIC BOOL HTPassEightBitNum = FALSE;	/* Pass ^ numeric entities raw. */
PUBLIC BOOL HTPassHighCtrlRaw = FALSE;	/* Pass 127-160,173,&#127; raw. */
PUBLIC BOOL HTPassHighCtrlNum = FALSE;	/* Pass &#128;-&#159; raw.	*/

extern int LYlowest_eightbit[];

/*	The State (context) of the parser
**
**	This is passed with each call to make the parser reentrant
**
*/

#define MAX_ATTRIBUTES 36	/* Max number of attributes per element */


/*		Element Stack
**		-------------
**	This allows us to return down the stack reselcting styles.
**	As we return, attribute values will be garbage in general.
*/
typedef struct _HTElement HTElement;
struct _HTElement {
	HTElement *	next;	/* Previously nested element or 0 */
	HTTag*		tag;	/* The tag at this level  */
};


/*	Internal Context Data Structure
**	-------------------------------
*/
struct _HTStream {

    CONST HTStreamClass *	isa;		/* inherited from HTStream */

    CONST SGML_dtd		*dtd;
    HTStructuredClass		*actions;	/* target class  */
    HTStructured		*target;	/* target object */

    HTTag			*current_tag;
    CONST HTTag 		*unknown_tag;
    BOOL			inSELECT;
    int 			current_attribute_number;
    HTChunk			*string;
    HTElement			*element_stack;
    enum sgml_state { S_text, S_litteral,
		S_tag, S_tag_gap, S_attr, S_attr_gap, S_equals, S_value,
		S_ero, S_cro, S_incro,
		S_exclamation, S_comment, S_doctype, S_marked,
		S_sgmlent, S_sgmlele, S_sgmlatt,
		S_squoted, S_dquoted, S_end, S_entity,
		S_esc,	  S_dollar,    S_paren,    S_nonascii_text,
		S_dollar_paren,
		S_esc_sq, S_dollar_sq, S_paren_sq, S_nonascii_text_sq,
		S_dollar_paren_sq,
		S_esc_dq, S_dollar_dq, S_paren_dq, S_nonascii_text_dq,
		S_dollar_paren_dq,
		S_in_kanji, S_junk_tag} state;
#ifdef CALLERDATA
    void *			callerData;
#endif /* CALLERDATA */
    BOOL present[MAX_ATTRIBUTES];	/* Flags: attribute is present? */
    char * value[MAX_ATTRIBUTES];	/* malloc'd strings or NULL if none */

    BOOL			lead_exclamation;
    BOOL			first_dash;
    BOOL			end_comment;
    BOOL			doctype_bracket;
    BOOL			first_bracket;
    BOOL			second_bracket;
    BOOL			isHex;

    HTParentAnchor *		node_anchor;
    LYUCcharset *		inUCI;		/* pointer to anchor UCInfo */
    int 			inUCLYhndl;	/* charset we are fed	    */
    LYUCcharset *		outUCI; 	/* anchor UCInfo for target */
    int 			outUCLYhndl;	/* charset for target	    */
    char			utf_count;
    UCode_t			utf_char;
    char			utf_buf[8];
    char *			utf_buf_p;
    UCTransParams		T;
    int 			current_tag_charset; /* charset to pass attributes */

    char *			recover;
    int 			recover_index;
    char *			include;
    int 			include_index;
    char *			url;
    char *			csi;
    int 			csi_index;
} ;

PRIVATE void set_chartrans_handling ARGS3(
	HTStream *,		context,
	HTParentAnchor *,	anchor,
	int,			chndl)
{
    if (chndl < 0) {
	/*
	**  Nothing was set for the parser in earlier stages,
	**  so the HTML parser's UCLYhndl should still be it's
	**  default. - FM
	*/
	chndl = HTAnchor_getUCLYhndl(anchor, UCT_STAGE_STRUCTURED);
	if (chndl < 0)
	    /*
	    **	That wasn't set either, so seek the HText default. - FM
	    */
	    chndl = HTAnchor_getUCLYhndl(anchor, UCT_STAGE_HTEXT);
	if (chndl < 0)
	    /*
	    **	That wasn't set either, so assume the current display
	    **	character set. - FM
	    */
	    chndl = current_char_set;
	/*
	**  Try to set the HText and HTML stages' chartrans info
	**  with the default lock level (will not be changed if
	**  it was set previously with a higher lock level). - FM
	*/
	HTAnchor_setUCInfoStage(anchor, chndl,
				UCT_STAGE_HTEXT,
				UCT_SETBY_DEFAULT);
	HTAnchor_setUCInfoStage(anchor, chndl,
				UCT_STAGE_STRUCTURED,
				UCT_SETBY_DEFAULT);
	/*
	**  Get the chartrans info for output to the HTML parser. - FM
	*/
	context->outUCI = HTAnchor_getUCInfoStage(anchor,
						   UCT_STAGE_STRUCTURED);
	context->outUCLYhndl = HTAnchor_getUCLYhndl(context->node_anchor,
						      UCT_STAGE_STRUCTURED);
    }
    /*
    **	Set the in->out transformation parameters. - FM
    */
    UCSetTransParams(&context->T,
		     context->inUCLYhndl, context->inUCI,
		     context->outUCLYhndl, context->outUCI);
    /*
    **	This is intended for passing the SGML parser's input
    **	charset as an argument in each call to the HTML
    **	parser's start tag function, but it would be better
    **	to call a Lynx_HTML_parser function to set an element
    **	in its HTStructured object, itself, if this were
    **	needed. - FM
    */
    if (HTCJK != NOCJK) {
	context->current_tag_charset = -1;
    } else if (context->T.transp) {
	context->current_tag_charset = context->inUCLYhndl;
    } else if (context->T.decode_utf8) {
	context->current_tag_charset = context->inUCLYhndl;
    } else if (context->T.do_8bitraw ||
	       context->T.use_raw_char_in) {
	context->current_tag_charset = context->inUCLYhndl;
    } else if (context->T.output_utf8 ||
	       context->T.trans_from_uni) {
	context->current_tag_charset = UCGetLYhndl_byMIME("utf-8");
    } else {
	context->current_tag_charset = 0;
    }
}

PRIVATE void change_chartrans_handling ARGS1(
	HTStream *,		context)
{
    int new_LYhndl = HTAnchor_getUCLYhndl(context->node_anchor,
					  UCT_STAGE_PARSER);
    if (new_LYhndl != context->inUCLYhndl &&
	new_LYhndl >= 0) {
	/*
	 *  Something changed. but ignore if a META wants an unknown charset.
	 */
	LYUCcharset * new_UCI = HTAnchor_getUCInfoStage(context->node_anchor,
							UCT_STAGE_PARSER);
	if (new_UCI) {
	    LYUCcharset * next_UCI = HTAnchor_getUCInfoStage(
				    context->node_anchor, UCT_STAGE_STRUCTURED
							    );
	    int next_LYhndl = HTAnchor_getUCLYhndl(
				    context->node_anchor, UCT_STAGE_STRUCTURED
						  );
	    context->inUCI = new_UCI;
	    context->inUCLYhndl = new_LYhndl;
	    context->outUCI = next_UCI;
	    context->outUCLYhndl = next_LYhndl;
	    set_chartrans_handling(context,
				   context->node_anchor, next_LYhndl);
	}
    }
}

#define PUTC(ch) ((*context->actions->put_character)(context->target, ch))
#define PUTUTF8(code) (UCPutUtf8_charstring((HTStream *)context->target, \
		      (putc_func_t*)(context->actions->put_character), code))

extern BOOL historical_comments;
extern BOOL minimal_comments;
extern BOOL soft_dquotes;

#ifdef USE_COLOR_STYLE
#include "AttrList.h"
extern char class_string[TEMPSTRINGSIZE];
int current_is_class=0;
#endif

/*	Handle Attribute
**	----------------
*/
/* PUBLIC CONST char * SGML_default = "";   ?? */

PRIVATE void handle_attribute_name ARGS2(
	HTStream *,	context,
	CONST char *,	s)
{

    HTTag * tag = context->current_tag;
    attr * attributes = tag->attributes;
    int high, low, i, diff;

    /*
    **	Ignore unknown tag. - KW
    */
    if (tag == context->unknown_tag) {
	return;
    }

    /*
    **	Binary search for attribute name.
    */
    for (low = 0, high = tag->number_of_attributes;
	 high > low;
	 diff < 0 ? (low = i+1) : (high = i)) {
	i = (low + (high-low)/2);
	diff = strcasecomp(attributes[i].name, s);
	if (diff == 0) {		/* success: found it */
	    context->current_attribute_number = i;
	    context->present[i] = YES;
	    FREE(context->value[i]);
#ifdef USE_COLOR_STYLE
	    current_is_class=(!strcasecomp("class", s));
	    if (TRACE)
		fprintf(stderr, "SGML: found attribute %s, %d\n", s, current_is_class);
#endif
	    return;
	} /* if */

    } /* for */

    if (TRACE)
	fprintf(stderr, "SGML: Unknown attribute %s for tag %s\n",
	    s, context->current_tag->name);
    context->current_attribute_number = INVALID;	/* Invalid */
}


/*	Handle attribute value
**	----------------------
*/
PRIVATE void handle_attribute_value ARGS2(
	HTStream *,	context,
	CONST char *,	s)
{
    if (context->current_attribute_number != INVALID) {
	StrAllocCopy(context->value[context->current_attribute_number], s);
#ifdef USE_COLOR_STYLE
	if (current_is_class)
	{
	    strncpy (class_string, s, TEMPSTRINGSIZE);
	    if (TRACE)
		fprintf(stderr, "SGML: class is '%s'\n", s);
	}
	else
	{
	    if (TRACE)
		fprintf(stderr, "SGML: attribute value is '%s'\n", s);
	}
#endif
    } else {
	if (TRACE)
	    fprintf(stderr, "SGML: Attribute value %s ignored\n", s);
    }
    context->current_attribute_number = INVALID; /* can't have two assignments! */
}


/*
**  Translate some Unicodes to Lynx special codes and output them.
**  Special codes - ones those output depend on parsing.
**
**  Additional issue, like handling bidirectional text if nesseccery
**  may be called from here:  zwnj (8204), zwj (8205), lrm (8206), rlm (8207)
**  - currently they are passed to def7_uni.tbl as regular characters.
**
*/
PRIVATE BOOL put_special_unicodes ARGS2(
	HTStream *,	context,
	UCode_t,	code)
{
    if (code == 160) {
	/*
	**  Use Lynx special character for nbsp.
	*/
	PUTC(HT_NON_BREAK_SPACE);
    } else  if (code == 173) {
	/*
	**  Use Lynx special character for shy.
	*/
	PUTC(LY_SOFT_HYPHEN);
    } else if (code == 8194 || code == 8195 || code == 8201) {
	/*
	**  Use Lynx special character for ensp, emsp or thinsp.
	**
	**  Originally, Lynx use space '32' as word delimiter and omits this
	**  space at end of line if word is wrapped to the next line.  There
	**  are several other spaces in the Unicode repertoire and we should
	**  teach Lynx to understand them, not only as regular characters but
	**  in the context of line wrapping.  Unfortunately, if we use
	**  HT_EM_SPACE we override the chartrans tables for those spaces
	**  (e.g., emsp= double space) with a single '32' for all (but do line
	**  wrapping more fancy).  In the future we need HT_SPACE with a
	**  transferred parameter (Unicode number) which falls back to
	**  chartrans if line wrapping is not the case.
	**
	*/
	PUTC(HT_EM_SPACE);
#ifdef NOTUSED_FOTEMODS
    } else if (code == 8211 || code == 8212) {
	/*
	**  Use ASCII hyphen for ndash/endash or mdash/emdash.
	*/
	PUTC('-');
#endif
    } else {
	/*
	**  Return NO if nothing done.
	*/
	return NO;
    }
    /*
    **	We have handled it.
    */
    return YES;
}

/*	Handle entity
**	-------------
**
** On entry,
**	s	contains the entity name zero terminated
** Bugs:
**	If the entity name is unknown, the terminator is treated as
**	a printable non-special character in all cases, even if it is '<'
** Bug-fix:
**	Modified SGML_character() so we only come here with terminator
**	as '\0' and check a FoundEntity flag. -- Foteos Macrides
**
** Modified more (for use with Lynx character translation code):
*/
PRIVATE char replace_buf [64];	      /* buffer for replacement strings */
PRIVATE BOOL FoundEntity = FALSE;

#define IncludesLatin1Enc \
		(context->outUCLYhndl == 0 || \
		 (context->outUCI && \
		  (context->outUCI->enc & (UCT_CP_SUPERSETOF_LAT1))))

PRIVATE void handle_entity ARGS2(
	HTStream *,	context,
	char,		term)
{
    UCode_t code;
    long uck;
    CONST char *p;
    CONST char *s = context->string->data;
#ifdef NOTUSED_FOTEMODS
    int high, low, i, diff;
#endif


    /*
    **	Handle all entities normally. - FM
    */
    FoundEntity = FALSE;
    if ((code = HTMLGetEntityUCValue(s)) != 0) {
	/*
	**  We got a Unicode value for the entity name.
	**  Check for special Unicodes. - FM
	*/
	if (put_special_unicodes(context, code)) {
	    FoundEntity = TRUE;
	    return;
	}
	/*
	**  Seek a translation from the chartrans tables.
	*/
	if ((uck = UCTransUniChar(code, context->outUCLYhndl)) >= 32 &&
	    uck < 256 &&
	    (uck < 127 ||
	     uck >= LYlowest_eightbit[context->outUCLYhndl])) {
	    PUTC(FROMASCII((char)uck));
	    FoundEntity = TRUE;
	    return;
	} else if ((uck == -4 ||
		    (context->T.repl_translated_C0 &&
		     uck > 0 && uck < 32)) &&
		   /*
		   **  Not found; look for replacement string.
		   */
		   (uck = UCTransUniCharStr(replace_buf, 60, code,
					    context->outUCLYhndl, 0) >= 0)) {
	    for (p = replace_buf; *p; p++)
		PUTC(*p);
	    FoundEntity = TRUE;
	    return;
	}
	/*
	**  If we're displaying UTF-8, try that now. - FM
	*/
	if (context->T.output_utf8 && PUTUTF8(code)) {
	    FoundEntity = TRUE;
	    return;
	}
	/*
	**  If it's safe ASCII, use it. - FM
	*/
	if (code >= 32 && code < 127) {
	    PUTC(FROMASCII((char)code));
	    FoundEntity = TRUE;
	    return;
	}
#ifdef NOTUSED_FOTEMODS
	/*
	**  If the value is greater than 255 and we do not
	**  have the "7-bit approximations" as our output
	**  character set (in which case we did it already)
	**  seek a translation for that. - FM
	*/
	if ((chk = ((code > 255) &&
		    context->outUCLYhndl !=
				   UCGetLYhndl_byMIME("us-ascii"))) &&
	    (uck = UCTransUniChar(code,
				   UCGetLYhndl_byMIME("us-ascii")))>= 32 &&
	    uck < 127) {
	    /*
	    **	Got an ASCII character (yippey). - FM
	    */
	    PUTC(((char)(uck & 0xff)));
	    FoundEntity = TRUE;
	    return;
	} else if ((chk && uck == -4) &&
		   (uck = UCTransUniCharStr(replace_buf,
					    60, code,
					    UCGetLYhndl_byMIME("us-ascii"),
					    0) >= 0)) {
	    /*
	    **	Got a replacement string (yippey). - FM
	    */
	    for (p = replace_buf; *p; p++)
		PUTC(*p);
	    FoundEntity = TRUE;
	    return;
	}
    }
    /*
    **	Ignore zwnj (8204) and zwj (8205), if we get to here.
    **	Note that zwnj may have been handled as <WBR>
    **	by the calling function. - FM
    */
    if (!strcmp(s, "zwnj") ||
	!strcmp(s, "zwj")) {
	if (TRACE) {
	    fprintf(stderr, "handle_entity: Ignoring '%s'.\n", s);
	}
	FoundEntity = TRUE;
	return;
    }

    /*
    **	Ignore lrm (8206), and rln (8207), if we get to here. - FM
    */
    if (!strcmp(s, "lrm") ||
	!strcmp(s, "rlm")) {
	if (TRACE) {
	    fprintf(stderr, "handle_entity: Ignoring '%s'.\n", s);
	}
	FoundEntity = TRUE;
	return;
    }

    /*
    **	We haven't succeeded yet, so try the old LYCharSets
    **	arrays for translation strings. - FM
    */
    for (low = 0, high = context->dtd->number_of_entities;
	 high > low;
	 diff < 0 ? (low = i+1) : (high = i)) {  /* Binary search */
	i = (low + (high-low)/2);
	diff = strcmp(entities[i], s);	/* Case sensitive! */
	if (diff == 0) {		/* success: found it */
	    for (p = LYCharSets[context->outUCLYhndl][i]; *p; p++) {
		PUTC(*p);
	    }
	    FoundEntity = TRUE;
	    return;
	}
#endif
    }

    /*
    **	If entity string not found, display as text.
    */
    if (TRACE)
	fprintf(stderr, "SGML: Unknown entity '%s'\n", s);
    PUTC('&');
    for (p = s; *p; p++) {
	PUTC(*p);
    }
    if (term != '\0')
	PUTC(term);
}


/*	Handle comment
**	--------------
*/
PRIVATE void handle_comment ARGS1(
	HTStream *,		context)
{
    CONST char *s = context->string->data;

    if (TRACE)
	fprintf(stderr, "SGML Comment:\n<%s>\n", s);

    if (context->csi == NULL &&
	strncmp(s, "!--#", 4) == 0 &&
	LYCheckForCSI(context->node_anchor, (char **)&context->url) == TRUE) {
	LYDoCSI(context->url, s, (char **)&context->csi);
    }

    return;
}


/*	Handle identifier
**	-----------------
*/
PRIVATE void handle_identifier ARGS1(
	HTStream *,		context)
{
    CONST char *s = context->string->data;

    if (TRACE)
	fprintf(stderr, "SGML Identifier\n<%s>\n", s);

    return;
}


/*	Handle doctype
**	--------------
*/
PRIVATE void handle_doctype ARGS1(
	HTStream *,		context)
{
    CONST char *s = context->string->data;

    if (TRACE)
	fprintf(stderr, "SGML Doctype\n<%s>\n", s);

    return;
}


/*	Handle marked
**	-------------
*/
PRIVATE void handle_marked ARGS1(
	HTStream *,		context)
{
    CONST char *s = context->string->data;

    if (TRACE)
	fprintf(stderr, "SGML Marked Section:\n<%s>\n", s);

    return;
}


/*	Handle sgmlent
**	--------------
*/
PRIVATE void handle_sgmlent ARGS1(
	HTStream *,		context)
{
    CONST char *s = context->string->data;

    if (TRACE)
	fprintf(stderr, "SGML Entity Declaration:\n<%s>\n", s);

    return;
}


/*	Handle sgmlent
**	--------------
*/
PRIVATE void handle_sgmlele ARGS1(
	HTStream *,		context)
{
    CONST char *s = context->string->data;

    if (TRACE)
	fprintf(stderr, "SGML Element Declaration:\n<%s>\n", s);

    return;
}


/*	Handle sgmlatt
**	--------------
*/
PRIVATE void handle_sgmlatt ARGS1(
	HTStream *,		context)
{
    CONST char *s = context->string->data;

    if (TRACE)
	fprintf(stderr, "SGML Attribute Declaration:\n<%s>\n", s);

    return;
}

#ifdef EXTENDED_HTMLDTD

PRIVATE BOOL element_valid_within ARGS3(
    HTTag *,	new_tag,
    HTTag *,	stacked_tag,
    BOOL,	direct)
{
    TagClass usecontains, usecontained;
    if (!stacked_tag || !new_tag)
	return YES;
    usecontains = (direct ? stacked_tag->contains : stacked_tag->icontains);
    usecontained = (direct ? new_tag->contained : new_tag->icontained);
    if (new_tag == stacked_tag)
	return ((Tgc_same & usecontains) &&
		(Tgc_same & usecontained));
    else
	return ((new_tag->tagclass & usecontains) &&
		(stacked_tag->tagclass & usecontained));
}

extern BOOL New_DTD;

typedef enum {
    close_NO	= 0,
    close_error = 1,
    close_valid = 2
} canclose_t;

PRIVATE canclose_t can_close ARGS2(
    HTTag *,	new_tag,
    HTTag *,	stacked_tag)
{
    if (!stacked_tag)
	return close_NO;
    if (stacked_tag->flags & Tgf_endO)
	return close_valid;
    else if (new_tag == stacked_tag)
	return ((Tgc_same & new_tag->canclose) ? close_error : close_NO);
    else
	return ((stacked_tag->tagclass & new_tag->canclose) ?
		close_error : close_NO);
}
PRIVATE void do_close_stacked ARGS1(
    HTStream *, context)
{
    HTElement * stacked = context->element_stack;
    if (!stacked)
	return; 		/* stack was empty */
    if (context->inSELECT && !strcasecomp(stacked->tag->name, "SELECT")) {
	context->inSELECT = FALSE;
    }
    (*context->actions->end_element)(
	context->target,
	stacked->tag - context->dtd->tags,
	(char **)&context->include);
    context->element_stack = stacked->next;
    FREE(stacked);
}
PRIVATE int is_on_stack ARGS2(
	HTStream *,	context,
	HTTag *,	old_tag)
{
   HTElement * stacked = context->element_stack;
    int i = 1;
    for (; stacked; stacked = stacked->next, i++) {
	if (stacked->tag == old_tag)
	    return i;
    }
    return 0;
}
#endif /* EXTENDED_HTMLDTD */

/*	End element
**	-----------
*/
PRIVATE void end_element ARGS2(
	HTStream *,	context,
	HTTag *,	old_tag)
{
#ifdef EXTENDED_HTMLDTD

    BOOL extra_action_taken = NO;
    canclose_t canclose_check = close_valid;
    int stackpos = is_on_stack(context, old_tag);

    if (New_DTD) {
	while (canclose_check != close_NO &&
	       context->element_stack &&
	       (stackpos > 1 || (!extra_action_taken && stackpos == 0))) {
	    canclose_check = can_close(old_tag, context->element_stack->tag);
	    if (canclose_check != close_NO) {
		if (TRACE)
		    fprintf(stderr, "SGML: End </%s> \t<- %s end </%s>\n",
			    context->element_stack->tag->name,
			    canclose_check == close_valid ? "supplied," : "forced by",
			    old_tag->name);
		do_close_stacked(context);
		extra_action_taken = YES;
		stackpos = is_on_stack(context, old_tag);
	    } else {
		if (TRACE)
		    fprintf(stderr, "SGML: Still open %s \t<- invalid end </%s>\n",
			    context->element_stack->tag->name,
			    old_tag->name);
		return;
	    }
	}

	if (stackpos == 0 && old_tag->contents != SGML_EMPTY) {
	    if (TRACE)
		fprintf(stderr, "SGML: Still open %s, no open %s for </%s>\n",
			context->element_stack ?
			context->element_stack->tag->name : "none",
			old_tag->name,
			old_tag->name);
	    return;
	}
	if (stackpos > 1) {
	    if (TRACE)
		fprintf(stderr, "SGML: Nesting <%s>...<%s> \t<- invalid end </%s>\n",
			old_tag->name,
			context->element_stack->tag->name,
			old_tag->name);
	    return;
	}
    }
    /* Now let the non-extended code deal with the rest. - kw */

#endif /* EXTENDED_HTMLDTD */

    /*
    **	If we are in a SELECT block, ignore anything
    **	but a SELECT end tag. - FM
    */
    if (context->inSELECT) {
	if (!strcasecomp(old_tag->name, "SELECT")) {
	    /*
	    **	Turn off the inSELECT flag and fall through. - FM
	    */
	    context->inSELECT = FALSE;
	} else {
	    /*
	    **	Ignore the end tag. - FM
	    */
	    if (TRACE) {
		fprintf(stderr,
			"SGML: Ignoring end tag </%s> in SELECT block.\n",
			old_tag->name);
	    }
	    return;
	}
    }
    /*
    **	Handle the end tag. - FM
    */
    if (TRACE)
	fprintf(stderr, "SGML: End </%s>\n", old_tag->name);
    if (old_tag->contents == SGML_EMPTY) {
	if (TRACE)
	    fprintf(stderr, "SGML: Illegal end tag </%s> found.\n",
			    old_tag->name);
	return;
    }
#ifdef WIND_DOWN_STACK
    while (context->element_stack) { /* Loop is error path only */
#else
    if (context->element_stack) { /* Substitute and remove one stack element */
#endif /* WIND_DOWN_STACK */
	HTElement * N = context->element_stack;
	HTTag * t = N->tag;

	if (old_tag != t) {		/* Mismatch: syntax error */
	    if (context->element_stack->next) { /* This is not the last level */
		if (TRACE) fprintf(stderr,
		"SGML: Found </%s> when expecting </%s>. </%s> assumed.\n",
		    old_tag->name, t->name, t->name);
	    } else {			/* last level */
		if (TRACE) fprintf(stderr,
		    "SGML: Found </%s> when expecting </%s>. </%s> Ignored.\n",
		    old_tag->name, t->name, old_tag->name);
		return; 		/* Ignore */
	    }
	}

	context->element_stack = N->next;		/* Remove from stack */
	FREE(N);
	(*context->actions->end_element)(context->target,
		 t - context->dtd->tags, (char **)&context->include);
#ifdef WIND_DOWN_STACK
	if (old_tag == t)
	    return;  /* Correct sequence */
#else
	return;
#endif /* WIND_DOWN_STACK */

	/* Syntax error path only */

    }
    if (TRACE)
	fprintf(stderr, "SGML: Extra end tag </%s> found and ignored.\n",
			old_tag->name);
}


/*	Start a element
*/
PRIVATE void start_element ARGS1(
	HTStream *,	context)
{
    HTTag * new_tag = context->current_tag;

#ifdef EXTENDED_HTMLDTD

    BOOL valid = YES;
    BOOL direct_container = YES;
    BOOL extra_action_taken = NO;
    canclose_t canclose_check = close_valid;

    if (New_DTD) {
	while (context->element_stack &&
	       (canclose_check == close_valid ||
		(canclose_check == close_error &&
		 new_tag == context->element_stack->tag)) &&
	       !(valid = element_valid_within(new_tag, context->element_stack->tag,
					      direct_container))) {
	    canclose_check = can_close(new_tag, context->element_stack->tag);
	    if (canclose_check != close_NO) {
		if (TRACE)
		    fprintf(stderr, "SGML: End </%s> \t<- %s start <%s>\n",
			    context->element_stack->tag->name,
			    canclose_check == close_valid ? "supplied," : "forced by",
			    new_tag->name);
		do_close_stacked(context);
		extra_action_taken = YES;
		if (canclose_check  == close_error)
		    direct_container = NO;
	    } else {
		if (TRACE)
		    fprintf(stderr, "SGML: Still open %s \t<- invalid start <%s>\n",
			    context->element_stack->tag->name,
			    new_tag->name);
	    }
	}
	if (context->element_stack && !valid &&
	    (context->element_stack->tag->flags & Tgf_strict) &&
	    !(valid = element_valid_within(new_tag, context->element_stack->tag,
					   direct_container))) {
	    if (TRACE)
		fprintf(stderr, "SGML: Still open %s \t<- ignoring start <%s>\n",
			context->element_stack->tag->name,
			new_tag->name);
	    return;
	}

	if (context->element_stack && !extra_action_taken &&
	    canclose_check == close_NO && !valid && (new_tag->flags & Tgf_mafse)) {
	    BOOL has_attributes = NO;
	    int i = 0;
	    for (; i< new_tag->number_of_attributes && !has_attributes; i++)
		has_attributes = context->present[i];
	    if (!has_attributes) {
		if (TRACE)
		    fprintf(stderr, "SGML: Still open %s, converting invalid <%s> to </%s>\n",
			    context->element_stack->tag->name,
			    new_tag->name,
			    new_tag->name);
		end_element(context, new_tag);
		return;
	    }
	}

	if (context->element_stack &&
	    canclose_check == close_error && !(valid =
					       element_valid_within(
						   new_tag,
						   context->element_stack->tag,
						   direct_container))) {
	    if (TRACE)
		fprintf(stderr, "SGML: Still open %s \t<- invalid start <%s>\n",
			context->element_stack->tag->name,
			new_tag->name);
	}
    }
    /* Fall through to the non-extended code - kw */

#endif /* EXTENDED_HTMLDTD */

    /*
    **	If we are not in a SELECT block, check if this is
    **	a SELECT start tag.  Otherwise (i.e., we are in a
    **	SELECT block) accept only OPTION as valid, terminate
    **	the SELECT block if it is any other form-related
    **	element, and otherwise ignore it. - FM
    */
    if (!context->inSELECT) {
	/*
	**  We are not in a SELECT block, so check if this starts one. - FM
	*/
	if (!strcasecomp(new_tag->name, "SELECT")) {
	    /*
	    **	Set the inSELECT flag and fall through. - FM
	    */
	    context->inSELECT = TRUE;
	}
    } else {
	/*
	**  We are in a SELECT block. - FM
	*/
	if (strcasecomp(new_tag->name, "OPTION")) {
	    /*
	    **	Ugh, it is not an OPTION. - FM
	    */
	    if (!strcasecomp(new_tag->name, "INPUT") ||
		!strcasecomp(new_tag->name, "TEXTAREA") ||
		!strcasecomp(new_tag->name, "SELECT") ||
		!strcasecomp(new_tag->name, "BUTTON") ||
		!strcasecomp(new_tag->name, "FIELDSET") ||
		!strcasecomp(new_tag->name, "LABEL") ||
		!strcasecomp(new_tag->name, "LEGEND") ||
		!strcasecomp(new_tag->name, "FORM")) {
		/*
		**  It is another form-related start tag, so terminate
		**  the current SELECT block and fall through. - FM
		*/
		if (TRACE)
		    fprintf(stderr,
		       "SGML: Faking SELECT end tag before <%s> start tag.\n",
			    new_tag->name);
		end_element(context, SGMLFindTag(context->dtd, "SELECT"));
	    } else {
		/*
		**  Ignore the start tag. - FM
		*/
		if (TRACE)
		    fprintf(stderr,
			  "SGML: Ignoring start tag <%s> in SELECT block.\n",
			    new_tag->name);
		return;
	    }
	}
    }
    /*
    **	Handle the start tag. - FM
    */
    if (TRACE)
	fprintf(stderr, "SGML: Start <%s>\n", new_tag->name);
    (*context->actions->start_element)(
	context->target,
	new_tag - context->dtd->tags,
	context->present,
	(CONST char**) context->value,	/* coerce type for think c */
	context->current_tag_charset,
	(char **)&context->include);
    if (new_tag->contents != SGML_EMPTY) {		/* i.e. tag not empty */
	HTElement * N = (HTElement *)malloc(sizeof(HTElement));
	if (N == NULL)
	    outofmem(__FILE__, "start_element");
	N->next = context->element_stack;
	N->tag = new_tag;
	context->element_stack = N;
    } else if (!strcasecomp(new_tag->name, "META")) {
	/*
	**  Check for result of META tag. - KW & FM
	*/
	change_chartrans_handling(context);
    }
}


/*		Find Tag in DTD tag list
**		------------------------
**
** On entry,
**	dtd	points to dtd structire including valid tag list
**	string	points to name of tag in question
**
** On exit,
**	returns:
**		NULL		tag not found
**		else		address of tag structure in dtd
*/
PUBLIC HTTag * SGMLFindTag ARGS2(
	CONST SGML_dtd*,	dtd,
	CONST char *,		string)
{
    int high, low, i, diff;
    for (low = 0, high=dtd->number_of_tags;
	 high > low;
	 diff < 0 ? (low = i+1) : (high = i)) {  /* Binary search */
	i = (low + (high-low)/2);
	diff = strcasecomp(dtd->tags[i].name, string);	/* Case insensitive */
	if (diff == 0) {		/* success: found it */
	    return &dtd->tags[i];
	}
    }
    if (isalpha((unsigned char)string[0])) {
	/*
	**  Unrecognized, but may be valid. - KW
	*/
	return (HTTag *)&HTTag_unrecognized;
    }
    return NULL;
}

/*________________________________________________________________________
**			Public Methods
*/


/*	Could check that we are back to bottom of stack! @@  */
/*	Do check! - FM					     */
/*							     */
PRIVATE void SGML_free ARGS1(
	HTStream *,	context)
{
    int i;
    HTElement * cur;
    HTTag * t;

    /*
    **	Free the buffers. - FM
    */
    FREE(context->recover);
    FREE(context->url);
    FREE(context->csi);
    FREE(context->include);

    /*
    **	Wind down stack if any elements are open. - FM
    */
    while (context->element_stack) {
	cur = context->element_stack;
	t = cur->tag;
	context->element_stack = cur->next;	/* Remove from stack */
	FREE(cur);
	(*context->actions->end_element)(context->target,
		 t - context->dtd->tags, (char **)&context->include);
	FREE(context->include);
    }

    /*
    **	Finish off the target. - FM
    */
    (*context->actions->_free)(context->target);

    /*
    **	Free the strings and context structure. - FM
    */
    HTChunkFree(context->string);
    for (i = 0; i < MAX_ATTRIBUTES; i++)
	FREE(context->value[i]);
    FREE(context);
}

PRIVATE void SGML_abort ARGS2(
	HTStream *,	context,
	HTError,	e)
{
    int i;
    HTElement * cur;

    /*
    **	Abort the target. - FM
    */
    (*context->actions->_abort)(context->target, e);

    /*
    **	Free the buffers. - FM
    */
    FREE(context->recover);
    FREE(context->include);
    FREE(context->url);
    FREE(context->csi);

    /*
    **	Free stack memory if any elements were left open. - KW
    */
    while (context->element_stack) {
	cur = context->element_stack;
	context->element_stack = cur->next;	/* Remove from stack */
	FREE(cur);
    }

    /*
    **	Free the strings and context structure. - FM
    */
    HTChunkFree(context->string);
    for (i = 0; i < MAX_ATTRIBUTES; i++)
	FREE(context->value[i]);
    FREE(context);
}


/*	Read and write user callback handle
**	-----------------------------------
**
**   The callbacks from the SGML parser have an SGML context parameter.
**   These calls allow the caller to associate his own context with a
**   particular SGML context.
*/

#ifdef CALLERDATA
PUBLIC void* SGML_callerData ARGS1(
	HTStream *,	context)
{
    return context->callerData;
}

PUBLIC void SGML_setCallerData ARGS2(
	HTStream *,	context,
	void*,		data)
{
    context->callerData = data;
}
#endif /* CALLERDATA */

PRIVATE void SGML_character ARGS2(
	HTStream *,	context,
	char,		c_in)
{
    CONST SGML_dtd *dtd =	context->dtd;
    HTChunk	*string =	context->string;
    CONST char * EntityName;
    char * p;
    BOOLEAN chk;	/* Helps (?) walk through all the else ifs... */
    UCode_t clong, uck; /* Enough bits for UCS4 ... */
    char c;
    char saved_char_in = '\0';

    /*
    **	Now some fun with the preprocessor.
    **	Use copies for c and unsign_c == clong, so that
    **	we can revert back to the unchanged c_in. - KW
    */
#define unsign_c clong

    c = c_in;
    clong = (unsigned char)c;	/* a.k.a. unsign_c */

    if (context->T.decode_utf8) {
	/*
	**  Combine UTF-8 into Unicode.
	**  Incomplete characters silently ignored.
	**  From Linux kernel's console.c. - KW
	*/
	if ((unsigned char)c > 127) {
	    /*
	    **	We have an octet from a multibyte character. - FM
	    */
	    if (context->utf_count > 0 && (c & 0xc0) == 0x80) {
		context->utf_char = (context->utf_char << 6) | (c & 0x3f);
		context->utf_count--;
		*(context->utf_buf_p) = c;
		(context->utf_buf_p)++;
		if (context->utf_count == 0) {
		    /*
		    **	We have all of the bytes, so terminate
		    **	the buffer and set 'clong' to the UCode_t
		    **	value. - FM
		    */
		    *(context->utf_buf_p) = '\0';
		    clong = context->utf_char;
		    if (clong < 256) {
			c = ((char)(clong & 0xff));
		    }
		    goto top1;
		} else {
		    /*
		    **	Wait for more. - KW
		    */
		    return;
		}
	    } else {
		/*
		**  Start handling a new multibyte character. - FM
		*/
		context->utf_buf_p = context->utf_buf;
		*(context->utf_buf_p) = c;
		(context->utf_buf_p)++;
		if ((c & 0xe0) == 0xc0) {
		    context->utf_count = 1;
		    context->utf_char = (c & 0x1f);
		} else if ((c & 0xf0) == 0xe0) {
		    context->utf_count = 2;
		    context->utf_char = (c & 0x0f);
		} else if ((c & 0xf8) == 0xf0) {
		    context->utf_count = 3;
		    context->utf_char = (c & 0x07);
		} else if ((c & 0xfc) == 0xf8) {
		    context->utf_count = 4;
		    context->utf_char = (c & 0x03);
		} else if ((c & 0xfe) == 0xfc) {
		    context->utf_count = 5;
		    context->utf_char = (c & 0x01);
		} else {
		    /*
		    **	Garbage. - KW
		    */
		    context->utf_count = 0;
		    context->utf_buf_p = context->utf_buf;
		    *(context->utf_buf_p) = '\0';
		}
		/*
		**  Wait for more. - KW
		*/
		return;
	    }
	} else {
	    /*
	    **	Got an ASCII char. - KW
	    */
	    context->utf_count = 0;
	    context->utf_buf_p = context->utf_buf;
	    *(context->utf_buf_p) = '\0';
		    /*	goto top;  */
	}
    }

#ifdef NOTDEFINED
    /*
    **	If we have a koi8-r input and do not have
    **	koi8-r as the output, save the raw input
    **	in saved_char_in before we potentially
    **	convert it to Unicode. - FM
    */
    if (context->T.strip_raw_char_in)
	saved_char_in = c;
#endif /* NOTDEFINED */

    /*
    **	If we want the raw input converted
    **	to Unicode, try that now. - FM
    */
    if (context->T.trans_to_uni &&
	((unsign_c >= 127) ||
	 (unsign_c < 32 && unsign_c != 0 &&
	  context->T.trans_C0_to_uni))) {
	/*
	**  Convert the octet to Unicode. - FM
	*/
	clong = UCTransToUni(c, context->inUCLYhndl);
	if (clong > 0) {
	    saved_char_in = c;
	    if (clong < 256) {
		c = FROMASCII((char)clong);
	    }
	}
	goto top1;
    } else if (unsign_c < 32 && unsign_c != 0 &&
	       context->T.trans_C0_to_uni) {
	/*
	**  This else if may be too ugly to keep. - KW
	*/
	if (context->T.trans_from_uni &&
	    (((clong = UCTransToUni(c, context->inUCLYhndl)) >= 32) ||
	     (context->T.transp &&
	      (clong = UCTransToUni(c, context->inUCLYhndl)) > 0))) {
	    saved_char_in = c;
	    if (clong < 256) {
		c = FROMASCII((char)clong);
	    }
	    goto top1;
	} else {
	    uck = -1;
	    if (context->T.transp) {
		uck = UCTransCharStr(replace_buf, 60, c,
				     context->inUCLYhndl,
				     context->inUCLYhndl, NO);
	    }
	    if (!context->T.transp || uck < 0) {
		uck = UCTransCharStr(replace_buf, 60, c,
				     context->inUCLYhndl,
				     context->outUCLYhndl, YES);
	    }
	    if (uck == 0) {
		return;
	    } else if (uck < 0) {
		goto top0a;
	    }
	    c = replace_buf[0];
	    if (c && replace_buf[1]) {
		if (context->state == S_text) {
		    for (p = replace_buf; *p; p++)
			PUTC(*p);
		    return;
		}
		StrAllocCat(context->recover, replace_buf + 1);
	    }
	    goto top0a;
	} /*  Next line end of ugly stuff for C0. - KW */
    } else {
	goto top0a;
    }

    /*
    **	At this point we have either unsign_c a.k.a. clong in
    **	Unicode (and c in latin1 if clong is in the latin1 range),
    **	or unsign_c and c will have to be passed raw. - KW
    */
/*
**  We jump up to here from below if we have
**  stuff in the recover, insert, or csi buffers
**  to process.  We zero saved_char_in, in effect
**  as a flag that the octet in not that of the
**  actual call to this function.  This may be OK
**  for now, for the stuff this function adds to
**  its recover buffer, but it might not be for
**  stuff other functions added to the insert or
**  csi buffer, so bear that in mind. - FM
*/
top:
    saved_char_in = '\0';
/*
**  We jump to here from above when we don't have
**  UTF-8 input, haven't converted to Unicode, and
**  want clong set to the input octet (unsigned)
**  without zeroing its saved_char_in copy (which
**  is signed). - FM
*/
top0a:
    *(context->utf_buf) = '\0';
    clong = (unsigned char)c;
/*
**  We jump to here from above if we have converted
**  the input, or a multibyte sequence across calls,
**  to a Unicode value and loaded it into clong (to
**  which unsign_c has been defined), and from below
**  when we are recycling a character (e.g., because
**  it terminated an entity but is not the standard
**  semi-colon).  The chararcter will already have
**  been put through the Unicode conversions. - FM
*/
top1:
    /*
    **	Ignore low ISO 646 7-bit control characters
    **	if HTCJK is not set. - FM
    */
    if (unsign_c < 32 &&
	c != 9 && c != 10 && c != 13 &&
	HTCJK == NOCJK)
	return;

    /*
    **	Ignore 127 if we don't have HTPassHighCtrlRaw
    **	or HTCJK set. - FM
    */
#define PASSHICTRL (context->T.transp || \
		    unsign_c >= LYlowest_eightbit[context->inUCLYhndl])
    if (c == 127 &&
	!(PASSHICTRL || HTCJK != NOCJK))
	return;

    /*
    **	Ignore 8-bit control characters 128 - 159 if
    **	neither HTPassHighCtrlRaw nor HTCJK is set. - FM
    */
    if (unsign_c > 127 && unsign_c < 160 &&
	!(PASSHICTRL || HTCJK != NOCJK))
	return;

    /*
    **	Handle character based on context->state.
    */
    switch(context->state) {

    case S_in_kanji:
	/*
	**  Note that if we don't have a CJK input, then this
	**  is not the second byte of a CJK di-byte, and we're
	**  trashing the input.  That's why 8-bit characters
	**  followed by, for example, '<' can cause the tag to
	**  be treated as text, not markup.  We could try to deal
	**  with it by holding each first byte and then checking
	**  byte pairs, but that doesn't seem worth the overhead
	**  (see below). - FM
	*/
	context->state = S_text;
	PUTC(c);
	break;

    case S_text:
	if (HTCJK != NOCJK && (c & 0200) != 0) {
	    /*
	    **	Setting up for Kanji multibyte handling (based on
	    **	Takuya ASADA's (asada@three-a.co.jp) CJK Lynx).
	    **	Note that if the input is not in fact CJK, the
	    **	next byte also will be mishandled, as explained
	    **	above.	Toggle raw mode off in such cases, or
	    **	select the "7 bit approximations" display
	    **	character set, which is largely equivalent
	    **	to having raw mode off with CJK. - FM
	    */
	    context->state = S_in_kanji;
	    PUTC(c);
	    break;
	} else if (HTCJK != NOCJK && c == '\033') {
	    /*
	    **	Setting up for CJK escape sequence handling (based on
	    **	Takuya ASADA's (asada@three-a.co.jp) CJK Lynx). - FM
	    */
	    context->state = S_esc;
	    PUTC(c);
	    break;
	}
	if (c == '&' && unsign_c < 127	&&
	    (!context->element_stack ||
	     (context->element_stack->tag  &&
	      (context->element_stack->tag->contents == SGML_MIXED ||
	       context->element_stack->tag->contents == SGML_PCDATA ||
	       context->element_stack->tag->contents == SGML_RCDATA)))) {
	    /*
	    **	Setting up for possible entity, without the leading '&'. - FM
	    */
	    string->size = 0;
	    context->state = S_ero;
	} else if (c == '<' && unsign_c < 127) {
	    /*
	    **	Setting up for possible tag. - FM
	    */
	    string->size = 0;
	    context->state = (context->element_stack &&
			context->element_stack->tag  &&
			context->element_stack->tag->contents == SGML_LITTERAL)
					 ?
			      S_litteral : S_tag;
#define PASS8859SPECL context->T.pass_160_173_raw
	/*
	**  Convert 160 (nbsp) to Lynx special character if
	**  neither HTPassHighCtrlRaw nor HTCJK is set. - FM
	*/
	} else if (unsign_c == 160 &&
		   !(PASS8859SPECL || HTCJK != NOCJK)) {
	    PUTC(HT_NON_BREAK_SPACE);
	/*
	**  Convert 173 (shy) to Lynx special character if
	**  neither HTPassHighCtrlRaw nor HTCJK is set. - FM
	*/
	} else if (unsign_c == 173 &&
		   !(PASS8859SPECL || HTCJK != NOCJK)) {
	    PUTC(LY_SOFT_HYPHEN);
	/*
	**  Handle the case in which we think we have a character
	**  which doesn't need further processing (e.g., a koi8-r
	**  input for a koi8-r output). - FM
	*/
	} else if (context->T.use_raw_char_in && saved_char_in) {
	    /*
	    **	Only if the original character is still in saved_char_in,
	    **	otherwise we may be iterating from a goto top. - KW
	    */
	    PUTC(saved_char_in);
	    saved_char_in = '\0';
/******************************************************************
 *   I. LATIN-1 OR UCS2  TO  DISPLAY CHARSET
 ******************************************************************/
	} else if ((chk = (context->T.trans_from_uni && unsign_c >= 160)) &&
		   (uck = UCTransUniChar(unsign_c,
					 context->outUCLYhndl)) >= 32 &&
		   uck < 256) {
	    if (TRACE) {
		fprintf(stderr,
			"UCTransUniChar returned 0x%.2lX:'%c'.\n",
			uck, FROMASCII((char)uck));
	    }
	    /*
	    **	We got one octet from the conversions, so use it. - FM
	    */
	    PUTC(FROMASCII((char)uck));
	} else if ((chk &&
		   (uck == -4 ||
		    (context->T.repl_translated_C0 &&
		     uck > 0 && uck < 32))) &&
		   /*
		   **  Not found; look for replacement string. - KW
		   */
		   (uck = UCTransUniCharStr(replace_buf, 60, clong,
					    context->outUCLYhndl,
					    0) >= 0)) {
	    /*
	    **	Got a replacement string.
	    **	No further tests for valididy - assume that whoever
	    **	defined replacement strings knew what she was doing. - KW
	    */
	    for (p = replace_buf; *p; p++)
		PUTC(*p);
	/*
	**  If we're displaying UTF-8, try that now. - FM
	*/
	} else if (context->T.output_utf8 && PUTUTF8(clong)) {
	    ; /* do nothing more */
	/*
	**  If it's any other (> 160) 8-bit chararcter, and
	**  we have not set HTPassEightBitRaw nor HTCJK, nor
	**  have the "ISO Latin 1" character set selected,
	**  back translate for our character set. - FM
	*/
#define PASSHI8BIT (HTPassEightBitRaw || \
		    (context->T.do_8bitraw && !context->T.trans_from_uni))
	} else if (unsign_c > 160 && unsign_c < 256 &&
		   !(PASSHI8BIT || HTCJK != NOCJK) &&
		   !IncludesLatin1Enc) {
	    int i;

	    string->size = 0;
	    EntityName = HTMLGetEntityName((int)(unsign_c - 160));
	    for (i = 0; EntityName[i]; i++)
		HTChunkPutc(string, EntityName[i]);
	    HTChunkTerminate(string);
	    handle_entity(context, '\0');
	    string->size = 0;
	    if (!FoundEntity)
		PUTC(';');
	/*
	**  If we get to here and have an ASCII char,
	**  pass the character. - KW
	*/
	} else if (unsign_c < 127 && unsign_c > 0) {
	    PUTC(c);
	/*
	**  If we get to here, and should have translated,
	**  translation has failed so far. - KW
	**
	**  We should have sent UTF-8 output to the parser
	**  already, but what the heck, try again. - FM
	*/
	} else if (context->T.output_utf8 && *context->utf_buf) {
	    for (p = context->utf_buf; *p; p++)
		PUTC(*p);
	    context->utf_buf_p = context->utf_buf;
	    *(context->utf_buf_p) = '\0';
#ifdef NOTDEFINED
	/*
	**  Check for a strippable koi8-r 8-bit character. - FM
	*/
	} else if (context->T.strip_raw_char_in && saved_char_in &&
		   ((unsigned char)saved_char_in >= 0xc0) &&
		   ((unsigned char)saved_char_in < 255)) {
	    /*
	    **	KOI8 special: strip high bit, gives (somewhat) readable
	    **	ASCII or KOI7 - it was constructed that way! - KW
	    */
	    PUTC(((char)(saved_char_in & 0x7f)));
	    saved_char_in = '\0';
#endif /* NOTDEFINED */
	/*
	**  If we don't actually want the character,
	**  make it safe and output that now. - FM
	*/
	} else if ((unsigned char)c <
			LYlowest_eightbit[context->outUCLYhndl] ||
		   (context->T.trans_from_uni && !HTPassEightBitRaw)) {
#ifdef NOTUSED_FOTEMODS
	    /*
	    **	If we do not have the "7-bit approximations" as our
	    **	output character set (in which case we did it already)
	    **	seek a translation for that.  Otherwise, or if the
	    **	translation fails, use UHHH notation. - FM
	    */
	    if ((chk = (context->outUCLYhndl !=
			UCGetLYhndl_byMIME("us-ascii"))) &&
		(uck = UCTransUniChar(unsign_c,
				      UCGetLYhndl_byMIME("us-ascii")))
				      >= 32 && uck < 127) {
		/*
		**  Got an ASCII character (yippey). - FM
		*/
		PUTC(((char)(uck & 0xff)));
	    } else if ((chk && uck == -4) &&
		       (uck = UCTransUniCharStr(replace_buf,
						60, clong,
						UCGetLYhndl_byMIME("us-ascii"),
						0) >= 0)) {
		/*
		**  Got a replacement string (yippey). - FM
		*/
		for (p = replace_buf; *p; p++)
		    PUTC(*p);
	    } else {
		/*
		**  Out of luck, so use the UHHH notation (ugh). - FM
		*/
#endif /* NOTUSED_FOTEMODS */
		sprintf(replace_buf, "U%.2lX", unsign_c);
		for (p = replace_buf; *p; p++) {
		    PUTC(*p);
		}
#ifdef NOTUSED_FOTEMODS
	    }
#endif /* NOTUSED_FOTEMODS */
	/*
	**  If we get to here, pass the character. - FM
	*/
	} else {
	    PUTC(c);
	}
	break;

    /*
    **	In litteral mode, waits only for specific end tag (for
    **	compatibility with old servers, and for Lynx). - FM
    */
    case S_litteral:
	HTChunkPutc(string, c);
	if (TOUPPER(c) != ((string->size == 1) ?
					   '/' :
			context->element_stack->tag->name[string->size-2])) {
	    int i;

	    /*
	    **	If complete match, end litteral.
	    */
	    if ((c == '>') &&
		(!context->element_stack->tag->name[string->size-2])) {
		end_element(context, context->element_stack->tag);
		string->size = 0;
		context->current_attribute_number = INVALID;
		context->state = S_text;
		break;
	    }
	    /*
	    **	If Mismatch: recover string.
	    */
	    PUTC('<');
	    for (i = 0; i < string->size; i++)	/* recover */
	       PUTC(string->data[i]);
	    string->size = 0;
	    context->state = S_text;
	}
	break;

    /*
    **	Character reference (numeric entity) or named entity.
    */
    case S_ero:
	if (c == '#') {
	    /*
	    **	Setting up for possible numeric entity.
	    */
	    context->state = S_cro;  /* &# is Char Ref Open */
	    break;
	}
	context->state = S_entity;   /* Fall through! */

    /*
    **	Handle possible named entity.
    */
    case S_entity:
	if (unsign_c < 127 && (string->size ?
		  isalnum((unsigned char)c) : isalpha((unsigned char)c))) {
	    /*
	    **	Accept valid ASCII character. - FM
	    */
	    HTChunkPutc(string, c);
	} else if (string->size == 0) {
	    /*
	    **	It was an ampersand that's just text, so output
	    **	the ampersand and recycle this character. - FM
	    */
	    PUTC('&');
	    context->state = S_text;
	    goto top1;
	} else {
	    /*
	    **	Terminate entity name and try to handle it. - FM
	    */
	    HTChunkTerminate(string);
	    if (!strcmp(string->data, "zwnj") &&
		(!context->element_stack ||
		 (context->element_stack->tag  &&
		  context->element_stack->tag->contents == SGML_MIXED))) {
		/*
		**  Handle zwnj (8204) as <WBR>. - FM
		*/
		char temp[8];

		if (TRACE) {
		    fprintf(stderr,
		"SGML_character: Handling 'zwnj' entity as 'WBR' element.\n");
		}
		if (c != ';') {
		    sprintf(temp, "<WBR>%c", c);
		} else {
		    sprintf(temp, "<WBR>");
		}
		if (context->recover == NULL) {
		    StrAllocCopy(context->recover, temp);
		    context->recover_index = 0;
		} else {
		    StrAllocCat(context->recover, temp);
		}
		string->size = 0;
		context->state = S_text;
		break;
	    } else {
		handle_entity(context, '\0');
	    }
	    string->size = 0;
	    context->state = S_text;
	    /*
	    **	Don't eat the terminator if we didn't find the
	    **	entity name and therefore sent the raw string
	    **	via handle_entity(), or if the terminator is
	    **	not the "standard" semi-colon for HTML. - FM
	    */
	    if (!FoundEntity || c != ';')
		goto top1;
	}
	break;

    /*
    **	Check for a numeric entity.
    */
    case S_cro:
	if (unsign_c < 127 && TOLOWER((unsigned char)c) == 'x') {
	    context->isHex = TRUE;
	    context->state = S_incro;
	} else if (unsign_c < 127 && isdigit((unsigned char)c)) {
	    /*
	    **	Accept only valid ASCII digits. - FM
	    */
	    HTChunkPutc(string, c);	/* accumulate a character NUMBER */
	    context->isHex = FALSE;
	    context->state = S_incro;
	} else if (string->size == 0) {
	    /*
	    **	No 'x' or digit following the "&#" so recover
	    **	them and recycle the character. - FM
	    */
	    PUTC('&');
	    PUTC('#');
	    context->state = S_text;
	    goto top1;
	}
	break;

    /*
    **	Handle a numeric entity.
    */
    case S_incro:
	if ((unsign_c < 127) &&
	    (context->isHex ? isxdigit((unsigned char)c) :
			      isdigit((unsigned char)c))) {
	    /*
	    **	Accept only valid hex or ASCII digits. - FM
	    */
	    HTChunkPutc(string, c);	/* accumulate a character NUMBER */
	} else if (string->size == 0) {
	    /*
	    **	No hex digit following the "&#x" so recover
	    **	them and recycle the character. - FM
	    */
	    PUTC('&');
	    PUTC('#');
	    PUTC('x');
	    context->isHex = FALSE;
	    context->state = S_text;
	    goto top1;
	} else {
	    /*
	    **	Terminate the numeric entity and try to handle it. - FM
	    */
	    UCode_t code;
	    int i;
	    HTChunkTerminate(string);
	    if ((context->isHex ? sscanf(string->data, "%lx", &code) :
				  sscanf(string->data, "%ld", &code)) == 1) {
		if ((code == 1) ||
		    (code > 129 && code < 156)) {
		    /*
		    **	Assume these are Microsoft code points,
		    **	inflicted on us by FrontPage. - FM
		    **
		    **	MS FrontPage uses syntax like &#153; in 128-159 range
		    **	and doesn't follow Unicode standards for this area.
		    **	Windows-1252 codepoints are assumed here.
		    */
		    switch (code) {
			case 1:
			    /*
			    **	WHITE SMILING FACE
			    */
			    code = 0x263a;
			    break;
			case 130:
			    /*
			    **	SINGLE LOW-9 QUOTATION MARK (sbquo)
			    */
			    code = 0x201a;
			    break;
			case 132:
			    /*
			    **	DOUBLE LOW-9 QUOTATION MARK (bdquo)
			    */
			    code = 0x201e;
			    break;
			case 133:
			    /*
			    **	HORIZONTAL ELLIPSIS (hellip)
			    */
			    code = 0x2026;
			    break;
			case 134:
			    /*
			    **	DAGGER (dagger)
			    */
			    code = 0x2020;
			    break;
			case 135:
			    /*
			    **	DOUBLE DAGGER (Dagger)
			    */
			    code = 0x2021;
			    break;
			case 137:
			    /*
			    **	PER MILLE SIGN (permil)
			    */
			    code = 0x2030;
			    break;
			case 139:
			    /*
			    **	SINGLE LEFT-POINTING ANGLE QUOTATION MARK
			    **	(lsaquo)
			    */
			    code = 0x2039;
			    break;
			case 145:
			    /*
			    **	LEFT SINGLE QUOTATION MARK (lsquo)
			    */
			    code = 0x2018;
			    break;
			case 146:
			    /*
			    **	RIGHT SINGLE QUOTATION MARK (rsquo)
			    */
			    code = 0x2019;
			    break;
			case 147:
			    /*
			    **	LEFT DOUBLE QUOTATION MARK (ldquo)
			    */
			    code = 0x201c;
			    break;
			case 148:
			    /*
			    **	RIGHT DOUBLE QUOTATION MARK (rdquo)
			    */
			    code = 0x201d;
			    break;
			case 149:
			    /*
			    **	BULLET (bull)
			    */
			    code = 0x2022;
			    break;
			case 150:
			    /*
			    **	EN DASH (ndash)
			    */
			    code = 0x2013;
			    break;
			case 151:
			    /*
			    **	EM DASH (mdash)
			    */
			    code = 0x2014;
			    break;
			case 152:
			    /*
			    **	SMALL TILDE (tilde)
			    */
			    code = 0x02dc;
			    break;
			case 153:
			    /*
			    **	TRADE MARK SIGN (trade)
			    */
			    code = 0x2122;
			    break;
			case 155:
			    /*
			    **	SINGLE RIGHT-POINTING ANGLE QUOTATION MARK
			    **	(rsaquo)
			    */
			    code = 0x203a;
			    break;
			default:
			    /*
			    **	Do not attempt a conversion
			    **	to valid Unicode values.
			    */
			    break;
		    }
		}
		/*
		**  Check for special values. - FM
		*/
		if ((code == 8204) &&
		    (!context->element_stack ||
		     (context->element_stack->tag  &&
		      context->element_stack->tag->contents == SGML_MIXED))) {
		    /*
		    **	Handle zwnj (8204) as <WBR>. - FM
		    */
		    char temp[8];

		    if (TRACE) {
			fprintf(stderr,
      "SGML_character: Handling '8204' (zwnj) reference as 'WBR' element.\n");
		    }
		    /*
		    **	Include the terminator if it is not
		    **	the standard semi-colon. - FM
		    */
		    if (c != ';') {
			sprintf(temp, "<WBR>%c", c);
		    } else {
			sprintf(temp, "<WBR>");
		    }
		    /*
		    **	Add the replacement string to the
		    **	recover buffer for processing. - FM
		    */
		    if (context->recover == NULL) {
			StrAllocCopy(context->recover, temp);
			context->recover_index = 0;
		    } else {
			StrAllocCat(context->recover, temp);
		    }
		    string->size = 0;
		    context->isHex = FALSE;
		    context->state = S_text;
		    break;
		} else if (put_special_unicodes(context, code)) {
		    /*
		    **	We handled the value as a special character,
		    **	so recycle the terminator or break. - FM
		    */
		    string->size = 0;
		    context->isHex = FALSE;
		    context->state = S_text;
		    if (c != ';')
			goto top1;
		    break;
		}
		/*
		**  Seek a translation from the chartrans tables.
		*/
		if ((uck = UCTransUniChar(code,
					  context->outUCLYhndl)) >= 32 &&
		    uck < 256 &&
		    (uck < 127 ||
		     uck >= LYlowest_eightbit[context->outUCLYhndl])) {
		    PUTC(FROMASCII((char)uck));
		} else if ((uck == -4 ||
			    (context->T.repl_translated_C0 &&
			     uck > 0 && uck < 32)) &&
			   /*
			   **  Not found; look for replacement string.
			   */
			   (uck = UCTransUniCharStr(replace_buf, 60, code,
						    context->outUCLYhndl,
						    0) >= 0)) {
		    for (p = replace_buf; *p; p++) {
			PUTC(*p);
		    }
		/*
		**  If we're displaying UTF-8, try that now. - FM
		*/
		} else if (context->T.output_utf8 && PUTUTF8(code)) {
		    ;  /* do nothing more */
#ifdef NOTUSED_FOTEMODS
		/*
		**  If the value is greater than 255 and we do not
		**  have the "7-bit approximations" as our output
		**  character set (in which case we did it already)
		**  seek a translation for that. - FM
		*/
		} else if ((chk = ((code > 255) &&
				   context->outUCLYhndl !=
				   UCGetLYhndl_byMIME("us-ascii"))) &&
			   (uck = UCTransUniChar(code,
				   UCGetLYhndl_byMIME("us-ascii")))
				  >= 32 && uck < 127) {
		    /*
		    **	Got an ASCII character (yippey). - FM
		    */
		    PUTC(((char)(uck & 0xff)));
		} else if ((chk && uck == -4) &&
			   (uck = UCTransUniCharStr(replace_buf,
						    60, code,
						UCGetLYhndl_byMIME("us-ascii"),
						    0) >= 0)) {
		    /*
		    **	Got a replacement string (yippey). - FM
		    */
		    for (p = replace_buf; *p; p++)
			PUTC(*p);
		/*
		**  Ignore 8205 (zwj),
		**  8206 (lrm), and 8207 (rln), if we get to here. - FM
		*/
		} else if (code == 8205 ||
			   code == 8206 ||
			   code == 8207) {
		    if (TRACE) {
			string->size--;
			LYstrncpy(replace_buf,
				  string->data,
				  (string->size < 64 ? string->size : 63));
			fprintf(stderr,
				"SGML_character: Ignoring '%s%s'.\n",
				(context->isHex ? "&#x" : "&#"),
				replace_buf);
		    }
		    string->size = 0;
		    context->isHex = FALSE;
		    context->state = S_text;
		    if (c != ';')
			goto top1;
		    break;
#endif /* NOTUSED_FOTEMODS */
		/*
		**  Show the numeric entity if we get to here
		**  and the value:
		**   (1) Is greater than 255 (but use ASCII characters
		**	 for spaces or dashes).
		**   (2) Is less than 32, and not valid or we don't
		**	 have HTCJK set.
		**   (3) Is 127 and we don't have HTPassHighCtrlRaw or
		**	 HTCJK set.
		**   (4) Is 128 - 159 and we don't have HTPassHighCtrlNum
		**	 set.
		**  - FM
		*/
		} else if ((code > 255) ||
			   (code < 32 &&
			    code != 9 && code != 10 && code != 13 &&
			    HTCJK == NOCJK) ||
			   (code == 127 &&
			    !(HTPassHighCtrlRaw || HTCJK != NOCJK)) ||
			   (code > 127 && code < 160 &&
			    !HTPassHighCtrlNum)) {
			/*
			**  Unhandled or illegal value.  Recover the
			**  "&#" or "&#x" and digit(s), and recycle
			**  the terminator. - FM
			*/
			PUTC('&');
			PUTC('#');
			if (context->isHex) {
			    PUTC('x');
			    context->isHex = FALSE;
			}
			string->size--;
			for (i = 0; i < string->size; i++)	/* recover */
			    PUTC(string->data[i]);
			string->size = 0;
			context->isHex = FALSE;
			context->state = S_text;
			goto top1;
		} else if (code < 161 ||
			   HTPassEightBitNum ||
			   IncludesLatin1Enc) {
		    /*
		    **	No conversion needed. - FM
		    */
		    PUTC(FROMASCII((char)code));
		} else {
		    /*
		    **	Handle as named entity. - FM
		    */
		    code -= 160;
		    EntityName = HTMLGetEntityName(code);
		    if (EntityName && EntityName[0] != '\0') {
			string->size = 0;
			for (i = 0; EntityName[i]; i++)
			    HTChunkPutc(string, EntityName[i]);
			HTChunkTerminate(string);
			handle_entity(context, '\0');
			/*
			**  Add a semi-colon if something went wrong
			**  and handle_entity() sent the string. - FM
			*/
			if (!FoundEntity) {
			    PUTC(';');
			}
		    } else {
			/*
			**  Our conversion failed, so recover the "&#"
			**  and digit(s), and recycle the terminator. - FM
			*/
			PUTC('&');
			PUTC('#');
			if (context->isHex) {
			    PUTC('x');
			    context->isHex = FALSE;
			}
			string->size--;
			for (i = 0; i < string->size; i++)	/* recover */
			    PUTC(string->data[i]);
			string->size = 0;
			context->isHex = FALSE;
			context->state = S_text;
			goto top1;
		    }
		}
		/*
		**  If we get to here, we succeeded.  Hoorah!!! - FM
		*/
		string->size = 0;
		context->isHex = FALSE;
		context->state = S_text;
		/*
		**  Don't eat the terminator if it's not
		**  the "standard" semi-colon for HTML. - FM
		*/
		if (c != ';') {
		    goto top1;
		}
	    } else {
		/*
		**  Not an entity, and don't know why not, so add
		**  the terminator to the string, output the "&#"
		**  or "&#x", and process the string via the recover
		**  element. - FM
		*/
		string->size--;
		HTChunkPutc(string, c);
		HTChunkTerminate(string);
		PUTC('&');
		PUTC('#');
		if (context->isHex) {
		    PUTC('x');
		    context->isHex = FALSE;
		}
		if (context->recover == NULL) {
		    StrAllocCopy(context->recover, string->data);
		    context->recover_index = 0;
		} else {
		    StrAllocCat(context->recover, string->data);
		}
		string->size = 0;
		context->isHex = FALSE;
		context->state = S_text;
		break;
	    }
	}
	break;

    /*
    **	Tag
    */
    case S_tag: 				/* new tag */
	if (unsign_c < 127 && (string->size ?
		  isalnum((unsigned char)c) : isalpha((unsigned char)c))) {
	    /*
	    **	Add valid ASCII character. - FM
	    */
	    HTChunkPutc(string, c);
	} else if (c == '!' && !string->size) { /* <! */
	    /*
	    **	Terminate and set up for possible comment,
	    **	identifier, declaration, or marked section. - FM
	    */
	    context->state = S_exclamation;
	    context->lead_exclamation = TRUE;
	    context->doctype_bracket = FALSE;
	    context->first_bracket = FALSE;
	    HTChunkPutc(string, c);
	    break;
	} else if (!string->size &&
		   (unsign_c <= 160 &&
		    (c != '/' && c != '?' && c != '_' && c != ':'))) {
	    /*
	    **	'<' must be followed by an ASCII letter to be a valid
	    **	start tag.  Here it isn't, nor do we have a '/' for an
	    **	end tag, nor one of some other characters with a
	    **	special meaning for SGML or which are likely to be legal
	    **	Name Start characters in XML or some other extension.
	    **	So recover the '<' and following character as data. - FM & KW
	    */
	    context->state = S_text;
	    PUTC('<');
	    goto top1;
	} else {				/* End of tag name */
	    /*
	    **	Try to handle tag. - FM
	    */
	    HTTag * t;
	    if (c == '/') {
		if (TRACE)
		    if (string->size!=0)
			fprintf(stderr,"SGML: `<%s/' found!\n", string->data);
		context->state = S_end;
		break;
	    }
	    HTChunkTerminate(string) ;

	    t = SGMLFindTag(dtd, string->data);
	    if (t == context->unknown_tag && c == ':' &&
		0 == strcasecomp(string->data, "URL")) {
		/*
		**  Treat <URL: as text rather than a junk tag,
		**  so we display it and the URL (Lynxism 8-). - FM
		*/
		int i;
		PUTC('<');
		for (i = 0; i < 3; i++) /* recover */
		    PUTC(string->data[i]);
		PUTC(c);
		if (TRACE)
		    fprintf(stderr, "SGML: Treating <%s%c as text\n",
			    string->data, c);
		string->size = 0;
		context->state = S_text;
		break;
	    } else if (!t) {
		if (TRACE)
		    fprintf(stderr, "SGML: *** Invalid element %s\n",
			    string->data);
		context->state = (c == '>') ? S_text : S_junk_tag;
		break;
	    } else if (t == context->unknown_tag) {
		if (TRACE)
		    fprintf(stderr, "SGML: *** Unknown element %s\n",
			    string->data);
		/*
		**  Fall through and treat like valid
		**  tag for attribute parsing. - KW
		*/
	    }
	    context->current_tag = t;

	    /*
	    **	Clear out attributes.
	    */
	    {
		int i;
		for (i = 0; i < context->current_tag->number_of_attributes; i++)
		    context->present[i] = NO;
	    }
	    string->size = 0;
	    context->current_attribute_number = INVALID;

	    if (c == '>') {
		if (context->current_tag->name)
		    start_element(context);
		context->state = S_text;
	    } else {
		context->state = S_tag_gap;
	    }
	}
	break;

    case S_exclamation:
	if (context->lead_exclamation && c == '-') {
	    /*
	    **	Set up for possible comment. - FM
	    */
	    context->lead_exclamation = FALSE;
	    context->first_dash = TRUE;
	    HTChunkPutc(string, c);
	    break;
	}
	if (context->lead_exclamation && c == '[') {
	    /*
	    **	Set up for possible marked section. - FM
	    */
	    context->lead_exclamation = FALSE;
	    context->first_bracket = TRUE;
	    context->second_bracket = FALSE;
	    HTChunkPutc(string, c);
	    context->state = S_marked;
	    break;
	}
	if (context->first_dash && c == '-') {
	    /*
	    **	Set up to handle comment. - FM
	    */
	    context->lead_exclamation = FALSE;
	    context->first_dash = FALSE;
	    context->end_comment = FALSE;
	    HTChunkPutc(string, c);
	    context->state = S_comment;
	    break;
	}
	context->lead_exclamation = FALSE;
	context->first_dash = FALSE;
	if (c == '>') {
	    /*
	    **	Try to handle identifier. - FM
	    */
	    HTChunkTerminate(string);
	    handle_identifier(context);
	    string->size = 0;
	    context->state = S_text;
	    break;
	}
	if (WHITE(c)) {
	    if (string->size == 8 &&
		!strncasecomp(string->data, "!DOCTYPE", 8)) {
		/*
		**  Set up for DOCTYPE declaration. - FM
		*/
		HTChunkPutc(string, c);
		context->doctype_bracket = FALSE;
		context->state = S_doctype;
		break;
	    }
	    if (string->size == 7 &&
		!strncasecomp(string->data, "!ENTITY", 7)) {
		/*
		**  Set up for ENTITY declaration. - FM
		*/
		HTChunkPutc(string, c);
		context->first_dash = FALSE;
		context->end_comment = TRUE;
		context->state = S_sgmlent;
		break;
	    }
	    if (string->size == 8 &&
		!strncasecomp(string->data, "!ELEMENT", 8)) {
		/*
		**  Set up for ELEMENT declaration. - FM
		*/
		HTChunkPutc(string, c);
		context->first_dash = FALSE;
		context->end_comment = TRUE;
		context->state = S_sgmlele;
		break;
	    }
	    if (string->size == 8 &&
		!strncasecomp(string->data, "!ATTLIST", 8)) {
		/*
		**  Set up for ATTLIST declaration. - FM
		*/
		HTChunkPutc(string, c);
		context->first_dash = FALSE;
		context->end_comment = TRUE;
		context->state = S_sgmlatt;
		break;
	    }
	}
	HTChunkPutc(string, c);
	break;

    case S_comment:		/* Expecting comment. - FM */
	if (historical_comments) {
	    /*
	    **	Any '>' terminates. - FM
	    */
	    if (c == '>') {
		HTChunkTerminate(string);
		handle_comment(context);
		string->size = 0;
		context->end_comment = FALSE;
		context->first_dash = FALSE;
		context->state = S_text;
		break;
	    }
	    HTChunkPutc(string, c);
	    break;
	}
	if (!context->first_dash && c == '-') {
	    HTChunkPutc(string, c);
	    context->first_dash = TRUE;
	    break;
	}
	if (context->first_dash && c == '-') {
	    HTChunkPutc(string, c);
	    context->first_dash = FALSE;
	    if (!context->end_comment)
		context->end_comment = TRUE;
	    else if (!minimal_comments)
		/*
		**  Validly treat '--' pairs as successive comments
		**  (for minimal, any "--WHITE>" terminates). - FM
		*/
		context->end_comment = FALSE;
	    break;
	}
	if (context->end_comment && c == '>') {
	    /*
	    **	Terminate and handle the comment. - FM
	    */
	    HTChunkTerminate(string);
	    handle_comment(context);
	    string->size = 0;
	    context->end_comment = FALSE;
	    context->first_dash = FALSE;
	    context->state = S_text;
	    break;
	}
	context->first_dash = FALSE;
	if (context->end_comment && !isspace(c))
	    context->end_comment = FALSE;
	HTChunkPutc(string, c);
	break;

    case S_doctype:		/* Expecting DOCTYPE. - FM */
	if (context->doctype_bracket) {
	    HTChunkPutc(string, c);
	    if (c == ']')
		context->doctype_bracket = FALSE;
	    break;
	}
	if (c == '[' && WHITE(string->data[string->size - 1])) {
	    HTChunkPutc(string, c);
	    context->doctype_bracket = TRUE;
	    break;
	}
	if (c == '>') {
	    HTChunkTerminate(string);
	    handle_doctype(context);
	    string->size = 0;
	    context->state = S_text;
	    break;
	}
	HTChunkPutc(string, c);
	break;

    case S_marked:		/* Expecting marked section. - FM */
	if (context->first_bracket && c == '[') {
	    HTChunkPutc(string, c);
	    context->first_bracket = FALSE;
	    context->second_bracket = TRUE;
	    break;
	}
	if (context->second_bracket && c == ']' &&
	    string->data[string->size - 1] == ']') {
	    HTChunkPutc(string, c);
	    context->second_bracket = FALSE;
	    break;
	}
	if (!context->second_bracket && c == '>') {
	    HTChunkTerminate(string);
	    handle_marked(context);
	    string->size = 0;
	    context->state = S_text;
	    break;
	}
	HTChunkPutc(string, c);
	break;

    case S_sgmlent:		/* Expecting ENTITY. - FM */
	if (!context->first_dash && c == '-') {
	    HTChunkPutc(string, c);
	    context->first_dash = TRUE;
	    break;
	}
	if (context->first_dash && c == '-') {
	    HTChunkPutc(string, c);
	    context->first_dash = FALSE;
	    if (!context->end_comment)
		context->end_comment = TRUE;
	    else
		context->end_comment = FALSE;
	    break;
	}
	if (context->end_comment && c == '>') {
	    HTChunkTerminate(string);
	    handle_sgmlent(context);
	    string->size = 0;
	    context->end_comment = FALSE;
	    context->first_dash = FALSE;
	    context->state = S_text;
	    break;
	}
	context->first_dash = FALSE;
	HTChunkPutc(string, c);
	break;

    case S_sgmlele:		/* Expecting ELEMENT. - FM */
	if (!context->first_dash && c == '-') {
	    HTChunkPutc(string, c);
	    context->first_dash = TRUE;
	    break;
	}
	if (context->first_dash && c == '-') {
	    HTChunkPutc(string, c);
	    context->first_dash = FALSE;
	    if (!context->end_comment)
		context->end_comment = TRUE;
	    else
		context->end_comment = FALSE;
	    break;
	}
	if (context->end_comment && c == '>') {
	    HTChunkTerminate(string);
	    handle_sgmlele(context);
	    string->size = 0;
	    context->end_comment = FALSE;
	    context->first_dash = FALSE;
	    context->state = S_text;
	    break;
	}
	context->first_dash = FALSE;
	HTChunkPutc(string, c);
	break;

    case S_sgmlatt:		/* Expecting ATTLIST. - FM */
	if (!context->first_dash && c == '-') {
	    HTChunkPutc(string, c);
	    context->first_dash = TRUE;
	    break;
	}
	if (context->first_dash && c == '-') {
	    HTChunkPutc(string, c);
	    context->first_dash = FALSE;
	    if (!context->end_comment)
		context->end_comment = TRUE;
	    else
		context->end_comment = FALSE;
	    break;
	}
	if (context->end_comment && c == '>') {
	    HTChunkTerminate(string);
	    handle_sgmlatt(context);
	    string->size = 0;
	    context->end_comment = FALSE;
	    context->first_dash = FALSE;
	    context->state = S_text;
	    break;
	}
	context->first_dash = FALSE;
	HTChunkPutc(string, c);
	break;

    case S_tag_gap:		/* Expecting attribute or '>' */
	if (WHITE(c))
	    break;		/* Gap between attributes */
	if (c == '>') { 	/* End of tag */
	    if (context->current_tag->name)
		start_element(context);
	    context->state = S_text;
	    break;
	}
	HTChunkPutc(string, c);
	context->state = S_attr; /* Get attribute */
	break;

				/* accumulating value */
    case S_attr:
	if (WHITE(c) || (c == '>') || (c == '=')) {	/* End of word */
	    HTChunkTerminate(string);
	    handle_attribute_name(context, string->data);
	    string->size = 0;
	    if (c == '>') {				/* End of tag */
		if (context->current_tag->name)
		    start_element(context);
		context->state = S_text;
		break;
	    }
	    context->state = (c == '=' ?  S_equals: S_attr_gap);
	} else {
	    HTChunkPutc(string, c);
	}
	break;

    case S_attr_gap:		/* Expecting attribute or '=' or '>' */
	if (WHITE(c))
	    break;		/* Gap after attribute */
	if (c == '>') { 	/* End of tag */
	    if (context->current_tag->name)
		start_element(context);
	    context->state = S_text;
	    break;
	} else if (c == '=') {
	    context->state = S_equals;
	    break;
	}
	HTChunkPutc(string, c);
	context->state = S_attr;		/* Get next attribute */
	break;

    case S_equals:		/* After attr = */
	if (WHITE(c))
	    break;		/* Before attribute value */
	if (c == '>') { 	/* End of tag */
	    if (TRACE)
		fprintf(stderr, "SGML: found = but no value\n");
	    if (context->current_tag->name)
		start_element(context);
	    context->state = S_text;
	    break;

	} else if (c == '\'') {
	    context->state = S_squoted;
	    break;

	} else if (c == '"') {
	    context->state = S_dquoted;
	    break;
	}
	HTChunkPutc(string, c);
	context->state = S_value;
	break;

    case S_value:
	if (WHITE(c) || (c == '>')) {		/* End of word */
	    HTChunkTerminate(string) ;
	    handle_attribute_value(context, string->data);
	    string->size = 0;
	    if (c == '>') {		/* End of tag */
		if (context->current_tag->name)
		    start_element(context);
		context->state = S_text;
		break;
	    }
	    else context->state = S_tag_gap;
	} else if (context->T.decode_utf8 &&
		   *context->utf_buf) {
	    HTChunkPuts(string, context->utf_buf);
	    context->utf_buf_p = context->utf_buf;
	    *(context->utf_buf_p) = '\0';
	} else if (HTCJK == NOCJK &&
		   (context->T.output_utf8 ||
		    context->T.trans_from_uni)) {
	    if (clong == 0xfffd && saved_char_in &&
		HTPassEightBitRaw &&
		(unsigned char)saved_char_in >=
		LYlowest_eightbit[context->outUCLYhndl]) {
		HTChunkPutUtf8Char(string,
				   (0xf000 | (unsigned char)saved_char_in));
	    } else {
		HTChunkPutUtf8Char(string, clong);
	    }
	} else if (saved_char_in && context->T.use_raw_char_in) {
	    HTChunkPutc(string, saved_char_in);
	} else {
	    HTChunkPutc(string, c);
	}
	break;

    case S_squoted:		/* Quoted attribute value */
	if (c == '\'') {	/* End of attribute value */
	    HTChunkTerminate(string) ;
	    handle_attribute_value(context, string->data);
	    string->size = 0;
	    context->state = S_tag_gap;
	} else if (c == '\033') {
	    /*
	    **	Setting up for possible single quotes in CJK escape
	    **	sequences. - Takuya ASADA (asada@three-a.co.jp)
	    */
	    context->state = S_esc_sq;
	    HTChunkPutc(string, c);
	} else if (context->T.decode_utf8 &&
		   *context->utf_buf) {
	    HTChunkPuts(string, context->utf_buf);
	    context->utf_buf_p = context->utf_buf;
	    *(context->utf_buf_p) = '\0';
	} else if (HTCJK == NOCJK &&
		   (context->T.output_utf8 ||
		    context->T.trans_from_uni)) {
	    if (clong == 0xfffd && saved_char_in &&
		HTPassEightBitRaw &&
		(unsigned char)saved_char_in >=
		LYlowest_eightbit[context->outUCLYhndl]) {
		HTChunkPutUtf8Char(string,
				   (0xf000 | (unsigned char)saved_char_in));
	    } else {
		HTChunkPutUtf8Char(string, clong);
	    }
	} else if (saved_char_in && context->T.use_raw_char_in) {
	    HTChunkPutc(string, saved_char_in);
	} else {
	    HTChunkPutc(string, c);
	}
	break;

    case S_dquoted:		/* Quoted attribute value */
	if (c == '"' || 	/* Valid end of attribute value */
	    (soft_dquotes &&	/*  If emulating old Netscape bug, treat '>' */
	     c == '>')) {	/*  as a co-terminator of dquoted and tag    */
	    HTChunkTerminate(string) ;
	    handle_attribute_value(context, string->data);
	    string->size = 0;
	    context->state = S_tag_gap;
	    if (c == '>')	/* We emulated the Netscape bug, so we go  */
		goto top1;	/* back and treat it as the tag terminator */
	} else if (c == '\033') {
	    /*
	    **	Setting up for possible double quotes in CJK escape
	    **	sequences. - Takuya ASADA (asada@three-a.co.jp)
	    */
	    context->state = S_esc_dq;
	    HTChunkPutc(string, c);
	} else if (context->T.decode_utf8 &&
		   *context->utf_buf) {
	    HTChunkPuts(string, context->utf_buf);
	    context->utf_buf_p = context->utf_buf;
	    *(context->utf_buf_p) = '\0';
	} else if (HTCJK == NOCJK &&
		   (context->T.output_utf8 ||
		    context->T.trans_from_uni)) {
	    if (clong == 0xfffd && saved_char_in &&
		HTPassEightBitRaw &&
		(unsigned char)saved_char_in >=
		LYlowest_eightbit[context->outUCLYhndl]) {
		HTChunkPutUtf8Char(string,
				   (0xf000 | (unsigned char)saved_char_in));
	    } else {
		HTChunkPutUtf8Char(string, clong);
	    }
	} else if (saved_char_in && context->T.use_raw_char_in) {
	    HTChunkPutc(string, saved_char_in);
	} else {
	    HTChunkPutc(string, c);
	}
	break;

    case S_end: 				/* </ */
	if (unsign_c < 127 && isalnum((unsigned char)c)) {
	    HTChunkPutc(string, c);
	} else {				/* End of end tag name */
	    HTTag * t = 0;

	    HTChunkTerminate(string);
	    if (!*string->data) {	/* Empty end tag */
		if (context->element_stack)
		    t = context->element_stack->tag;
	    } else {
		t = SGMLFindTag(dtd, string->data);
	    }
	    if (!t || t == context->unknown_tag) {
		if (TRACE)
		    fprintf(stderr, "Unknown end tag </%s>\n", string->data);
	    } else {
		BOOL tag_OK = (c == '>' || WHITE(c));
		context->current_tag = t;
#ifdef EXTENDED_HTMLDTD
		/*
		**  Just handle ALL end tags normally :-) - kw
		*/
		if (New_DTD) {
		    end_element( context, context->current_tag);
		} else
#endif /* EXTENDED_HTMLDTD */
		if (tag_OK &&
		    (!strcasecomp(string->data, "DD") ||
		     !strcasecomp(string->data, "DT") ||
		     !strcasecomp(string->data, "LI") ||
		     !strcasecomp(string->data, "LH") ||
		     !strcasecomp(string->data, "TD") ||
		     !strcasecomp(string->data, "TH") ||
		     !strcasecomp(string->data, "TR") ||
		     !strcasecomp(string->data, "THEAD") ||
		     !strcasecomp(string->data, "TFOOT") ||
		     !strcasecomp(string->data, "TBODY") ||
		     !strcasecomp(string->data, "COLGROUP"))) {
		    /*
		    **	Don't treat these end tags as invalid,
		    **	nor act on them. - FM
		    */
		    if (TRACE)
			fprintf(stderr,
				"SGML: `</%s%c' found!  Ignoring it.\n",
				string->data, c);
		    string->size = 0;
		    context->current_attribute_number = INVALID;
		    if (c != '>') {
			context->state = S_junk_tag;
		    } else {
			context->state = S_text;
		    }
		    break;
		} else if (tag_OK &&
			   (!strcasecomp(string->data, "A") ||
			    !strcasecomp(string->data, "B") ||
			    !strcasecomp(string->data, "BLINK") ||
			    !strcasecomp(string->data, "CITE") ||
			    !strcasecomp(string->data, "EM") ||
			    !strcasecomp(string->data, "FONT") ||
			    !strcasecomp(string->data, "FORM") ||
			    !strcasecomp(string->data, "I") ||
			    !strcasecomp(string->data, "P") ||
			    !strcasecomp(string->data, "STRONG") ||
			    !strcasecomp(string->data, "TT") ||
			    !strcasecomp(string->data, "U"))) {
		    /*
		    **	Handle end tags for container elements declared
		    **	as SGML_EMPTY to prevent "expected tag substitution"
		    **	but still processed via HTML_end_element() in HTML.c
		    **	with checks there to avoid throwing the HTML.c stack
		    **	out of whack (Ugh, what a hack! 8-). - FM
		    */
		    if (context->inSELECT) {
			/*
			**  We are in a SELECT block. - FM
			*/
			if (strcasecomp(string->data, "FORM")) {
			    /*
			    **	It is not at FORM end tag, so ignore it. - FM
			    */
			    if (TRACE) {
				fprintf(stderr,
			    "SGML: Ignoring end tag </%s> in SELECT block.\n",
					string->data);
			    }
			} else {
			    /*
			    **	End the SELECT block and then
			    **	handle the FORM end tag. - FM
			    */
			    if (TRACE) {
				fprintf(stderr,
			"SGML: Faking SELECT end tag before </%s> end tag.\n",
					string->data);
			    }
			    end_element(context,
					SGMLFindTag(context->dtd, "SELECT"));
			    if (TRACE) {
				fprintf(stderr,
					"SGML: End </%s>\n", string->data);
			    }
			    (*context->actions->end_element)
				(context->target,
				 (context->current_tag - context->dtd->tags),
				 (char **)&context->include);
			}
		    } else if (!strcasecomp(string->data, "P")) {
			/*
			**  Treat a P end tag like a P start tag (Ugh,
			**  what a hack! 8-). - FM
			*/
			if (TRACE)
			    fprintf(stderr,
				    "SGML: `</%s%c' found!  Treating as '<%s%c'.\n",
				    string->data, c, string->data, c);
			{
			    int i;
			    for (i = 0;
				 i < context->current_tag->number_of_attributes;
				 i++) {
				context->present[i] = NO;
			    }
			}
			if (context->current_tag->name)
			    start_element(context);
		    } else {
			if (TRACE) {
			    fprintf(stderr,
				    "SGML: End </%s>\n", string->data);
			}
			(*context->actions->end_element)
			    (context->target,
			     (context->current_tag - context->dtd->tags),
			     (char **)&context->include);
		    }
		    string->size = 0;
		    context->current_attribute_number = INVALID;
		    if (c != '>') {
			context->state = S_junk_tag;
		    } else {
			context->state = S_text;
		    }
		    break;
		} else {
		    /*
		    **	Handle all other end tags normally. - FM
		    */
		    end_element( context, context->current_tag);
		}
	    }

	    string->size = 0;
	    context->current_attribute_number = INVALID;
	    if (c != '>') {
		if (TRACE && !WHITE(c))
		    fprintf(stderr,"SGML: `</%s%c' found!\n", string->data, c);
		context->state = S_junk_tag;
	    } else {
		context->state = S_text;
	    }
	}
	break;


    case S_esc: 	/* Expecting '$'or '(' following CJK ESC. */
	if (c == '$') {
	    context->state = S_dollar;
	} else if (c == '(') {
	    context->state = S_paren;
	} else {
	    context->state = S_text;
	}
	PUTC(c);
	break;

    case S_dollar:	/* Expecting '@', 'B', 'A' or '(' after CJK "ESC$". */
	if (c == '@' || c == 'B' || c == 'A') {
	    context->state = S_nonascii_text;
	} else if (c == '(') {
	    context->state = S_dollar_paren;
	}
	PUTC(c);
	break;

    case S_dollar_paren: /* Expecting 'C' after CJK "ESC$(". */
	if (c == 'C') {
	    context->state = S_nonascii_text;
	} else {
	    context->state = S_text;
	}
	PUTC(c);
	break;

    case S_paren:	/* Expecting 'B', 'J', 'T' or 'I' after CJK "ESC(". */
	if (c == 'B' || c == 'J' || c == 'T') {
	    context->state = S_text;
	} else if (c == 'I') {
	    context->state = S_nonascii_text;
	} else {
	    context->state = S_text;
	}
	PUTC(c);
	break;

    case S_nonascii_text: /* Expecting CJK ESC after non-ASCII text. */
	if (c == '\033') {
	    context->state = S_esc;
	}
	PUTC(c);
	break;

    case S_esc_sq:	/* Expecting '$'or '(' following CJK ESC. */
	if (c == '$') {
	    context->state = S_dollar_sq;
	} else if (c == '(') {
	    context->state = S_paren_sq;
	} else {
	    context->state = S_squoted;
	}
	HTChunkPutc(string, c);
	break;

    case S_dollar_sq:	/* Expecting '@', 'B', 'A' or '(' after CJK "ESC$". */
	if (c == '@' || c == 'B' || c == 'A') {
	    context->state = S_nonascii_text_sq;
	} else if (c == '(') {
	    context->state = S_dollar_paren_sq;
	}
	HTChunkPutc(string, c);
	break;

    case S_dollar_paren_sq: /* Expecting 'C' after CJK "ESC$(". */
	if (c == 'C') {
	    context->state = S_nonascii_text_sq;
	} else {
	    context->state = S_squoted;
	}
	HTChunkPutc(string, c);
	break;

    case S_paren_sq:	/* Expecting 'B', 'J', 'T' or 'I' after CJK "ESC(". */
	if (c == 'B' || c == 'J' || c == 'T') {
	    context->state = S_squoted;
	} else if (c == 'I') {
	    context->state = S_nonascii_text_sq;
	} else {
	    context->state = S_squoted;
	}
	HTChunkPutc(string, c);
	break;

    case S_nonascii_text_sq: /* Expecting CJK ESC after non-ASCII text. */
	if (c == '\033') {
	    context->state = S_esc_sq;
	}
	HTChunkPutc(string, c);
	break;

    case S_esc_dq:		/* Expecting '$'or '(' following CJK ESC. */
	if (c == '$') {
	    context->state = S_dollar_dq;
	} else if (c == '(') {
	    context->state = S_paren_dq;
	} else {
	    context->state = S_dquoted;
	}
	HTChunkPutc(string, c);
	break;

    case S_dollar_dq:	/* Expecting '@', 'B', 'A' or '(' after CJK "ESC$". */
	if (c == '@' || c == 'B' || c == 'A') {
	    context->state = S_nonascii_text_dq;
	} else if (c == '(') {
	    context->state = S_dollar_paren_dq;
	}
	HTChunkPutc(string, c);
	break;

    case S_dollar_paren_dq: /* Expecting 'C' after CJK "ESC$(". */
	if (c == 'C') {
	    context->state = S_nonascii_text_dq;
	} else {
	    context->state = S_dquoted;
	}
	HTChunkPutc(string, c);
	break;

    case S_paren_dq:	/* Expecting 'B', 'J', 'T' or 'I' after CJK "ESC(". */
	if (c == 'B' || c == 'J' || c == 'T') {
	    context->state = S_dquoted;
	} else if (c == 'I') {
	    context->state = S_nonascii_text_dq;
	} else {
	    context->state = S_dquoted;
	}
	HTChunkPutc(string, c);
	break;

    case S_nonascii_text_dq: /* Expecting CJK ESC after non-ASCII text. */
	if (c == '\033') {
	    context->state = S_esc_dq;
	}
	HTChunkPutc(string, c);
	break;

    case S_junk_tag:
	if (c == '>') {
	    context->state = S_text;
	}
    } /* switch on context->state */

    /*
    **	Check whether we've added anything to the recover buffer. - FM
    */
    if (context->recover != NULL) {
	if (context->recover[context->recover_index] == '\0') {
	    FREE(context->recover);
	    context->recover_index = 0;
	} else {
	    c = context->recover[context->recover_index];
	    context->recover_index++;
	    goto top;
	}
    }

    /*
    **	Check whether an external function has added
    **	anything to the include buffer. - FM
    */
    if (context->include != NULL) {
	if (context->include[context->include_index] == '\0') {
	    FREE(context->include);
	    context->include_index = 0;
	} else {
	    c = context->include[context->include_index];
	    context->include_index++;
	    goto top;
	}
    }

    /*
    **	Check whether an external function has added
    **	anything to the csi buffer. - FM
    */
    if (context->csi != NULL) {
	if (context->csi[context->csi_index] == '\0') {
	    FREE(context->csi);
	    context->csi_index = 0;
	} else {
	    c = context->csi[context->csi_index];
	    context->csi_index++;
	    goto top;
	}
    }
}  /* SGML_character */


PRIVATE void SGML_string ARGS2(
	HTStream *,	context,
	CONST char*,	str)
{
    CONST char *p;
    for (p = str; *p; p++)
	SGML_character(context, *p);
}


PRIVATE void SGML_write ARGS3(
	HTStream *,	context,
	CONST char*,	str,
	int,		l)
{
    CONST char *p;
    CONST char *e = str+l;
    for (p = str; p < e; p++)
	SGML_character(context, *p);
}

/*_______________________________________________________________________
*/

/*	Structured Object Class
**	-----------------------
*/
PUBLIC CONST HTStreamClass SGMLParser =
{
	"SGMLParser",
	SGML_free,
	SGML_abort,
	SGML_character,
	SGML_string,
	SGML_write,
};

/*	Create SGML Engine
**	------------------
**
** On entry,
**	dtd		represents the DTD, along with
**	actions 	is the sink for the data as a set of routines.
**
*/

PUBLIC HTStream* SGML_new  ARGS3(
	CONST SGML_dtd *,	dtd,
	HTParentAnchor *,	anchor,
	HTStructured *, 	target)
{
    int i;
    HTStream* context = (HTStream *) malloc(sizeof(*context));
    if (!context)
	outofmem(__FILE__, "SGML_begin");

    context->isa = &SGMLParser;
    context->string = HTChunkCreate(128);	/* Grow by this much */
    context->dtd = dtd;
    context->target = target;
    context->actions = (HTStructuredClass*)(((HTStream*)target)->isa);
					/* Ugh: no OO */
    context->unknown_tag = &HTTag_unrecognized;
    context->state = S_text;
    context->element_stack = 0; 		/* empty */
    context->inSELECT = FALSE;
#ifdef CALLERDATA
    context->callerData = (void*) callerData;
#endif /* CALLERDATA */
    for (i = 0; i < MAX_ATTRIBUTES; i++)
	context->value[i] = 0;

    context->lead_exclamation = FALSE;
    context->first_dash = FALSE;
    context->end_comment = FALSE;
    context->doctype_bracket = FALSE;
    context->first_bracket = FALSE;
    context->second_bracket = FALSE;
    context->isHex = FALSE;

    context->node_anchor = anchor; /* Could be NULL? */
    context->utf_count = 0;
    context->utf_char = 0;
    context->utf_buf[0] = context->utf_buf[6] = '\0';
    context->utf_buf_p = context->utf_buf;
    UCTransParams_clear(&context->T);
    context->inUCLYhndl = HTAnchor_getUCLYhndl(anchor,
					       UCT_STAGE_PARSER);
    if (context->inUCLYhndl < 0) {
	HTAnchor_copyUCInfoStage(anchor,
				 UCT_STAGE_PARSER,
				 UCT_STAGE_MIME,
				 -1);
	context->inUCLYhndl = HTAnchor_getUCLYhndl(anchor,
						   UCT_STAGE_PARSER);
    }
    context->inUCI = HTAnchor_getUCInfoStage(anchor,
					     UCT_STAGE_PARSER);
    set_chartrans_handling(context, anchor, -1);

    context->recover = NULL;
    context->recover_index = 0;
    context->include = NULL;
    context->include_index = 0;
    context->url = NULL;
    context->csi = NULL;
    context->csi_index = 0;

    return context;
}

/*		Asian character conversion functions
**		====================================
**
**	Added 24-Mar-96 by FM, based on:
**
////////////////////////////////////////////////////////////////////////
Copyright (c) 1993 Electrotechnical Laboratry (ETL)

Permission to use, copy, modify, and distribute this material
for any purpose and without fee is hereby granted, provided
that the above copyright notice and this permission notice
appear in all copies, and that the name of ETL not be
used in advertising or publicity pertaining to this
material without the specific, prior written permission
of an authorized representative of ETL.
ETL MAKES NO REPRESENTATIONS ABOUT THE ACCURACY OR SUITABILITY
OF THIS MATERIAL FOR ANY PURPOSE.  IT IS PROVIDED "AS IS",
WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES.
/////////////////////////////////////////////////////////////////////////
Content-Type:	program/C; charset=US-ASCII
Program:	SJIS.c
Author: 	Yutaka Sato <ysato@etl.go.jp>
Description:
History:
	930923	extracted from codeconv.c of cosmos
///////////////////////////////////////////////////////////////////////
*/

PUBLIC int TREAT_SJIS = 1;

PUBLIC void JISx0201TO0208_EUC ARGS4(
	register unsigned char, 	IHI,
	register unsigned char, 	ILO,
	register unsigned char *,	OHI,
	register unsigned char *,	OLO)
{
    static char *table[] = {
	"\xA1\xA3", "\xA1\xD6", "\xA1\xD7", "\xA1\xA2", "\xA1\xA6", "\xA5\xF2",
	"\xA5\xA1", "\xA5\xA3", "\xA5\xA5", "\xA5\xA7", "\xA5\xA9",
	"\xA5\xE3", "\xA5\xE5", "\xA5\xE7", "\xA5\xC3", "\xA1\xBC",
	"\xA5\xA2", "\xA5\xA4", "\xA5\xA6", "\xA5\xA8", "\xA5\xAA",
	"\xA5\xAB", "\xA5\xAD", "\xA5\xAF", "\xA5\xB1", "\xA5\xB3",
	"\xA5\xB5", "\xA5\xB7", "\xA5\xB9", "\xA5\xBB", "\xA5\xBD",
	"\xA5\xBF", "\xA5\xC1", "\xA5\xC4", "\xA5\xC6", "\xA5\xC8",
	"\xA5\xCA", "\xA5\xCB", "\xA5\xCC", "\xA5\xCD", "\xA5\xCE",
	"\xA5\xCF", "\xA5\xD2", "\xA5\xD5", "\xA5\xD8", "\xA5\xDB",
	"\xA5\xDE", "\xA5\xDF", "\xA5\xE0", "\xA5\xE1", "\xA5\xE2",
	"\xA5\xE4", "\xA5\xE6", "\xA5\xE8", "\xA5\xE9", "\xA5\xEA",
	"\xA5\xEB", "\xA5\xEC", "\xA5\xED", "\xA5\xEF", "\xA5\xF3",
	"\xA1\xAB", "\xA1\xAC"
    };

    if ((IHI == 0x8E) && (ILO >= 0xA1) && (ILO <= 0xDF)) {
	*OHI = table[ILO - 0xA1][0];
	*OLO = table[ILO - 0xA1][1];
    } else {
	*OHI = IHI;
	*OLO = ILO;
    }
}

PUBLIC unsigned char * SJIS_TO_JIS1 ARGS3(
	register unsigned char, 	HI,
	register unsigned char, 	LO,
	register unsigned char *,	JCODE)
{
    HI -= (HI <= 0x9F) ? 0x71 : 0xB1;
    HI = (HI << 1) + 1;
    if (0x7F < LO)
	LO--;
    if (0x9E <= LO) {
	LO -= 0x7D;
	HI++;
    } else {
	LO -= 0x1F;
    }
    JCODE[0] = HI;
    JCODE[1] = LO;
    return JCODE;
}

PUBLIC unsigned char * JIS_TO_SJIS1 ARGS3(
	register unsigned char, 	HI,
	register unsigned char, 	LO,
	register unsigned char *,	SJCODE)
{
    if (HI & 1)
	LO += 0x1F;
    else
	LO += 0x7D;
    if (0x7F <= LO)
	LO++;

    HI = ((HI - 0x21) >> 1) + 0x81;
    if (0x9F < HI)
	HI += 0x40;
    SJCODE[0] = HI;
    SJCODE[1] = LO;
    return SJCODE;
}

PUBLIC unsigned char * EUC_TO_SJIS1 ARGS3(
	unsigned char,			HI,
	unsigned char,			LO,
	register unsigned char *,	SJCODE)
{
    if (HI == 0x8E) JISx0201TO0208_EUC(HI, LO, &HI, &LO);
    JIS_TO_SJIS1(HI&0x7F, LO&0x7F, SJCODE);
    return SJCODE;
}

PUBLIC void JISx0201TO0208_SJIS ARGS3(
	register unsigned char, 	I,
	register unsigned char *,	OHI,
	register unsigned char *,	OLO)
{
    unsigned char SJCODE[2];

    JISx0201TO0208_EUC('\x8E', I, OHI, OLO);
    JIS_TO_SJIS1(*OHI&0x7F, *OLO&0x7F, SJCODE);
    *OHI = SJCODE[0];
    *OLO = SJCODE[1];
}

PUBLIC unsigned char * SJIS_TO_EUC1 ARGS3(
	unsigned char,		HI,
	unsigned char,		LO,
	unsigned char *,	data)
{
    SJIS_TO_JIS1(HI, LO, data);
    data[0] |= 0x80;
    data[1] |= 0x80;
    return data;
}

PUBLIC unsigned char * SJIS_TO_EUC ARGS2(
	unsigned char *,	src,
	unsigned char *,	dst)
{
    register unsigned char hi, lo, *sp, *dp;
    register int in_sjis = 0;

    for (sp = src, dp = dst; (0 != (hi = sp[0]));) {
	lo = sp[1];
	if (TREAT_SJIS && IS_SJIS(hi, lo, in_sjis)) {
	    SJIS_TO_JIS1(hi,lo,dp);
	    dp[0] |= 0x80;
	    dp[1] |= 0x80;
	    dp += 2;
	    sp += 2;
	} else {
	    *dp++ = *sp++;
	}
    }
    *dp = 0;
    return dst;
}

PUBLIC unsigned char * EUC_TO_SJIS ARGS2(
	unsigned char *,	src,
	unsigned char *,	dst)
{
    register unsigned char *sp, *dp;

    for (sp = src, dp = dst; *sp;) {
	if (*sp & 0x80) {
	    if (sp[1] && (sp[1] & 0x80)) {
		JIS_TO_SJIS1(sp[0]&0x7F, sp[1]&0x7F, dp);
		dp += 2;
		sp += 2;
	    } else {
		sp++;
	    }
	} else {
	    *dp++ = *sp++;
	}
    }
    *dp = 0;
    return dst;
}

PUBLIC unsigned char * EUC_TO_JIS ARGS4(
	unsigned char *,	src,
	unsigned char *,	dst,
	CONST char *,		toK,
	CONST char *,		toA)
{
    register unsigned char kana_mode = 0;
    register unsigned char cch;
    register unsigned char *sp = src;
    register unsigned char *dp = dst;
    register int i;

    while (0 != (cch = *sp++)) {
	if (cch & 0x80) {
	    if (!kana_mode) {
		kana_mode = ~kana_mode;
		for (i = 0; toK[i]; i++) {
		    *dp++ = (unsigned char)toK[i];
		}
	    }
	    if (*sp & 0x80) {
		*dp++ = cch & ~0x80;
		*dp++ = *sp++ & ~0x80;
	    }
	} else {
	    if (kana_mode) {
		kana_mode = ~kana_mode;
		for (i = 0; toA[i]; i++) {
		    *dp++ = (unsigned char)toA[i];
		    *dp = '\0';
		}
	    }
	    *dp++ = cch;
	}
    }
    if (kana_mode) {
	for (i = 0; toA[i]; i++) {
	    *dp++ = (unsigned char)toA[i];
	}
    }

    if (dp)
	*dp = 0;
    return dst;
}

PUBLIC unsigned char * TO_EUC ARGS2(
	unsigned char *,	jis,
	unsigned char *,	euc)
{
    register unsigned char *s, *d, c, jis_stat;
    register int to1B, to2B;
    register int in_sjis = 0;

    s = jis;
    d = euc;
    jis_stat = 0;
    to2B = TO_2BCODE;
    to1B = TO_1BCODE;

    while (0 != (c = *s++)) {
	if (c == ESC) {
	    if (*s == to2B) {
		if ((s[1] == 'B') || (s[1] == '@') || (s[1] == 'A')) {
		    jis_stat = 0x80;
		    s += 2;
		    continue;
		} else if ((s[1] == '(') && s[2] && (s[2] == 'C')) {
		    jis_stat = 0x80;
		    s += 3;
		    continue;
		}
	    } else {
		if (*s == to1B) {
		    if ((s[1]=='B') || (s[1]=='J') ||
			(s[1]=='H') || (s[1]=='T')) {
			jis_stat = 0;
			s += 2;
			continue;
		    }
		}
	    }
	}
	if (IS_SJIS(c,*s,in_sjis)) {
	    SJIS_TO_EUC1(c, *s, d);
	    d += 2;
	    s++;
	} else {
	    if (jis_stat && (0x20 < c)) {
		*d++ = jis_stat | c;
	    } else {
		*d++ = c;
	    }
	}
    }
    *d = 0;
    return euc;
}

PUBLIC void TO_SJIS ARGS2(
	unsigned char *,	any,
	unsigned char *,	sjis)
{
    unsigned char *euc;

    if (!any || !sjis)
       return;

    euc = (unsigned char*)malloc(strlen((CONST char *)any)+1);
    if (euc == NULL)
	outofmem(__FILE__, "TO_SJIS");

    TO_EUC(any, euc);
    EUC_TO_SJIS(euc, sjis);
    FREE(euc);
}

PUBLIC void TO_JIS ARGS2(
	unsigned char *,	any,
	unsigned char *,	jis)
{
    unsigned char *euc;

    if (!any || !jis)
       return;

    euc = (unsigned char*)malloc(strlen((CONST char *)any)+1);
    if (euc == NULL)
	outofmem(__FILE__, "TO_JIS");

    TO_EUC(any, euc);
    EUC_TO_JIS(euc, jis, TO_KANJI, TO_ASCII);
    FREE(euc);
}
