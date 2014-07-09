/*
 * $LynxId: SGML.c,v 1.148 2012/02/10 18:32:26 tom Exp $
 *
 *			General SGML Parser code		SGML.c
 *			========================
 *
 *	This module implements an HTStream object.  To parse an
 *	SGML file, create this object which is a parser.  The object
 *	is (currently) created by being passed a DTD structure,
 *	and a target HTStructured object at which to throw the parsed stuff.
 *
 *	 6 Feb 93  Binary searches used. Interface modified.
 */

#define HTSTREAM_INTERNAL 1

#include <HTUtils.h>

#include <SGML.h>
#include <HTMLDTD.h>
#include <HTAccess.h>
#include <HTCJK.h>		/* FIXME: this doesn't belong in SGML.c */
#include <UCMap.h>
#include <UCDefs.h>
#include <UCAux.h>

#include <HTChunk.h>
#include <HTUtils.h>

#include <LYCharSets.h>
#include <LYCharVals.h>		/* S/390 -- gil -- 0635 */
#include <LYGlobalDefs.h>
#include <LYStrings.h>
#include <LYLeaks.h>
#include <LYUtils.h>

#ifdef USE_COLOR_STYLE
# include <LYStyle.h>
#endif
#ifdef USE_PRETTYSRC
# include <LYPrettySrc.h>
#endif

#define AssumeCP1252(context) \
	(((context)->inUCLYhndl == LATIN1 \
	  || (context)->inUCLYhndl == US_ASCII) \
	 && html5_charsets)

#define INVALID (-1)

static int sgml_offset;

#ifdef USE_PRETTYSRC

static char *entity_string;	/* this is used for printing entity name.

				   Unconditionally added since redundant assigments don't hurt much */

static void fake_put_character(void *p GCC_UNUSED,
			       char c GCC_UNUSED)
{
}

#define START TRUE
#define STOP FALSE

#define PUTS_TR(x) psrc_convert_string = TRUE; PUTS(x)

#endif

/* my_casecomp() - optimized by the first character, NOT_ASCII ok */
#define my_casecomp(a,b)  ((TOUPPER(*a) == TOUPPER(*b)) ? \
			AS_casecomp(a,b) : \
			(TOASCII(TOUPPER(*a)) - TOASCII(TOUPPER(*b))))

 /* will use partially inlined version */
#define orig_HTChunkPutUtf8Char HTChunkPutUtf8Char
#undef HTChunkPutUtf8Char

/* ...used for comments and attributes value like href... */
#define HTChunkPutUtf8Char(ch,x) \
    { \
    if ((TOASCII(x) < 128)  && (ch->size < ch->allocated)) \
	ch->data[ch->size++] = (char)x; \
    else \
	orig_HTChunkPutUtf8Char(ch,x); \
    }

#define PUTS(str) ((*context->actions->put_string)(context->target, str))
#define PUTC(ch)  ((*context->actions->put_character)(context->target, (char) ch))
#define PUTUTF8(code) (UCPutUtf8_charstring((HTStream *)context->target, \
		      (putc_func_t*)(context->actions->put_character), code))

#ifdef USE_PRETTYSRC
#define PRETTYSRC_PUTC(c) if (psrc_view) PUTC(c)
#else
#define PRETTYSRC_PUTC(c)	/* nothing */
#endif

/*the following macros are used for pretty source view. */
#define IS_C(attr) (attr.type == HTMLA_CLASS)

HTCJKlang HTCJK = NOCJK;	/* CJK enum value.              */
BOOL HTPassEightBitRaw = FALSE;	/* Pass 161-172,174-255 raw.    */
BOOL HTPassEightBitNum = FALSE;	/* Pass ^ numeric entities raw. */
BOOL HTPassHighCtrlRaw = FALSE;	/* Pass 127-160,173,&#127; raw. */
BOOL HTPassHighCtrlNum = FALSE;	/* Pass &#128;-&#159; raw.      */

/*	The State (context) of the parser
 *
 *	This is passed with each call to make the parser reentrant
 *
 */

#define MAX_ATTRIBUTES 36	/* Max number of attributes per element */

/*		Element Stack
 *		-------------
 *	This allows us to return down the stack reselecting styles.
 *	As we return, attribute values will be garbage in general.
 */
typedef struct _HTElement HTElement;
struct _HTElement {
    HTElement *next;		/* Previously nested element or 0 */
    HTTag *tag;			/* The tag at this level  */
};

typedef enum {
    S_text = 0
    ,S_attr
    ,S_attr_gap
    ,S_comment
    ,S_cro
    ,S_doctype
    ,S_dollar
    ,S_dollar_dq
    ,S_dollar_paren
    ,S_dollar_paren_dq
    ,S_dollar_paren_sq
    ,S_dollar_sq
    ,S_dquoted
    ,S_end
    ,S_entity
    ,S_equals
    ,S_ero
    ,S_esc
    ,S_esc_dq
    ,S_esc_sq
    ,S_exclamation
    ,S_in_kanji
    ,S_incro
    ,S_junk_tag
    ,S_litteral
    ,S_marked
    ,S_nonascii_text
    ,S_nonascii_text_dq
    ,S_nonascii_text_sq
    ,S_paren
    ,S_paren_dq
    ,S_paren_sq
    ,S_pcdata
    ,S_pi
    ,S_script
    ,S_sgmlatt
    ,S_sgmlele
    ,S_sgmlent
    ,S_squoted
    ,S_tag
    ,S_tag_gap
    ,S_tagname_slash
    ,S_value
} sgml_state;

/*	Internal Context Data Structure
 *	-------------------------------
 */
struct _HTStream {

    const HTStreamClass *isa;	/* inherited from HTStream */

    const SGML_dtd *dtd;
    const HTStructuredClass *actions;	/* target class  */
    HTStructured *target;	/* target object */

    HTTag *current_tag;
    HTTag *slashedtag;
    const HTTag *unknown_tag;
    BOOL extended_html;		/* xhtml */
    BOOL strict_xml;		/* xml */
    BOOL inSELECT;
    BOOL no_lynx_specialcodes;
    int current_attribute_number;
    HTChunk *string;
    int leading_spaces;
    int trailing_spaces;
    HTElement *element_stack;
    sgml_state state;
    unsigned char kanji_buf;
#ifdef CALLERDATA
    void *callerData;
#endif				/* CALLERDATA */
    BOOL present[MAX_ATTRIBUTES];	/* Flags: attribute is present? */
    char *value[MAX_ATTRIBUTES];	/* NULL, or strings alloc'd with StrAllocCopy_extra() */

    BOOL lead_exclamation;
    BOOL first_dash;
    BOOL end_comment;
    BOOL doctype_bracket;
    BOOL first_bracket;
    BOOL second_bracket;
    BOOL isHex;

    HTParentAnchor *node_anchor;
    LYUCcharset *inUCI;		/* pointer to anchor UCInfo */
    int inUCLYhndl;		/* charset we are fed       */
    LYUCcharset *outUCI;	/* anchor UCInfo for target */
    int outUCLYhndl;		/* charset for target       */
    char utf_count;
    UCode_t utf_char;
    char utf_buf[8];
    char *utf_buf_p;
    UCTransParams T;
    int current_tag_charset;	/* charset to pass attributes */

    char *recover;
    int recover_index;
    char *include;
    char *active_include;
    int include_index;
    char *url;
    char *csi;
    int csi_index;
#ifdef USE_PRETTYSRC
    BOOL cur_attr_is_href;
    BOOL cur_attr_is_name;
#endif
};

#ifdef NO_LYNX_TRACE
#define state_name(n) "state"
#else
static const char *state_name(sgml_state n)
{
    const char *result = "?";
    /* *INDENT-OFF* */
    switch (n) {
    case S_attr:                result = "S_attr";              break;
    case S_attr_gap:            result = "S_attr_gap";          break;
    case S_comment:             result = "S_comment";           break;
    case S_cro:                 result = "S_cro";               break;
    case S_doctype:             result = "S_doctype";           break;
    case S_dollar:              result = "S_dollar";            break;
    case S_dollar_dq:           result = "S_dollar_dq";         break;
    case S_dollar_paren:        result = "S_dollar_paren";      break;
    case S_dollar_paren_dq:     result = "S_dollar_paren_dq";   break;
    case S_dollar_paren_sq:     result = "S_dollar_paren_sq";   break;
    case S_dollar_sq:           result = "S_dollar_sq";         break;
    case S_dquoted:             result = "S_dquoted";           break;
    case S_end:                 result = "S_end";               break;
    case S_entity:              result = "S_entity";            break;
    case S_equals:              result = "S_equals";            break;
    case S_ero:                 result = "S_ero";               break;
    case S_esc:                 result = "S_esc";               break;
    case S_esc_dq:              result = "S_esc_dq";            break;
    case S_esc_sq:              result = "S_esc_sq";            break;
    case S_exclamation:         result = "S_exclamation";       break;
    case S_in_kanji:            result = "S_in_kanji";          break;
    case S_incro:               result = "S_incro";             break;
    case S_pi:                  result = "S_pi";                break;
    case S_junk_tag:            result = "S_junk_tag";          break;
    case S_litteral:            result = "S_litteral";          break;
    case S_marked:              result = "S_marked";            break;
    case S_nonascii_text:       result = "S_nonascii_text";     break;
    case S_nonascii_text_dq:    result = "S_nonascii_text_dq";  break;
    case S_nonascii_text_sq:    result = "S_nonascii_text_sq";  break;
    case S_paren:               result = "S_paren";             break;
    case S_paren_dq:            result = "S_paren_dq";          break;
    case S_paren_sq:            result = "S_paren_sq";          break;
    case S_pcdata:              result = "S_pcdata";            break;
    case S_script:              result = "S_script";            break;
    case S_sgmlatt:             result = "S_sgmlatt";           break;
    case S_sgmlele:             result = "S_sgmlele";           break;
    case S_sgmlent:             result = "S_sgmlent";           break;
    case S_squoted:             result = "S_squoted";           break;
    case S_tag:                 result = "S_tag";               break;
    case S_tag_gap:             result = "S_tag_gap";           break;
    case S_tagname_slash:       result = "S_tagname_slash";     break;
    case S_text:                result = "S_text";              break;
    case S_value:               result = "S_value";             break;
    }
    /* *INDENT-ON* */

    return result;
}
#endif

/* storage for Element Stack */
#define DEPTH 10
static HTElement pool[DEPTH];
static int depth = 0;

static HTElement *pool_alloc(void)
{
    depth++;
    if (depth > DEPTH)
	return (HTElement *) malloc(sizeof(HTElement));
    return (pool + depth - 1);
}

static void pool_free(HTElement * e)
{
    if (depth > DEPTH)
	FREE(e);
    depth--;
    return;
}

#ifdef USE_PRETTYSRC

static void HTMLSRC_apply_markup(HTStream *context,
				 HTlexeme lexeme,
				 int start)
{
    HT_tagspec *ts = *((start ? lexeme_start : lexeme_end) + lexeme);

    while (ts) {
#ifdef USE_COLOR_STYLE
	if (ts->start) {
	    current_tag_style = ts->style;
	    force_current_tag_style = TRUE;
	    forced_classname = ts->class_name;
	    force_classname = TRUE;
	}
#endif
	CTRACE((tfp, ts->start ? "SRCSTART %d\n" : "SRCSTOP %d\n", (int) lexeme));
	if (ts->start)
	    (*context->actions->start_element) (context->target,
						(int) ts->element,
						ts->present,
						(STRING2PTR) ts->value,
						context->current_tag_charset,
						&context->include);
	else
	    (*context->actions->end_element) (context->target,
					      (int) ts->element,
					      &context->include);
	ts = ts->next;
    }
}

#define PSRCSTART(x)	HTMLSRC_apply_markup(context,HTL_##x,START)
#define PSRCSTOP(x)   HTMLSRC_apply_markup(context,HTL_##x,STOP)

#define attr_is_href context->cur_attr_is_href
#define attr_is_name context->cur_attr_is_name
#endif

static void set_chartrans_handling(HTStream *context,
				   HTParentAnchor *anchor,
				   int chndl)
{
    if (chndl < 0) {
	/*
	 * Nothing was set for the parser in earlier stages, so the HTML
	 * parser's UCLYhndl should still be its default.  - FM
	 */
	chndl = HTAnchor_getUCLYhndl(anchor, UCT_STAGE_STRUCTURED);
	if (chndl < 0)
	    /*
	     * That wasn't set either, so seek the HText default.  - FM
	     */
	    chndl = HTAnchor_getUCLYhndl(anchor, UCT_STAGE_HTEXT);
	if (chndl < 0)
	    /*
	     * That wasn't set either, so assume the current display character
	     * set.  - FM
	     */
	    chndl = current_char_set;
	/*
	 * Try to set the HText and HTML stages' chartrans info with the
	 * default lock level (will not be changed if it was set previously
	 * with a higher lock level).  - FM
	 */
	HTAnchor_setUCInfoStage(anchor, chndl,
				UCT_STAGE_HTEXT,
				UCT_SETBY_DEFAULT);
	HTAnchor_setUCInfoStage(anchor, chndl,
				UCT_STAGE_STRUCTURED,
				UCT_SETBY_DEFAULT);
	/*
	 * Get the chartrans info for output to the HTML parser.  - FM
	 */
	context->outUCI = HTAnchor_getUCInfoStage(anchor,
						  UCT_STAGE_STRUCTURED);
	context->outUCLYhndl = HTAnchor_getUCLYhndl(context->node_anchor,
						    UCT_STAGE_STRUCTURED);
    }
    /*
     * Set the in->out transformation parameters.  - FM
     */
    UCSetTransParams(&context->T,
		     context->inUCLYhndl, context->inUCI,
		     context->outUCLYhndl, context->outUCI);
    /*
     * This is intended for passing the SGML parser's input charset as an
     * argument in each call to the HTML parser's start tag function, but it
     * would be better to call a Lynx_HTML_parser function to set an element in
     * its HTStructured object, itself, if this were needed.  - FM
     */
#ifndef EXP_JAPANESEUTF8_SUPPORT
    if (IS_CJK_TTY) {
	context->current_tag_charset = -1;
    } else
#endif
    if (context->T.transp) {
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
	context->current_tag_charset = LATIN1;
    }
}

static void change_chartrans_handling(HTStream *context)
{
    int new_LYhndl = HTAnchor_getUCLYhndl(context->node_anchor,
					  UCT_STAGE_PARSER);

    if (new_LYhndl != context->inUCLYhndl &&
	new_LYhndl >= 0) {
	/*
	 * Something changed.  but ignore if a META wants an unknown charset.
	 */
	LYUCcharset *new_UCI = HTAnchor_getUCInfoStage(context->node_anchor,
						       UCT_STAGE_PARSER);

	if (new_UCI) {
	    LYUCcharset *next_UCI = HTAnchor_getUCInfoStage(context->node_anchor,
							    UCT_STAGE_STRUCTURED);
	    int next_LYhndl = HTAnchor_getUCLYhndl(context->node_anchor, UCT_STAGE_STRUCTURED);

	    context->inUCI = new_UCI;
	    context->inUCLYhndl = new_LYhndl;
	    context->outUCI = next_UCI;
	    context->outUCLYhndl = next_LYhndl;
	    set_chartrans_handling(context,
				   context->node_anchor, next_LYhndl);
	}
    }
}

#ifdef USE_COLOR_STYLE
#include <AttrList.h>
static int current_is_class = 0;
#endif

/*	Handle Attribute
 *	----------------
 */
/* PUBLIC const char * SGML_default = "";   ?? */

static void handle_attribute_name(HTStream *context, const char *s)
{
    HTTag *tag = context->current_tag;
    const attr *attributes = tag->attributes;
    int high, low, i, diff;

#ifdef USE_PRETTYSRC
    if (psrc_view) {
	attr_is_href = FALSE;
	attr_is_name = FALSE;
    }
#endif
    /*
     * Ignore unknown tag.  - KW
     */
    if (tag == context->unknown_tag) {
#ifdef USE_PRETTYSRC
	if (psrc_view)
	    context->current_attribute_number = 1;	/* anything !=INVALID */
#endif
	return;
    }

    /*
     * Binary search for attribute name.
     */
    for (low = 0, high = tag->number_of_attributes;
	 high > low;
	 diff < 0 ? (low = i + 1) : (high = i)) {
	i = (low + (high - low) / 2);
	diff = my_casecomp(attributes[i].name, s);
	if (diff == 0) {	/* success: found it */
	    context->current_attribute_number = i;
#ifdef USE_PRETTYSRC
	    if (psrc_view) {
		attr_is_name = (BOOL) (attributes[i].type == HTMLA_ANAME);
		attr_is_href = (BOOL) (attributes[i].type == HTMLA_HREF);
	    } else
#endif
	    {
		context->present[i] = YES;
		Clear_extra(context->value[i]);
#ifdef USE_COLOR_STYLE
#   ifdef USE_PRETTYSRC
		current_is_class = IS_C(attributes[i]);
#   else
		current_is_class = (!strcasecomp("class", s));
#   endif
		CTRACE((tfp, "SGML: found attribute %s, %d\n", s, current_is_class));
#endif
	    }
	    return;
	}
	/* if */
    }				/* for */

    CTRACE((tfp, "SGML: Unknown attribute %s for tag %s\n",
	    s, NonNull(context->current_tag->name)));
    context->current_attribute_number = INVALID;	/* Invalid */
}

/*	Handle attribute value
 *	----------------------
 */
static void handle_attribute_value(HTStream *context, const char *s)
{
    if (context->current_attribute_number != INVALID) {
	StrAllocCopy_extra(context->value[context->current_attribute_number], s);
#ifdef USE_COLOR_STYLE
	if (current_is_class) {
	    StrNCpy(class_string, s, TEMPSTRINGSIZE);
	    CTRACE((tfp, "SGML: class is '%s'\n", s));
	} else {
	    CTRACE((tfp, "SGML: attribute value is '%s'\n", s));
	}
#endif
    } else {
	CTRACE((tfp, "SGML: Attribute value %s ***ignored\n", s));
    }
    context->current_attribute_number = INVALID;	/* can't have two assignments! */
}

/*
 *  Translate some Unicodes to Lynx special codes and output them.
 *  Special codes - ones those output depend on parsing.
 *
 *  Additional issue, like handling bidirectional text if necessary
 *  may be called from here:  zwnj (8204), zwj (8205), lrm (8206), rlm (8207)
 *  - currently they are ignored in SGML.c and LYCharUtils.c
 *  but also in UCdomap.c because they are non printable...
 *
 */
static BOOL put_special_unicodes(HTStream *context, UCode_t code)
{
    /* (Tgf_nolyspcl) */
    if (context->no_lynx_specialcodes) {
	/*
	 * We were asked by a "DTD" flag to not generate lynx specials.  - kw
	 */
	return NO;
    }

    if (code == CH_NBSP) {	/* S/390 -- gil -- 0657 */
	/*
	 * Use Lynx special character for nbsp.
	 */
#ifdef USE_PRETTYSRC
	if (!psrc_view)
#endif
	    PUTC(HT_NON_BREAK_SPACE);
    } else if (code == CH_SHY) {
	/*
	 * Use Lynx special character for shy.
	 */
#ifdef USE_PRETTYSRC
	if (!psrc_view)
#endif
	    PUTC(LY_SOFT_HYPHEN);
    } else if (code == 8194 || code == 8201) {
	/*
	 * Use Lynx special character for ensp or thinsp.
	 *
	 * Originally, Lynx use space '32' as word delimiter and omits this
	 * space at end of line if word is wrapped to the next line.  There are
	 * several other spaces in the Unicode repertoire and we should teach
	 * Lynx to understand them, not only as regular characters but in the
	 * context of line wrapping.  Unfortunately, if we use HT_EN_SPACE we
	 * override the chartrans tables for those spaces with a single '32'
	 * for all (but do line wrapping more fancy).
	 *
	 * We may treat emsp as one or two ensp (below).
	 */
#ifdef USE_PRETTYSRC
	if (!psrc_view)
#endif
	    PUTC(HT_EN_SPACE);
    } else if (code == 8195) {
	/*
	 * Use Lynx special character for emsp.
	 */
#ifdef USE_PRETTYSRC
	if (!psrc_view) {
#endif
	    /* PUTC(HT_EN_SPACE);  let's stay with a single space :) */
	    PUTC(HT_EN_SPACE);
#ifdef USE_PRETTYSRC
	}
#endif
    } else {
	/*
	 * Return NO if nothing done.
	 */
	return NO;
    }
    /*
     * We have handled it.
     */
    return YES;
}

#ifdef USE_PRETTYSRC
static void put_pretty_entity(HTStream *context, int term)
{
    PSRCSTART(entity);
    PUTC('&');
    PUTS(entity_string);
    if (term)
	PUTC((char) term);
    PSRCSTOP(entity);
}

static void put_pretty_number(HTStream *context)
{
    PSRCSTART(entity);
    PUTS((context->isHex ? "&#x" : "&#"));
    PUTS(entity_string);
    PUTC(';');
    PSRCSTOP(entity);
}
#endif /* USE_PRETTYSRC */

/*	Handle entity
 *	-------------
 *
 * On entry,
 *	s	contains the entity name zero terminated
 * Bugs:
 *	If the entity name is unknown, the terminator is treated as
 *	a printable non-special character in all cases, even if it is '<'
 * Bug-fix:
 *	Modified SGML_character() so we only come here with terminator
 *	as '\0' and check a FoundEntity flag. -- Foteos Macrides
 *
 * Modified more (for use with Lynx character translation code):
 */
static char replace_buf[64];	/* buffer for replacement strings */
static BOOL FoundEntity = FALSE;

static void handle_entity(HTStream *context, int term)
{
    UCode_t code;
    long uck = -1;
    const char *s = context->string->data;

    /*
     * Handle all entities normally.  - FM
     */
    FoundEntity = FALSE;
    if ((code = HTMLGetEntityUCValue(s)) != 0) {
	/*
	 * We got a Unicode value for the entity name.  Check for special
	 * Unicodes.  - FM
	 */
	if (put_special_unicodes(context, code)) {
#ifdef USE_PRETTYSRC
	    if (psrc_view) {
		put_pretty_entity(context, term);
	    }
#endif
	    FoundEntity = TRUE;
	    return;
	}
	/*
	 * Seek a translation from the chartrans tables.
	 */
	if ((uck = UCTransUniChar(code, context->outUCLYhndl)) >= 32 &&
/* =============== work in ASCII below here ===============  S/390 -- gil -- 0672 */
	    uck < 256 &&
	    (uck < 127 ||
	     uck >= LYlowest_eightbit[context->outUCLYhndl])) {
#ifdef USE_PRETTYSRC
	    if (psrc_view) {
		put_pretty_entity(context, term);
	    } else
#endif
		PUTC(FROMASCII((char) uck));
	    FoundEntity = TRUE;
	    return;
	} else if ((uck == -4 ||
		    (context->T.repl_translated_C0 &&
		     uck > 0 && uck < 32)) &&
	    /*
	     * Not found; look for replacement string.
	     */
		   (uck = UCTransUniCharStr(replace_buf, 60, code,
					    context->outUCLYhndl, 0) >= 0)) {
#ifdef USE_PRETTYSRC
	    if (psrc_view) {
		put_pretty_entity(context, term);
	    } else
#endif
		PUTS(replace_buf);
	    FoundEntity = TRUE;
	    return;
	}
	/*
	 * If we're displaying UTF-8, try that now.  - FM
	 */
#ifndef USE_PRETTYSRC
	if (context->T.output_utf8 && PUTUTF8(code)) {
	    FoundEntity = TRUE;
	    return;
	}
#else
	if (context->T.output_utf8 && (psrc_view
				       ? (UCPutUtf8_charstring((HTStream *) context->target,
							       (putc_func_t *) (fake_put_character),
							       code))
				       : PUTUTF8(code))) {

	    if (psrc_view) {
		put_pretty_entity(context, term);
	    }

	    FoundEntity = TRUE;
	    return;
	}
#endif
	/*
	 * If it's safe ASCII, use it.  - FM
	 */
	if (code >= 32 && code < 127) {
#ifdef USE_PRETTYSRC
	    if (psrc_view) {
		put_pretty_entity(context, term);
	    } else
#endif

		PUTC(FROMASCII((char) code));
	    FoundEntity = TRUE;
	    return;
	}
/* =============== work in ASCII above here ===============  S/390 -- gil -- 0682 */
	/*
	 * Ignore zwnj (8204) and zwj (8205), if we get to here.  Note that
	 * zwnj may have been handled as <WBR> by the calling function.  - FM
	 */
	if (!strcmp(s, "zwnj") ||
	    !strcmp(s, "zwj")) {
	    CTRACE((tfp, "handle_entity: Ignoring '%s'.\n", s));
#ifdef USE_PRETTYSRC
	    if (psrc_view) {
		put_pretty_entity(context, term);
	    }
#endif
	    FoundEntity = TRUE;
	    return;
	}
	/*
	 * Ignore lrm (8206), and rln (8207), if we get to here.  - FM
	 */
	if (!strcmp(s, "lrm") ||
	    !strcmp(s, "rlm")) {
	    CTRACE((tfp, "handle_entity: Ignoring '%s'.\n", s));
#ifdef USE_PRETTYSRC
	    if (psrc_view) {
		put_pretty_entity(context, term);
	    }
#endif
	    FoundEntity = TRUE;
	    return;
	}
    }

    /*
     * If entity string not found, display as text.
     */
#ifdef USE_PRETTYSRC
    if (psrc_view)
	PSRCSTART(badseq);
#endif
    CTRACE((tfp, "SGML: Unknown entity '%s' %" PRI_UCode_t " %ld\n", s, code, uck));	/* S/390 -- gil -- 0695 */
    PUTC('&');
    PUTS(s);
    if (term != '\0')
	PUTC(term);
#ifdef USE_PRETTYSRC
    if (psrc_view)
	PSRCSTOP(badseq);
#endif
}

/*	Handle comment
 *	--------------
 */
static void handle_comment(HTStream *context)
{
    const char *s = context->string->data;

    CTRACE((tfp, "SGML Comment:\n<%s>\n", s));

    if (context->csi == NULL &&
	StrNCmp(s, "!--#", 4) == 0 &&
	LYCheckForCSI(context->node_anchor, &context->url) == TRUE) {
	LYDoCSI(context->url, s, &context->csi);
    } else {
	LYCommentHacks(context->node_anchor, context->string->data);
    }

    return;
}

/*	Handle identifier
 *	-----------------
 */
static void handle_identifier(HTStream *context)
{
    const char *s = context->string->data;

    CTRACE((tfp, "SGML Identifier:\n<%s>\n", s));

    return;
}

/*	Handle doctype
 *	--------------
 */
static void handle_doctype(HTStream *context)
{
    const char *s = context->string->data;

    CTRACE((tfp, "SGML Doctype:\n<%s>\n", s));
    if (strstr(s, "DTD XHTML ") != 0) {
	CTRACE((tfp, "...processing extended HTML\n"));
	context->extended_html = TRUE;
    }

    return;
}

/*	Handle marked
 *	-------------
 */
static void handle_marked(HTStream *context)
{
    const char *s = context->string->data;

    CTRACE((tfp, "SGML Marked Section:\n<%s>\n", s));

    if (!StrNCmp(context->string->data, "![INCLUDE[", 10)) {
	context->string->data[context->string->size - 3] = '\0';
	StrAllocCat(context->include, context->string->data + 10);
	/* @@@ This needs to take charset into account! @@@
	   the wrong assumptions will be made about the data's
	   charset once it is in include - kw */

    } else if (!StrNCmp(context->string->data, "![CDATA[", 8)) {
	(*context->actions->put_block) (context->target,
					context->string->data + 8,
					context->string->size - 11);

    }
    return;
}

/*	Handle processing instruction
 *	-----------------------------
 */
static void handle_processing_instruction(HTStream *context)
{
    const char *s = context->string->data;

    CTRACE((tfp, "SGML Processing instruction:\n<%s>\n", s));

    if (!StrNCmp(s, "?xml ", 5)) {
	int flag = context->T.decode_utf8;

	context->strict_xml = TRUE;
	/*
	 * Switch to UTF-8 if the encoding is explicitly "utf-8".
	 */
	if (!flag) {
	    char *t = strstr(s, "encoding=");

	    if (t != 0) {
		t += 9;
		if (*t == '"')
		    ++t;
		flag = !StrNCmp(t, "utf-8", 5);
	    }
	    if (flag) {
		CTRACE((tfp, "...Use UTF-8 for XML\n"));
		context->T.decode_utf8 = TRUE;
	    }
	}
    }

    return;
}

/*	Handle sgmlent
 *	--------------
 */
static void handle_sgmlent(HTStream *context)
{
    const char *s = context->string->data;

    CTRACE((tfp, "SGML Entity Declaration:\n<%s>\n", s));

    return;
}

/*	Handle sgmlent
 *	--------------
 */
static void handle_sgmlele(HTStream *context)
{
    const char *s = context->string->data;

    CTRACE((tfp, "SGML Element Declaration:\n<%s>\n", s));

    return;
}

/*	Handle sgmlatt
 *	--------------
 */
static void handle_sgmlatt(HTStream *context)
{
    const char *s = context->string->data;

    CTRACE((tfp, "SGML Attribute Declaration:\n<%s>\n", s));

    return;
}

/*
 * Convenience macros - tags (elements) are identified sometimes by an int or
 * enum value ('TAGNUM'), sometimes by a pointer to HTTag ('TAGP').  - kw
 */
#define TAGNUM_OF_TAGP(t) (HTMLElement) (t - context->dtd->tags)
#define TAGP_OF_TAGNUM(e) (context->dtd->tags + e)

/*
 * The following implement special knowledge about OBJECT.  As long as
 * HTML_OBJECT is the only tag for which an alternative variant exist, they can
 * be simple macros.  - kw
 */
/* does 'TAGNUM' e have an alternative (variant) parsing mode? */
#define HAS_ALT_TAGNUM(e) (e == HTML_OBJECT)

/* return 'TAGNUM' of the alternative mode for 'TAGNUM' e, if any. */
#define ALT_TAGNUM(e) ((e == HTML_OBJECT) ? HTML_ALT_OBJECT : e)

/* return 'TAGNUM' of the normal mode for 'TAGNUM' e which may be alt. */
#define NORMAL_TAGNUM(e) (((int)(e) >= HTML_ELEMENTS) ? HTML_OBJECT : (HTMLElement)e)

/* More convenience stuff. - kw */
#define ALT_TAGP_OF_TAGNUM(e) TAGP_OF_TAGNUM(ALT_TAGNUM(e))
#define NORMAL_TAGP_OF_TAGNUM(e) TAGP_OF_TAGNUM(NORMAL_TAGNUM(e))

#define ALT_TAGP(t) ALT_TAGP_OF_TAGNUM(TAGNUM_OF_TAGP(t))
#define NORMAL_TAGP(t) NORMAL_TAGP_OF_TAGNUM(TAGNUM_OF_TAGP(t))

static BOOL element_valid_within(HTTag * new_tag, HTTag * stacked_tag, int direct)
{
    BOOL result = YES;
    TagClass usecontains, usecontained;

    if (stacked_tag && new_tag) {
	usecontains = (direct ? stacked_tag->contains : stacked_tag->icontains);
	usecontained = (direct ? new_tag->contained : new_tag->icontained);
	if (new_tag == stacked_tag) {
	    result = (BOOL) ((Tgc_same & usecontains) &&
			     (Tgc_same & usecontained));
	} else {
	    result = (BOOL) ((new_tag->tagclass & usecontains) &&
			     (stacked_tag->tagclass & usecontained));
	}
    }
    return result;
}

typedef enum {
    close_NO = 0,
    close_error = 1,
    close_valid = 2
} canclose_t;

static canclose_t can_close(HTTag * new_tag, HTTag * stacked_tag)
{
    canclose_t result;

    if (!stacked_tag) {
	result = close_NO;
    } else if (stacked_tag->flags & Tgf_endO) {
	result = close_valid;
    } else if (new_tag == stacked_tag) {
	result = ((Tgc_same & new_tag->canclose)
		  ? close_error
		  : close_NO);
    } else {
	result = ((stacked_tag->tagclass & new_tag->canclose)
		  ? close_error
		  : close_NO);
    }
    return result;
}

static void do_close_stacked(HTStream *context)
{
    HTElement *stacked = context->element_stack;
    HTMLElement e;

    if (!stacked)
	return;			/* stack was empty */
    if (context->inSELECT && !strcasecomp(stacked->tag->name, "SELECT")) {
	context->inSELECT = FALSE;
    }
    e = NORMAL_TAGNUM(TAGNUM_OF_TAGP(stacked->tag));
#ifdef USE_PRETTYSRC
    if (!psrc_view)		/* Don't actually pass call on if viewing psrc - kw */
#endif
	(*context->actions->end_element) (context->target,
					  (int) e,
					  &context->include);
    context->element_stack = stacked->next;
    pool_free(stacked);
    context->no_lynx_specialcodes =
	(BOOL) (context->element_stack
		? (context->element_stack->tag->flags & Tgf_nolyspcl)
		: NO);
}

static int is_on_stack(HTStream *context, HTTag * old_tag)
{
    HTElement *stacked = context->element_stack;
    int i = 1;

    for (; stacked; stacked = stacked->next, i++) {
	if (stacked->tag == old_tag ||
	    stacked->tag == ALT_TAGP(old_tag))
	    return i;
    }
    return 0;
}

/*	End element
 *	-----------
 */
static void end_element(HTStream *context, HTTag * old_tag)
{
    BOOL extra_action_taken = NO;
    canclose_t canclose_check = close_valid;
    int stackpos = is_on_stack(context, old_tag);

    if (!Old_DTD) {
	while (canclose_check != close_NO &&
	       context->element_stack &&
	       (stackpos > 1 || (!extra_action_taken && stackpos == 0))) {
	    if (stackpos == 0 && (old_tag->flags & Tgf_startO) &&
		element_valid_within(old_tag, context->element_stack->tag, YES)) {
		CTRACE((tfp, "SGML: </%s> ignored\n", old_tag->name));
		return;
	    }
	    canclose_check = can_close(old_tag, context->element_stack->tag);
	    if (canclose_check != close_NO) {
		CTRACE((tfp, "SGML: End </%s> \t<- %s end </%s>\n",
			context->element_stack->tag->name,
			((canclose_check == close_valid)
			 ? "supplied,"
			 : "***forced by"),
			old_tag->name));
		do_close_stacked(context);
		extra_action_taken = YES;
		stackpos = is_on_stack(context, old_tag);
	    }
	}

	if (stackpos == 0 && old_tag->contents != SGML_EMPTY) {
	    CTRACE((tfp, "SGML: Still open %s, ***no open %s for </%s>\n",
		    context->element_stack ?
		    context->element_stack->tag->name : "none",
		    old_tag->name,
		    old_tag->name));
	    return;
	}
	if (stackpos > 1) {
	    CTRACE((tfp,
		    "SGML: Nesting <%s>...<%s> \t<- ***invalid end </%s>\n",
		    old_tag->name,
		    context->element_stack ?
		    context->element_stack->tag->name : "none",
		    old_tag->name));
	    return;
	}
    }
    /* Now let the non-extended code deal with the rest. - kw */

    /*
     * If we are in a SELECT block, ignore anything but a SELECT end tag.  - FM
     */
    if (context->inSELECT) {
	if (!strcasecomp(old_tag->name, "SELECT")) {
	    /*
	     * Turn off the inSELECT flag and fall through.  - FM
	     */
	    context->inSELECT = FALSE;
	} else {
	    /*
	     * Ignore the end tag.  - FM
	     */
	    CTRACE((tfp, "SGML: ***Ignoring end tag </%s> in SELECT block.\n",
		    old_tag->name));
	    return;
	}
    }
    /*
     * Handle the end tag.  - FM
     */
    CTRACE((tfp, "SGML: End </%s>\n", old_tag->name));
    if (old_tag->contents == SGML_EMPTY) {
	CTRACE((tfp, "SGML: ***Illegal end tag </%s> found.\n",
		old_tag->name));
	return;
    }
#ifdef WIND_DOWN_STACK
    while (context->element_stack)	/* Loop is error path only */
#else
    if (context->element_stack)	/* Substitute and remove one stack element */
#endif /* WIND_DOWN_STACK */
    {
	int status = HT_OK;
	HTMLElement e;
	HTElement *N = context->element_stack;
	HTTag *t = (N->tag != old_tag) ? NORMAL_TAGP(N->tag) : N->tag;

	if (old_tag != t) {	/* Mismatch: syntax error */
	    if (context->element_stack->next) {		/* This is not the last level */
		CTRACE((tfp,
			"SGML: Found </%s> when expecting </%s>. </%s> ***assumed.\n",
			old_tag->name, t->name, t->name));
	    } else {		/* last level */
		CTRACE((tfp,
			"SGML: Found </%s> when expecting </%s>. </%s> ***Ignored.\n",
			old_tag->name, t->name, old_tag->name));
		return;		/* Ignore */
	    }
	}

	e = NORMAL_TAGNUM(TAGNUM_OF_TAGP(t));
	CTRACE2(TRACE_SGML, (tfp, "tagnum(%p) = %d\n", (void *) t, (int) e));
#ifdef USE_PRETTYSRC
	if (!psrc_view)		/* Don't actually pass call on if viewing psrc - kw */
#endif
	    status = (*context->actions->end_element) (context->target,
						       (int) e,
						       &context->include);
	if (status == HT_PARSER_REOPEN_ELT) {
	    CTRACE((tfp, "SGML: Restart <%s>\n", t->name));
	    (*context->actions->start_element) (context->target,
						(int) e,
						NULL,
						NULL,
						context->current_tag_charset,
						&context->include);
	} else if (status == HT_PARSER_OTHER_CONTENT) {
	    CTRACE((tfp, "SGML: Continue with other content model for <%s>\n", t->name));
	    context->element_stack->tag = ALT_TAGP_OF_TAGNUM(e);
	} else {
	    context->element_stack = N->next;	/* Remove from stack */
	    pool_free(N);
	}
	context->no_lynx_specialcodes =
	    (BOOL) (context->element_stack
		    ? (context->element_stack->tag->flags & Tgf_nolyspcl)
		    : NO);
#ifdef WIND_DOWN_STACK
	if (old_tag == t)
	    return;		/* Correct sequence */
#else
	return;
#endif /* WIND_DOWN_STACK */

	/* Syntax error path only */

    }
    CTRACE((tfp, "SGML: Extra end tag </%s> found and ignored.\n",
	    old_tag->name));
}

/*	Start a element
*/
static void start_element(HTStream *context)
{
    int status;
    HTTag *new_tag = context->current_tag;
    HTMLElement e = TAGNUM_OF_TAGP(new_tag);
    BOOL ok = FALSE;

    BOOL valid = YES;
    BOOL direct_container = YES;
    BOOL extra_action_taken = NO;
    canclose_t canclose_check = close_valid;

    if (!Old_DTD) {
	while (context->element_stack &&
	       (canclose_check == close_valid ||
		(canclose_check == close_error &&
		 new_tag == context->element_stack->tag)) &&
	       !(valid = element_valid_within(new_tag,
					      context->element_stack->tag,
					      direct_container))) {
	    canclose_check = can_close(new_tag, context->element_stack->tag);
	    if (canclose_check != close_NO) {
		CTRACE((tfp, "SGML: End </%s> \t<- %s start <%s>\n",
			context->element_stack->tag->name,
			((canclose_check == close_valid)
			 ? "supplied,"
			 : "***forced by"),
			new_tag->name));
		do_close_stacked(context);
		extra_action_taken = YES;
		if (canclose_check == close_error)
		    direct_container = NO;
	    } else {
		CTRACE((tfp,
			"SGML: Still open %s \t<- ***invalid start <%s>\n",
			context->element_stack->tag->name,
			new_tag->name));
	    }
	}
	if (context->element_stack && !valid &&
	    (context->element_stack->tag->flags & Tgf_strict) &&
	    !(valid = element_valid_within(new_tag,
					   context->element_stack->tag,
					   direct_container))) {
	    CTRACE((tfp, "SGML: Still open %s \t<- ***ignoring start <%s>\n",
		    context->element_stack->tag->name,
		    new_tag->name));
	    return;
	}

	if (context->element_stack &&
	    !extra_action_taken &&
	    (canclose_check == close_NO) &&
	    !valid && (new_tag->flags & Tgf_mafse)) {
	    BOOL has_attributes = NO;
	    int i = 0;

	    for (; i < new_tag->number_of_attributes && !has_attributes; i++)
		has_attributes = context->present[i];
	    if (!has_attributes) {
		CTRACE((tfp,
			"SGML: Still open %s, ***converting invalid <%s> to </%s>\n",
			context->element_stack->tag->name,
			new_tag->name,
			new_tag->name));
		end_element(context, new_tag);
		return;
	    }
	}

	if (context->element_stack &&
	    (canclose_check == close_error) &&
	    !element_valid_within(new_tag,
				  context->element_stack->tag,
				  direct_container)) {
	    CTRACE((tfp, "SGML: Still open %s \t<- ***invalid start <%s>\n",
		    context->element_stack->tag->name,
		    new_tag->name));
	}
    }
    /* Fall through to the non-extended code - kw */

    /*
     * If we are not in a SELECT block, check if this is a SELECT start tag. 
     * Otherwise (i.e., we are in a SELECT block) accept only OPTION as valid,
     * terminate the SELECT block if it is any other form-related element, and
     * otherwise ignore it.  - FM
     */
    if (!context->inSELECT) {
	/*
	 * We are not in a SELECT block, so check if this starts one.  - FM
	 * (frequent case!)
	 */
	/* my_casecomp() - optimized by the first character */
	if (!my_casecomp(new_tag->name, "SELECT")) {
	    /*
	     * Set the inSELECT flag and fall through.  - FM
	     */
	    context->inSELECT = TRUE;
	}
    } else {
	/*
	 * We are in a SELECT block.  - FM
	 */
	if (strcasecomp(new_tag->name, "OPTION")) {
	    /*
	     * Ugh, it is not an OPTION.  - FM
	     */
	    switch (e) {
	    case HTML_INPUT:
	    case HTML_TEXTAREA:
	    case HTML_SELECT:
	    case HTML_BUTTON:
	    case HTML_FIELDSET:
	    case HTML_LABEL:
	    case HTML_LEGEND:
	    case HTML_FORM:
		ok = TRUE;
		break;
	    default:
		break;
	    }
	    if (ok) {
		/*
		 * It is another form-related start tag, so terminate the
		 * current SELECT block and fall through.  - FM
		 */
		CTRACE((tfp,
			"SGML: ***Faking SELECT end tag before <%s> start tag.\n",
			new_tag->name));
		end_element(context, SGMLFindTag(context->dtd, "SELECT"));
	    } else {
		/*
		 * Ignore the start tag.  - FM
		 */
		CTRACE((tfp,
			"SGML: ***Ignoring start tag <%s> in SELECT block.\n",
			new_tag->name));
		return;
	    }
	}
    }
    /*
     * Handle the start tag.  - FM
     */
    CTRACE((tfp, "SGML: Start <%s>\n", new_tag->name));
    status = (*context->actions->start_element) (context->target,
						 (int) TAGNUM_OF_TAGP(new_tag),
						 context->present,
						 (STRING2PTR) context->value,	/* coerce type for think c */
						 context->current_tag_charset,
						 &context->include);
    if (status == HT_PARSER_OTHER_CONTENT)
	new_tag = ALT_TAGP(new_tag);	/* this is only returned for OBJECT */
    if (new_tag->contents != SGML_EMPTY) {	/* i.e., tag not empty */
	HTElement *N = pool_alloc();

	if (N == NULL)
	    outofmem(__FILE__, "start_element");

	assert(N != NULL);

	N->next = context->element_stack;
	N->tag = new_tag;
	context->element_stack = N;
	context->no_lynx_specialcodes = (BOOLEAN) (new_tag->flags & Tgf_nolyspcl);

    } else if (e == HTML_META) {
	/*
	 * Check for result of META tag.  - KW & FM
	 */
	change_chartrans_handling(context);
    }
}

/*		Find Tag in DTD tag list
 *		------------------------
 *
 * On entry,
 *	dtd	points to dtd structure including valid tag list
 *	string	points to name of tag in question
 *
 * On exit,
 *	returns:
 *		NULL		tag not found
 *		else		address of tag structure in dtd
 */
HTTag *SGMLFindTag(const SGML_dtd * dtd,
		   const char *s)
{
    int high, low, i, diff;
    static HTTag *last[64] =
    {NULL};			/*optimize using the previous results */
    HTTag **res = last + (UCH(*s) % 64);	/*pointer arithmetic */

    if (*res) {
	if ((*res)->name == NULL)
	    return NULL;
	if (!strcasecomp((*res)->name, s))
	    return *res;
    }

    for (low = 0, high = dtd->number_of_tags;
	 high > low;
	 diff < 0 ? (low = i + 1) : (high = i)) {	/* Binary search */
	i = (low + (high - low) / 2);
	/* my_casecomp() - optimized by the first character, NOT_ASCII ok */
	diff = my_casecomp(dtd->tags[i].name, s);	/* Case insensitive */
	if (diff == 0) {	/* success: found it */
	    *res = &dtd->tags[i];
	    return *res;
	}
    }
    if (IsNmStart(*s)) {
	/*
	 * Unrecognized, but may be valid.  - KW
	 */
	return &HTTag_unrecognized;
    }
    return NULL;
}

/*________________________________________________________________________
 *			Public Methods
 */

/*	Could check that we are back to bottom of stack! @@  */
/*	Do check! - FM					     */
/*							     */
static void SGML_free(HTStream *context)
{
    int i;
    HTElement *cur;
    HTTag *t;

    /*
     * Free the buffers.  - FM
     */
    FREE(context->recover);
    FREE(context->url);
    FREE(context->csi);
    FREE(context->include);
    FREE(context->active_include);

    /*
     * Wind down stack if any elements are open.  - FM
     */
    while (context->element_stack) {
	cur = context->element_stack;
	t = cur->tag;
	context->element_stack = cur->next;	/* Remove from stack */
	pool_free(cur);
#ifdef USE_PRETTYSRC
	if (!psrc_view)		/* Don't actually call on target if viewing psrc - kw */
#endif
	    (*context->actions->end_element)
		(context->target,
		 (int) NORMAL_TAGNUM(TAGNUM_OF_TAGP(t)),
		 &context->include);
	FREE(context->include);
    }

    /*
     * Finish off the target.  - FM
     */
    (*context->actions->_free) (context->target);

    /*
     * Free the strings and context structure.  - FM
     */
    HTChunkFree(context->string);
    for (i = 0; i < MAX_ATTRIBUTES; i++)
	FREE_extra(context->value[i]);
    FREE(context);

#ifdef USE_PRETTYSRC
    sgml_in_psrc_was_initialized = FALSE;
#endif
}

static void SGML_abort(HTStream *context, HTError e)
{
    int i;
    HTElement *cur;

    /*
     * Abort the target.  - FM
     */
    (*context->actions->_abort) (context->target, e);

    /*
     * Free the buffers.  - FM
     */
    FREE(context->recover);
    FREE(context->include);
    FREE(context->active_include);
    FREE(context->url);
    FREE(context->csi);

    /*
     * Free stack memory if any elements were left open.  - KW
     */
    while (context->element_stack) {
	cur = context->element_stack;
	context->element_stack = cur->next;	/* Remove from stack */
	pool_free(cur);
    }

    /*
     * Free the strings and context structure.  - FM
     */
    HTChunkFree(context->string);
    for (i = 0; i < MAX_ATTRIBUTES; i++)
	FREE_extra(context->value[i]);
    FREE(context);

#ifdef USE_PRETTYSRC
    sgml_in_psrc_was_initialized = FALSE;
#endif
}

/*	Read and write user callback handle
 *	-----------------------------------
 *
 *   The callbacks from the SGML parser have an SGML context parameter.
 *   These calls allow the caller to associate his own context with a
 *   particular SGML context.
 */

#ifdef CALLERDATA
void *SGML_callerData(HTStream *context)
{
    return context->callerData;
}

void SGML_setCallerData(HTStream *context, void *data)
{
    context->callerData = data;
}
#endif /* CALLERDATA */

#ifdef USE_PRETTYSRC
static void transform_tag(HTStream *context, HTChunk *string)
{
    if (!context->strict_xml) {
	if (tagname_transform != 1) {
	    if (tagname_transform == 0)
		LYLowerCase(string->data);
	    else
		LYUpperCase(string->data);
	}
    }
}
#endif /* USE_PRETTYSRC */

static BOOL ignore_when_empty(HTTag * tag)
{
    BOOL result = FALSE;

    if (!LYPreparsedSource
	&& LYxhtml_parsing
	&& tag->name != 0
	&& !(tag->flags & Tgf_mafse)
	&& tag->contents != SGML_EMPTY
	&& tag->tagclass != Tgc_Plike
	&& (tag->tagclass == Tgc_SELECTlike
	    || (tag->contains && tag->icontains))) {
	result = TRUE;
    }
    CTRACE((tfp, "SGML Do%s ignore_when_empty:%s\n",
	    result ? "" : " not",
	    NonNull(tag->name)));
    return result;
}

static void discard_empty(HTStream *context)
{
    static HTTag empty_tag;

    CTRACE((tfp, "SGML discarding empty %s\n",
	    NonNull(context->current_tag->name)));
    CTRACE_FLUSH(tfp);

    memset(&empty_tag, 0, sizeof(empty_tag));
    context->current_tag = &empty_tag;
    context->string->size = 0;

    /* do not call end_element() if start_element() was not called */
}

#ifdef USE_PRETTYSRC
static BOOL end_if_prettysrc(HTStream *context, HTChunk *string, int end_ch)
{
    BOOL result = psrc_view;

    if (psrc_view) {
	if (attr_is_name) {
	    HTStartAnchor(context->target, string->data, NULL);
	    (*context->actions->end_element) (context->target,
					      HTML_A,
					      &context->include);
	} else if (attr_is_href) {
	    PSRCSTART(href);
	    HTStartAnchor(context->target, NULL, string->data);
	}
	PUTS_TR(string->data);
	if (attr_is_href) {
	    (*context->actions->end_element) (context->target,
					      HTML_A,
					      &context->include);
	    PSRCSTOP(href);
	}
	if (end_ch)
	    PUTC(end_ch);
	PSRCSTOP(attrval);
    }
    return result;
}
#endif

static void SGML_character(HTStream *context, int c_in)
{
    const SGML_dtd *dtd = context->dtd;
    HTChunk *string = context->string;
    const char *EntityName;
    HTTag *testtag = NULL;
    BOOLEAN chk;		/* Helps (?) walk through all the else ifs... */
    UCode_t clong, uck = 0;	/* Enough bits for UCS4 ... */
    int testlast;

    unsigned char c;
    unsigned char saved_char_in = '\0';

    ++sgml_offset;

    /*
     * Now some fun with the preprocessor.  Use copies for c and unsign_c ==
     * clong, so that we can revert back to the unchanged c_in.  - KW
     */
#define unsign_c clong

    c = UCH(c_in);
    clong = UCH(c);		/* a.k.a. unsign_c */

    if (context->T.decode_utf8) {
	/*
	 * Combine UTF-8 into Unicode.  Incomplete characters silently ignored. 
	 * From Linux kernel's console.c.  - KW
	 */
	if (TOASCII(UCH(c)) > 127) {	/* S/390 -- gil -- 0710 */
	    /*
	     * We have an octet from a multibyte character.  - FM
	     */
	    if (context->utf_count > 0 && (TOASCII(c) & 0xc0) == 0x80) {
		context->utf_char = (context->utf_char << 6) | (TOASCII(c) & 0x3f);
		context->utf_count--;
		*(context->utf_buf_p) = (char) c;
		(context->utf_buf_p)++;
		if (context->utf_count == 0) {
		    /*
		     * We have all of the bytes, so terminate the buffer and
		     * set 'clong' to the UCode_t value.  - FM
		     */
		    *(context->utf_buf_p) = '\0';
		    clong = context->utf_char;
		    if (clong < 256) {
			c = UCH(clong & 0xff);
		    }
		    /* lynx does not use left-to-right */
		    if (clong == 0x200e)
			return;
		    goto top1;
		} else {
		    /*
		     * Wait for more.  - KW
		     */
		    return;
		}
	    } else {
		/*
		 * Start handling a new multibyte character.  - FM
		 */
		context->utf_buf_p = context->utf_buf;
		*(context->utf_buf_p) = (char) c;
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
		     * Garbage.  - KW
		     */
		    context->utf_count = 0;
		    context->utf_buf_p = context->utf_buf;
		    *(context->utf_buf_p) = '\0';
		}
		/*
		 * Wait for more.  - KW
		 */
		return;
	    }
	} else {
	    /*
	     * Got an ASCII char.  - KW
	     */
	    context->utf_count = 0;
	    context->utf_buf_p = context->utf_buf;
	    *(context->utf_buf_p) = '\0';
	    /*  goto top;  */
	}
    }
    /* end of context->T.decode_utf8      S/390 -- gil -- 0726 */
#ifdef NOTDEFINED
    /*
     * If we have a koi8-r input and do not have koi8-r as the output, save the
     * raw input in saved_char_in before we potentially convert it to Unicode. 
     * - FM
     */
    if (context->T.strip_raw_char_in)
	saved_char_in = c;
#endif /* NOTDEFINED */

    /*
     * If we want the raw input converted to Unicode, try that now.  - FM
     */
    if (context->T.trans_to_uni &&
#ifdef EXP_JAPANESEUTF8_SUPPORT
	((strcmp(LYCharSet_UC[context->inUCLYhndl].MIMEname, "euc-jp") == 0) ||
	 (strcmp(LYCharSet_UC[context->inUCLYhndl].MIMEname, "shift_jis") == 0))) {
	if (strcmp(LYCharSet_UC[context->inUCLYhndl].MIMEname, "shift_jis") == 0) {
	    if (context->utf_count == 0) {
		if (IS_SJIS_HI1((unsigned char) c) ||
		    IS_SJIS_HI2((unsigned char) c)) {
		    context->utf_buf[0] = (char) c;
		    context->utf_count = 1;
		    clong = -11;
		}
	    } else {
		if (IS_SJIS_LO((unsigned char) c)) {
		    context->utf_buf[1] = (char) c;
		    clong = UCTransJPToUni(context->utf_buf, 2, context->inUCLYhndl);
		}
		context->utf_count = 0;
	    }
	} else {
	    if (context->utf_count == 0) {
		if (IS_EUC_HI((unsigned char) c)) {
		    context->utf_buf[0] = (char) c;
		    context->utf_count = 1;
		    clong = -11;
		}
	    } else {
		if (IS_EUC_LOX((unsigned char) c)) {
		    context->utf_buf[1] = (char) c;
		    clong = UCTransJPToUni(context->utf_buf, 2, context->inUCLYhndl);
		}
		context->utf_count = 0;
	    }
	}
	goto top1;
    } else if (context->T.trans_to_uni &&
#endif
	       ((TOASCII(unsign_c) >= LYlowest_eightbit[context->inUCLYhndl]) ||	/* S/390 -- gil -- 0744 */
		(unsign_c < ' ' && unsign_c != 0 &&
		 context->T.trans_C0_to_uni))) {
	/*
	 * Convert the octet to Unicode.  - FM
	 */
	clong = UCTransToUni((char) c, context->inUCLYhndl);
	if (clong > 0) {
	    saved_char_in = c;
	    if (clong < 256) {
		c = FROMASCII(UCH(clong));
	    }
	}
	goto top1;
    } else if (unsign_c < ' ' && unsign_c != 0 &&	/* S/390 -- gil -- 0768 */
	       context->T.trans_C0_to_uni) {
	/*
	 * This else if may be too ugly to keep.  - KW
	 */
	if (context->T.trans_from_uni &&
	    (((clong = UCTransToUni((char) c, context->inUCLYhndl)) >= ' ') ||
	     (context->T.transp &&
	      (clong = UCTransToUni((char) c, context->inUCLYhndl)) > 0))) {
	    saved_char_in = c;
	    if (clong < 256) {
		c = FROMASCII(UCH(clong));
	    }
	    goto top1;
	} else {
	    uck = -1;
	    if (context->T.transp) {
		uck = UCTransCharStr(replace_buf, 60, (char) c,
				     context->inUCLYhndl,
				     context->inUCLYhndl, NO);
	    }
	    if (!context->T.transp || uck < 0) {
		uck = UCTransCharStr(replace_buf, 60, (char) c,
				     context->inUCLYhndl,
				     context->outUCLYhndl, YES);
	    }
	    if (uck == 0) {
		return;
	    } else if (uck < 0) {
		goto top0a;
	    }
	    c = UCH(replace_buf[0]);
	    if (c && replace_buf[1]) {
		if (context->state == S_text) {
		    PUTS(replace_buf);
		    return;
		}
		StrAllocCat(context->recover, replace_buf + 1);
	    }
	    goto top0a;
	}			/*  Next line end of ugly stuff for C0. - KW */
    } else {			/* end of context->T.trans_to_uni  S/390 -- gil -- 0791 */
	goto top0a;
    }

    /*
     * At this point we have either unsign_c a.k.a.  clong in Unicode (and c in
     * latin1 if clong is in the latin1 range), or unsign_c and c will have to
     * be passed raw.  - KW
     */
/*
 *  We jump up to here from below if we have
 *  stuff in the recover, insert, or csi buffers
 *  to process.	 We zero saved_char_in, in effect
 *  as a flag that the octet is not that of the
 *  actual call to this function.  This may be OK
 *  for now, for the stuff this function adds to
 *  its recover buffer, but it might not be for
 *  stuff other functions added to the insert or
 *  csi buffer, so bear that in mind. - FM
 *  Stuff from the recover buffer is now handled
 *  as UTF-8 if we can expect that's what it is,
 *  and in that case we don't come back up here. - kw
 */
  top:
    saved_char_in = '\0';
/*
 *  We jump to here from above when we don't have
 *  UTF-8 input, haven't converted to Unicode, and
 *  want clong set to the input octet (unsigned)
 *  without zeroing its saved_char_in copy (which
 *  is signed). - FM
 */
  top0a:
    *(context->utf_buf) = '\0';
    clong = UCH(c);
/*
 *  We jump to here from above if we have converted
 *  the input, or a multibyte sequence across calls,
 *  to a Unicode value and loaded it into clong (to
 *  which unsign_c has been defined), and from below
 *  when we are recycling a character (e.g., because
 *  it terminated an entity but is not the standard
 *  semi-colon).  The character will already have
 *  been put through the Unicode conversions. - FM
 */
  top1:
    /*
     * Ignore low ISO 646 7-bit control characters if HTCJK is not set.  - FM
     */
    /*
     * Works for both ASCII and EBCDIC. -- gil
     * S/390 -- gil -- 0811
     */
    if (TOASCII(unsign_c) < 32 &&
	c != '\t' && c != '\n' && c != '\r' &&
	!IS_CJK_TTY)
	goto after_switch;

    /*
     * Ignore 127 if we don't have HTPassHighCtrlRaw or HTCJK set.  - FM
     */
#define PASSHICTRL (context->T.transp || \
		    unsign_c >= LYlowest_eightbit[context->inUCLYhndl])
    if (TOASCII(c) == 127 &&	/* S/390 -- gil -- 0830 */
	!(PASSHICTRL || IS_CJK_TTY))
	goto after_switch;

    /*
     * Ignore 8-bit control characters 128 - 159 if neither HTPassHighCtrlRaw
     * nor HTCJK is set.  - FM
     */
    if (TOASCII(unsign_c) > 127 && TOASCII(unsign_c) < 160 &&	/* S/390 -- gil -- 0847 */
	!(PASSHICTRL || IS_CJK_TTY)) {
	/*
	 * If we happen to be reading from an "ISO-8859-1" or "US-ASCII"
	 * document, allow the cp-1252 codes, to accommodate the HTML5 draft
	 * recommendation for replacement encoding:
	 *
	 * http://www.whatwg.org/specs/web-apps/current-work/multipage/infrastructure.html#character-encodings-0
	 */
	if (AssumeCP1252(context)) {
	    clong = LYcp1252ToUnicode((UCode_t) c);
	    goto top1;
	}
	goto after_switch;
    }

    /* Almost all CJK characters are double byte but only Japanese
     * JIS X0201 Kana is single byte. To prevent to fail SGML parsing
     * we have to take care of them here. -- TH
     */
    if ((HTCJK == JAPANESE) && (context->state == S_in_kanji) &&
	!IS_JAPANESE_2BYTE(context->kanji_buf, UCH(c))
#ifdef EXP_JAPANESEUTF8_SUPPORT
	&& !context->T.decode_utf8
#endif
	) {
#ifdef CONV_JISX0201KANA_JISX0208KANA
	if (IS_SJIS_X0201KANA(context->kanji_buf)) {
	    unsigned char sjis_hi, sjis_lo;

	    JISx0201TO0208_SJIS(context->kanji_buf, &sjis_hi, &sjis_lo);
	    PUTC(sjis_hi);
	    PUTC(sjis_lo);
	} else
#endif
	    PUTC(context->kanji_buf);
	context->state = S_text;
    }

    /*
     * Handle character based on context->state.
     */
    CTRACE2(TRACE_SGML, (tfp, "SGML before %s|%.*s|%c|\n",
			 state_name(context->state),
			 string->size,
			 NonNull(string->data),
			 UCH(c)));
    switch (context->state) {

    case S_in_kanji:
	/*
	 * Note that if we don't have a CJK input, then this is not the second
	 * byte of a CJK di-byte, and we're trashing the input.  That's why
	 * 8-bit characters followed by, for example, '<' can cause the tag to
	 * be treated as text, not markup.  We could try to deal with it by
	 * holding each first byte and then checking byte pairs, but that
	 * doesn't seem worth the overhead (see below).  - FM
	 */
	context->state = S_text;
	PUTC(context->kanji_buf);
	PUTC(c);
	break;

    case S_tagname_slash:
	/*
	 * We had something link "<name/" so far, set state to S_text but keep
	 * context->slashedtag as a flag; except if we get '>' directly
	 * after the "<name/", and really have a tag for that name in
	 * context->slashedtag, in which case keep state as is and let code
	 * below deal with it.  - kw
	 */
	if (!(c == '>' && context->slashedtag && TOASCII(unsign_c) < 127)) {
	    context->state = S_text;
	}
	/* fall through in any case! */
    case S_text:
	if (IS_CJK_TTY && ((TOASCII(c) & 0200) != 0)
#ifdef EXP_JAPANESEUTF8_SUPPORT
	    && !context->T.decode_utf8
#endif
	    ) {			/* S/390 -- gil -- 0864 */
	    /*
	     * Setting up for Kanji multibyte handling (based on Takuya ASADA's
	     * (asada@three-a.co.jp) CJK Lynx).  Note that if the input is not
	     * in fact CJK, the next byte also will be mishandled, as explained
	     * above.  Toggle raw mode off in such cases, or select the "7 bit
	     * approximations" display character set, which is largely
	     * equivalent to having raw mode off with CJK.  - FM
	     */
	    context->state = S_in_kanji;
	    context->kanji_buf = c;
	    break;
	} else if (IS_CJK_TTY && TOASCII(c) == '\033') {	/* S/390 -- gil -- 0881 */
	    /*
	     * Setting up for CJK escape sequence handling (based on Takuya
	     * ASADA's (asada@three-a.co.jp) CJK Lynx).  - FM
	     */
	    context->state = S_esc;
	    PUTC(c);
	    break;
	}

	if (c == '&' || c == '<') {
#ifdef USE_PRETTYSRC
	    if (psrc_view) {	/*there is nothing useful in the element_stack */
		testtag = context->current_tag;
	    } else
#endif
	    {
		testtag = context->element_stack ?
		    context->element_stack->tag : NULL;
	    }
	}

	if (c == '&' && TOASCII(unsign_c) < 127 &&	/* S/390 -- gil -- 0898 */
	    (!testtag ||
	     (testtag->contents == SGML_MIXED ||
	      testtag->contents == SGML_ELEMENT ||
	      testtag->contents == SGML_PCDATA ||
#ifdef USE_PRETTYSRC
	      testtag->contents == SGML_EMPTY ||
#endif
	      testtag->contents == SGML_RCDATA))) {
	    /*
	     * Setting up for possible entity, without the leading '&'.  - FM
	     */
	    string->size = 0;
	    context->state = S_ero;
	} else if (c == '<' && TOASCII(unsign_c) < 127) {	/* S/390 -- gil -- 0915 */
	    /*
	     * Setting up for possible tag.  - FM
	     */
	    string->size = 0;
	    if (testtag && testtag->contents == SGML_PCDATA) {
		context->state = S_pcdata;
	    } else if (testtag && (testtag->contents == SGML_LITTERAL
				   || testtag->contents == SGML_CDATA)) {
		context->state = S_litteral;
	    } else if (testtag && (testtag->contents == SGML_SCRIPT)) {
		context->state = S_script;
	    } else {
		context->state = S_tag;
	    }
	    context->slashedtag = NULL;
	} else if (context->slashedtag &&
		   context->slashedtag->name &&
		   (c == '/' ||
		    (c == '>' && context->state == S_tagname_slash)) &&
		   TOASCII(unsign_c) < 127) {
	    /*
	     * We got either the second slash of a pending "<NAME/blah blah/"
	     * shortref construct, or the '>' of a mere "<NAME/>".  In both
	     * cases generate a "</NAME>" end tag in the recover buffer for
	     * reparsing unless NAME is really an empty element.  - kw
	     */
#ifdef USE_PRETTYSRC
	    if (psrc_view) {
		PSRCSTART(abracket);
		PUTC(c);
		PSRCSTOP(abracket);
	    } else
#endif
		if (context->slashedtag != context->unknown_tag &&
		    !ReallyEmptyTag(context->slashedtag)) {
		if (context->recover == NULL) {
		    StrAllocCopy(context->recover, "</");
		    context->recover_index = 0;
		} else {
		    StrAllocCat(context->recover, "</");
		}
		StrAllocCat(context->recover, context->slashedtag->name);
		StrAllocCat(context->recover, ">");
	    }
	    context->slashedtag = NULL;

	} else if (context->element_stack &&
		   (context->element_stack->tag->flags & Tgf_frecyc)) {
	    /*
	     * The element stack says we are within the contents of an element
	     * that the next stage (HTML.c) may want to feed us back again (via
	     * the *include string).  So try to output text in UTF-8 if
	     * possible, using the same logic as for attribute values (which
	     * should be in line with what context->current_tag_charset
	     * indicates).  - kw
	     */
	    if (context->T.decode_utf8 &&
		*context->utf_buf) {
		PUTS(context->utf_buf);
		context->utf_buf_p = context->utf_buf;
		*(context->utf_buf_p) = '\0';
	    } else if (!IS_CJK_TTY &&
		       (context->T.output_utf8 ||
			context->T.trans_from_uni)) {
		if (LYIsASCII(clong)) {
		    PUTC(c);
		} else if (clong == 0xfffd && saved_char_in &&
			   HTPassEightBitRaw &&
			   saved_char_in >=
			   LYlowest_eightbit[context->outUCLYhndl]) {
		    PUTUTF8((UCode_t) (0xf000 | saved_char_in));
		} else {
		    PUTUTF8(clong);
		}
	    } else if (saved_char_in && context->T.use_raw_char_in) {
		PUTC(saved_char_in);
	    } else {
		PUTC(c);
	    }

#define PASS8859SPECL context->T.pass_160_173_raw
	    /*
	     * Convert 160 (nbsp) to Lynx special character if neither
	     * HTPassHighCtrlRaw nor HTCJK is set.  - FM
	     */
	} else if (unsign_c == CH_NBSP &&	/* S/390 -- gil -- 0932 */
		   !context->no_lynx_specialcodes &&
		   !(PASS8859SPECL || IS_CJK_TTY)) {
	    PUTC(HT_NON_BREAK_SPACE);
	    /*
	     * Convert 173 (shy) to Lynx special character if neither
	     * HTPassHighCtrlRaw nor HTCJK is set.  - FM
	     */
	} else if (unsign_c == CH_SHY &&	/* S/390 -- gil -- 0949 */
		   !context->no_lynx_specialcodes &&
		   !(PASS8859SPECL || IS_CJK_TTY)) {
	    PUTC(LY_SOFT_HYPHEN);
	    /*
	     * Handle the case in which we think we have a character which
	     * doesn't need further processing (e.g., a koi8-r input for a
	     * koi8-r output).  - FM
	     */
	} else if (context->T.use_raw_char_in && saved_char_in) {
	    /*
	     * Only if the original character is still in saved_char_in,
	     * otherwise we may be iterating from a goto top.  - KW
	     */
	    PUTC(saved_char_in);
/******************************************************************
 * I.  LATIN-1 OR UCS2 TO DISPLAY CHARSET
 ******************************************************************/
	} else if ((chk = (BOOL) (context->T.trans_from_uni &&
				  TOASCII(unsign_c) >= 160)) &&		/* S/390 -- gil -- 0968 */
		   (uck = UCTransUniChar(unsign_c,
					 context->outUCLYhndl)) >= ' ' &&
		   uck < 256) {
	    CTRACE((tfp, "UCTransUniChar returned 0x%.2" PRI_UCode_t
		    ":'%c'.\n",
		    uck, FROMASCII((char)uck)));
	    /*
	     * We got one octet from the conversions, so use it.  - FM
	     */
	    PUTC(FROMASCII((char) uck));
	} else if ((chk &&
		    (uck == -4 ||
		     (context->T.repl_translated_C0 &&
		      uck > 0 && uck < 32))) &&
	    /*
	     * Not found; look for replacement string.  - KW
	     */
		   (uck = UCTransUniCharStr(replace_buf, 60, clong,
					    context->outUCLYhndl,
					    0) >= 0)) {
	    /*
	     * Got a replacement string.  No further tests for validity -
	     * assume that whoever defined replacement strings knew what she
	     * was doing.  - KW
	     */
	    PUTS(replace_buf);
	    /*
	     * If we're displaying UTF-8, try that now.  - FM
	     */
	} else if (context->T.output_utf8 && PUTUTF8(clong)) {
	    ;			/* do nothing more */
	    /*
	     * If it's any other (> 160) 8-bit character, and we have not set
	     * HTPassEightBitRaw nor HTCJK, nor have the "ISO Latin 1"
	     * character set selected, back translate for our character set.  -
	     * FM
	     */
#define IncludesLatin1Enc \
		(context->outUCLYhndl == LATIN1 || \
		 (context->outUCI && \
		  (context->outUCI->enc & (UCT_CP_SUPERSETOF_LAT1))))

#define PASSHI8BIT (HTPassEightBitRaw || \
		    (context->T.do_8bitraw && !context->T.trans_from_uni))

	} else if (unsign_c > 160 && unsign_c < 256 &&
		   !(PASSHI8BIT || IS_CJK_TTY) &&
		   !IncludesLatin1Enc) {
#ifdef USE_PRETTYSRC
	    int psrc_view_backup = 0;
#endif

	    string->size = 0;
	    EntityName = HTMLGetEntityName((UCode_t) (unsign_c - 160));
	    HTChunkPuts(string, EntityName);
	    HTChunkTerminate(string);
#ifdef USE_PRETTYSRC
	    /* we need to disable it temporarily */
	    if (psrc_view) {
		psrc_view_backup = 1;
		psrc_view = 0;
	    }
#endif
	    handle_entity(context, '\0');
#ifdef USE_PRETTYSRC
	    /* we need to disable it temporarily */
	    if (psrc_view_backup)
		psrc_view = TRUE;
#endif

	    string->size = 0;
	    if (!FoundEntity)
		PUTC(';');
	    /*
	     * If we get to here and have an ASCII char, pass the character.  -
	     * KW
	     */
	} else if (TOASCII(unsign_c) < 127 && unsign_c > 0) {	/* S/390 -- gil -- 0987 */
	    PUTC(c);
	    /*
	     * If we get to here, and should have translated, translation has
	     * failed so far.  - KW
	     *
	     * We should have sent UTF-8 output to the parser already, but what
	     * the heck, try again.  - FM
	     */
	} else if (context->T.output_utf8 && *context->utf_buf) {
	    PUTS(context->utf_buf);
	    context->utf_buf_p = context->utf_buf;
	    *(context->utf_buf_p) = '\0';
#ifdef NOTDEFINED
	    /*
	     * Check for a strippable koi8-r 8-bit character.  - FM
	     */
	} else if (context->T.strip_raw_char_in && saved_char_in &&
		   (saved_char_in >= 0xc0) &&
		   (saved_char_in < 255)) {
	    /*
	     * KOI8 special:  strip high bit, gives (somewhat) readable ASCII
	     * or KOI7 - it was constructed that way!  - KW
	     */
	    PUTC((saved_char_in & 0x7f));
	    saved_char_in = '\0';
#endif /* NOTDEFINED */
	    /*
	     * If we don't actually want the character, make it safe and output
	     * that now.  - FM
	     */
	} else if (TOASCII(UCH(c)) <	/* S/390 -- gil -- 0997 */
		   LYlowest_eightbit[context->outUCLYhndl] ||
		   (context->T.trans_from_uni && !HTPassEightBitRaw)) {
	    /*
	     * If we get to here, pass the character.  - FM
	     */
	} else {
	    PUTC(c);
	}
	break;

	/*
	 * Found '<' in SGML_PCDATA content; treat this mode nearly like
	 * S_litteral, but recognize '<!' and '<?' to filter out comments and
	 * processing instructions.  - kw
	 */
    case S_pcdata:
	if (!string->size && TOASCII(unsign_c) < 127) {		/* first after '<' */
	    if (c == '!') {	/* <! */
		/*
		 * Terminate and set up for possible comment, identifier,
		 * declaration, or marked section as under S_tag.  - kw
		 */
		context->state = S_exclamation;
		context->lead_exclamation = TRUE;
		context->doctype_bracket = FALSE;
		context->first_bracket = FALSE;
		HTChunkPutc(string, c);
		break;
	    } else if (c == '?') {	/* <? - ignore as a PI until '>' - kw */
		CTRACE((tfp,
			"SGML: Found PI in PCDATA, junking it until '>'\n"));
#ifdef USE_PRETTYSRC
		if (psrc_view) {
		    PSRCSTART(abracket);
		    PUTS("<?");
		    PSRCSTOP(abracket);
		}
#endif
		context->state = S_pi;
		break;
	    }
	}
	goto case_S_litteral;

	/*
	 * Found '<' in SGML_SCRIPT content; treat this mode nearly like
	 * S_litteral, but recognize '<!' to allow the content to be treated as
	 * a comment by lynx.
	 */
    case S_script:
	if (!string->size && TOASCII(unsign_c) < 127) {		/* first after '<' */
	    if (c == '!') {	/* <! */
		/*
		 * Terminate and set up for possible comment, identifier,
		 * declaration, or marked section as under S_tag.  - kw
		 */
		context->state = S_exclamation;
		context->lead_exclamation = TRUE;
		context->doctype_bracket = FALSE;
		context->first_bracket = FALSE;
		HTChunkPutc(string, c);
		break;
	    }
	}
	goto case_S_litteral;

	/*
	 * In litteral mode, waits only for specific end tag (for compatibility
	 * with old servers, and for Lynx).  - FM
	 */
      case_S_litteral:
    case S_litteral:
	/*PSRC:this case not understood completely by HV, not done */
	HTChunkPutc(string, c);
#ifdef USE_PRETTYSRC
	if (psrc_view) {
	    /* there is nothing useful in the element_stack */
	    testtag = context->current_tag;
	} else
#endif
	    testtag = (context->element_stack
		       ? context->element_stack->tag
		       : NULL);

	if (testtag == NULL || testtag->name == NULL) {
	    string->size--;
	    context->state = S_text;
	    goto top1;
	}

	/*
	 * Normally when we get the closing ">",
	 *      testtag contains something like "TITLE"
	 *      string contains something like "/title>"
	 * so we decrement by 2 to compare the final character of each.
	 */
	testlast = string->size - 2 - context->trailing_spaces - context->leading_spaces;

	if (TOUPPER(c) != ((testlast < 0)
			   ? '/'
			   : testtag->name[testlast])) {
	    int i;

	    /*
	     * If complete match, end litteral.
	     */
	    if ((c == '>') &&
		testlast >= 0 && !testtag->name[testlast]) {
#ifdef USE_PRETTYSRC
		if (psrc_view) {
		    char *trailing = NULL;

		    if (context->trailing_spaces) {
			StrAllocCopy(trailing,
				     string->data
				     + string->size
				     - 1
				     - context->trailing_spaces);
			trailing[context->trailing_spaces] = '\0';
		    }

		    PSRCSTART(abracket);
		    PUTS("</");
		    PSRCSTOP(abracket);
		    PSRCSTART(tag);

		    strcpy(string->data, context->current_tag->name);
		    transform_tag(context, string);
		    PUTS(string->data);

		    if (trailing) {
			PUTS(trailing);
			FREE(trailing);
		    }

		    PSRCSTOP(tag);
		    PSRCSTART(abracket);
		    PUTC('>');
		    PSRCSTOP(abracket);

		    context->current_tag = NULL;
		} else
#endif
		    end_element(context, context->element_stack->tag);

		string->size = 0;
		context->current_attribute_number = INVALID;
		context->state = S_text;
		context->leading_spaces = 0;
		context->trailing_spaces = 0;
		break;
	    }

	    /*
	     * Allow whitespace between the "<" or ">" and the keyword, for
	     * error-recovery.
	     */
	    if (isspace(UCH(c))) {
		if (testlast == -1) {
		    context->leading_spaces += 1;
		    CTRACE2(TRACE_SGML, (tfp, "leading spaces: %d\n", context->leading_spaces));
		    break;
		} else if (testlast > 0) {
		    context->trailing_spaces += 1;
		    CTRACE2(TRACE_SGML, (tfp, "trailing spaces: %d\n", context->trailing_spaces));
		    break;
		}
	    }

	    /*
	     * Mismatch - recover.
	     */
	    context->leading_spaces = 0;
	    context->trailing_spaces = 0;
	    if (((testtag->contents != SGML_LITTERAL &&
		  (testtag->flags & Tgf_strict)) ||
		 (context->state == S_pcdata &&
		  (testtag->flags & (Tgf_strict | Tgf_endO)))) &&
		(testlast > -1 &&
		 (c == '>' || testlast > 0 || IsNmStart(c)))) {
		context->state = S_end;
		string->size--;
		for (i = 0; i < string->size; i++)	/* remove '/' */
		    string->data[i] = string->data[i + 1];
		if ((string->size == 1) ? IsNmStart(c) : IsNmChar(c))
		    break;
		string->size--;
		goto top1;
	    }
	    if (context->state == S_pcdata &&
		(testtag->flags & (Tgf_strict | Tgf_endO)) &&
		(testlast < 0 && IsNmStart(c))) {
		context->state = S_tag;
		break;
	    }
	    /*
	     * If Mismatch:  recover string literally.
	     */
	    PUTC('<');
	    for (i = 0; i < string->size - 1; i++)	/* recover, except last c */
		PUTC(string->data[i]);
	    string->size = 0;
	    context->state = S_text;
	    goto top1;		/* to recover last c */
	}
	break;

	/*
	 * Character reference (numeric entity) or named entity.
	 */
    case S_ero:
	if (c == '#') {
	    /*
	     * Setting up for possible numeric entity.
	     */
	    context->state = S_cro;	/* &# is Char Ref Open */
	    break;
	}
	context->state = S_entity;	/* Fall through! */

	/*
	 * Handle possible named entity.
	 */
    case S_entity:
	if (TOASCII(unsign_c) < 127 && (string->size ?	/* S/390 -- gil -- 1029 */
					isalnum(UCH(c)) : isalpha(UCH(c)))) {
	    /* Should probably use IsNmStart/IsNmChar above (is that right?),
	       but the world is not ready for that - there's &nbsp: (note
	       colon!) and stuff around. */
	    /*
	     * Accept valid ASCII character.  - FM
	     */
	    HTChunkPutc(string, c);
	} else if (string->size == 0) {
	    /*
	     * It was an ampersand that's just text, so output the ampersand
	     * and recycle this character.  - FM
	     */
#ifdef USE_PRETTYSRC
	    if (psrc_view)
		PSRCSTART(badseq);
#endif
	    PUTC('&');
#ifdef USE_PRETTYSRC
	    if (psrc_view)
		PSRCSTOP(badseq);
#endif
	    context->state = S_text;
	    goto top1;
	} else {
	    /*
	     * Terminate entity name and try to handle it.  - FM
	     */
	    HTChunkTerminate(string);
#ifdef USE_PRETTYSRC
	    entity_string = string->data;
#endif
	    /* S/390 -- gil -- 1039 */
	    /* CTRACE((tfp, "%s: %d: %s\n", __FILE__, __LINE__, string->data)); */
	    if (!strcmp(string->data, "zwnj") &&
		(!context->element_stack ||
		 (context->element_stack->tag &&
		  context->element_stack->tag->contents == SGML_MIXED))) {
		/*
		 * Handle zwnj (8204) as <WBR>.  - FM
		 */
		char temp[8];

		CTRACE((tfp,
			"SGML_character: Handling 'zwnj' entity as 'WBR' element.\n"));

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
	     * Don't eat the terminator if we didn't find the entity name and
	     * therefore sent the raw string via handle_entity(), or if the
	     * terminator is not the "standard" semi-colon for HTML.  - FM
	     */
#ifdef USE_PRETTYSRC
	    if (psrc_view && FoundEntity && c == ';') {
		PSRCSTART(entity);
		PUTC(c);
		PSRCSTOP(entity);
	    }
#endif
	    if (!FoundEntity || c != ';')
		goto top1;
	}
	break;

	/*
	 * Check for a numeric entity.
	 */
    case S_cro:
	if (TOASCII(unsign_c) < 127 && TOLOWER(UCH(c)) == 'x') {	/* S/390 -- gil -- 1060 */
	    context->isHex = TRUE;
	    context->state = S_incro;
	} else if (TOASCII(unsign_c) < 127 && isdigit(UCH(c))) {
	    /*
	     * Accept only valid ASCII digits.  - FM
	     */
	    HTChunkPutc(string, c);	/* accumulate a character NUMBER */
	    context->isHex = FALSE;
	    context->state = S_incro;
	} else if (string->size == 0) {
	    /*
	     * No 'x' or digit following the "&#" so recover them and recycle
	     * the character.  - FM
	     */
#ifdef USE_PRETTYSRC
	    if (psrc_view)
		PSRCSTART(badseq);
#endif
	    PUTC('&');
	    PUTC('#');
#ifdef USE_PRETTYSRC
	    if (psrc_view)
		PSRCSTOP(badseq);
#endif
	    context->state = S_text;
	    goto top1;
	}
	break;

	/*
	 * Handle a numeric entity.
	 */
    case S_incro:
	/* S/390 -- gil -- 1075 */
	if ((TOASCII(unsign_c) < 127) &&
	    (context->isHex
	     ? isxdigit(UCH(c))
	     : isdigit(UCH(c)))) {
	    /*
	     * Accept only valid hex or ASCII digits.  - FM
	     */
	    HTChunkPutc(string, c);	/* accumulate a character NUMBER */
	} else if (string->size == 0) {
	    /*
	     * No hex digit following the "&#x" so recover them and recycle the
	     * character.  - FM
	     */
#ifdef USE_PRETTYSRC
	    if (psrc_view)
		PSRCSTART(badseq);
#endif
	    PUTS("&#x");
#ifdef USE_PRETTYSRC
	    if (psrc_view)
		PSRCSTOP(badseq);
#endif
	    context->isHex = FALSE;
	    context->state = S_text;
	    goto top1;
	} else {
	    /*
	     * Terminate the numeric entity and try to handle it.  - FM
	     */
	    UCode_t code;
	    int i;

	    HTChunkTerminate(string);
#ifdef USE_PRETTYSRC
	    entity_string = string->data;
#endif
	    if (UCScanCode(&code, string->data, context->isHex)) {

/* =============== work in ASCII below here ===============  S/390 -- gil -- 1092 */
		if (AssumeCP1252(context)) {
		    code = LYcp1252ToUnicode(code);
		}
		/*
		 * Check for special values.  - FM
		 */
		if ((code == 8204) &&
		    (!context->element_stack ||
		     (context->element_stack->tag &&
		      context->element_stack->tag->contents == SGML_MIXED))) {
		    /*
		     * Handle zwnj (8204) as <WBR>.  - FM
		     */
		    char temp[8];

		    CTRACE((tfp,
			    "SGML_character: Handling '8204' (zwnj) reference as 'WBR' element.\n"));

		    /*
		     * Include the terminator if it is not the standard
		     * semi-colon.  - FM
		     */
		    if (c != ';') {
			sprintf(temp, "<WBR>%c", c);
		    } else {
			sprintf(temp, "<WBR>");
		    }
		    /*
		     * Add the replacement string to the recover buffer for
		     * processing.  - FM
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
		     * We handled the value as a special character, so recycle
		     * the terminator or break.  - FM
		     */
#ifdef USE_PRETTYSRC
		    if (psrc_view) {
			PSRCSTART(entity);
			PUTS((context->isHex ? "&#x" : "&#"));
			PUTS(entity_string);
			if (c == ';')
			    PUTC(';');
			PSRCSTOP(entity);
		    }
#endif
		    string->size = 0;
		    context->isHex = FALSE;
		    context->state = S_text;
		    if (c != ';')
			goto top1;
		    break;
		}
		/*
		 * Seek a translation from the chartrans tables.
		 */
		if ((uck = UCTransUniChar(code,
					  context->outUCLYhndl)) >= 32 &&
		    uck < 256 &&
		    (uck < 127 ||
		     uck >= LYlowest_eightbit[context->outUCLYhndl])) {
#ifdef USE_PRETTYSRC
		    if (!psrc_view) {
#endif
			PUTC(FROMASCII((char) uck));
#ifdef USE_PRETTYSRC
		    } else {
			put_pretty_number(context);
		    }
#endif
		} else if ((uck == -4 ||
			    (context->T.repl_translated_C0 &&
			     uck > 0 && uck < 32)) &&
		    /*
		     * Not found; look for replacement string.
		     */
			   (uck = UCTransUniCharStr(replace_buf, 60, code,
						    context->outUCLYhndl,
						    0) >= 0)) {
#ifdef USE_PRETTYSRC
		    if (psrc_view) {
			put_pretty_number(context);
		    } else
#endif
			PUTS(replace_buf);
		    /*
		     * If we're displaying UTF-8, try that now.  - FM
		     */
		} else if (context->T.output_utf8 && PUTUTF8(code)) {
		    ;		/* do nothing more */
		    /*
		     * Ignore 8205 (zwj), 8206 (lrm), and 8207 (rln), if we get
		     * to here.  - FM
		     */
		} else if (code == 8205 ||
			   code == 8206 ||
			   code == 8207) {
		    if (TRACE) {
			string->size--;
			LYStrNCpy(replace_buf,
				  string->data,
				  (string->size < 64 ? string->size : 63));
			fprintf(tfp,
				"SGML_character: Ignoring '%s%s'.\n",
				(context->isHex ? "&#x" : "&#"),
				replace_buf);
		    }
#ifdef USE_PRETTYSRC
		    if (psrc_view) {
			PSRCSTART(badseq);
			PUTS((context->isHex ? "&#x" : "&#"));
			PUTS(entity_string);
			if (c == ';')
			    PUTC(';');
			PSRCSTOP(badseq);
		    }
#endif
		    string->size = 0;
		    context->isHex = FALSE;
		    context->state = S_text;
		    if (c != ';')
			goto top1;
		    break;
		    /*
		     * Show the numeric entity if we get to here and the value:
		     * (1) Is greater than 255 (but use ASCII characters for
		     * spaces or dashes).
		     * (2) Is less than 32, and not valid or we don't have
		     * HTCJK set.
		     * (3) Is 127 and we don't have HTPassHighCtrlRaw or HTCJK
		     * set.
		     * (4) Is 128 - 159 and we don't have HTPassHighCtrlNum
		     * set.
		     * - FM
		     */
		} else if ((code > 255) ||
			   (code < ' ' &&	/* S/390 -- gil -- 1140 */
			    code != '\t' && code != '\n' && code != '\r' &&
			    !IS_CJK_TTY) ||
			   (TOASCII(code) == 127 &&
			    !(HTPassHighCtrlRaw || IS_CJK_TTY)) ||
			   (TOASCII(code) > 127 && code < 160 &&
			    !HTPassHighCtrlNum)) {
		    /*
		     * Unhandled or illegal value.  Recover the "&#" or "&#x"
		     * and digit(s), and recycle the terminator.  - FM
		     */
#ifdef USE_PRETTYSRC
		    if (psrc_view) {
			PSRCSTART(badseq);
		    }
#endif
		    if (context->isHex) {
			PUTS("&#x");
			context->isHex = FALSE;
		    } else {
			PUTS("&#");
		    }
		    string->size--;
		    for (i = 0; i < string->size; i++)	/* recover */
			PUTC(string->data[i]);
#ifdef USE_PRETTYSRC
		    if (psrc_view) {
			PSRCSTOP(badseq);
		    }
#endif
		    string->size = 0;
		    context->isHex = FALSE;
		    context->state = S_text;
		    goto top1;
		} else if (TOASCII(code) < 161 ||	/* S/390 -- gil -- 1162 */
			   HTPassEightBitNum ||
			   IncludesLatin1Enc) {
		    /*
		     * No conversion needed.  - FM
		     */
#ifdef USE_PRETTYSRC
		    if (psrc_view) {
			put_pretty_number(context);
		    } else
#endif
			PUTC(FROMASCII((char) code));
		} else {
		    /*
		     * Handle as named entity.  - FM
		     */
		    code -= 160;
		    EntityName = HTMLGetEntityName(code);
		    if (EntityName && EntityName[0] != '\0') {
			string->size = 0;
			HTChunkPuts(string, EntityName);
			HTChunkTerminate(string);
			handle_entity(context, '\0');
			/*
			 * Add a semi-colon if something went wrong and
			 * handle_entity() sent the string.  - FM
			 */
			if (!FoundEntity) {
			    PUTC(';');
			}
		    } else {
			/*
			 * Our conversion failed, so recover the "&#" and
			 * digit(s), and recycle the terminator.  - FM
			 */
#ifdef USE_PRETTYSRC
			if (psrc_view)
			    PSRCSTART(badseq);
#endif
			if (context->isHex) {
			    PUTS("&#x");
			    context->isHex = FALSE;
			} else {
			    PUTS("&#");
			}
			string->size--;
			for (i = 0; i < string->size; i++)	/* recover */
			    PUTC(string->data[i]);
#ifdef USE_PRETTYSRC
			if (psrc_view)
			    PSRCSTOP(badseq);
#endif
			string->size = 0;
			context->isHex = FALSE;
			context->state = S_text;
			goto top1;
		    }
		}
		/*
		 * If we get to here, we succeeded.  Hoorah!!!  - FM
		 */
		string->size = 0;
		context->isHex = FALSE;
		context->state = S_text;
		/*
		 * Don't eat the terminator if it's not the "standard"
		 * semi-colon for HTML.  - FM
		 */
		if (c != ';') {
		    goto top1;
		}
	    } else {
		/*
		 * Not an entity, and don't know why not, so add the terminator
		 * to the string, output the "&#" or "&#x", and process the
		 * string via the recover element.  - FM
		 */
		string->size--;
		HTChunkPutc(string, c);
		HTChunkTerminate(string);
#ifdef USE_PRETTYSRC
		if (psrc_view)
		    PSRCSTART(badseq);
#endif
		if (context->isHex) {
		    PUTS("&#x");
		    context->isHex = FALSE;
		} else {
		    PUTS("&#");
		}
#ifdef USE_PRETTYSRC
		if (psrc_view)
		    PSRCSTOP(badseq);
#endif
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
	 * Tag
	 */
    case S_tag:		/* new tag */
	if (TOASCII(unsign_c) < 127 && (string->size ?	/* S/390 -- gil -- 1179 */
					IsNmChar(c) : IsNmStart(c))) {
	    /*
	     * Add valid ASCII character.  - FM
	     */
	    HTChunkPutc(string, c);
	} else if (c == '!' && !string->size) {		/* <! */
	    /*
	     * Terminate and set up for possible comment, identifier,
	     * declaration, or marked section.  - FM
	     */
	    context->state = S_exclamation;
	    context->lead_exclamation = TRUE;
	    context->doctype_bracket = FALSE;
	    context->first_bracket = FALSE;
	    HTChunkPutc(string, c);
	    break;
	} else if (!string->size &&
		   (TOASCII(unsign_c) <= 160 &&		/* S/390 -- gil -- 1196 */
		    (c != '/' && c != '?' && c != '_' && c != ':'))) {
	    /*
	     * '<' must be followed by an ASCII letter to be a valid start tag. 
	     * Here it isn't, nor do we have a '/' for an end tag, nor one of
	     * some other characters with a special meaning for SGML or which
	     * are likely to be legal Name Start characters in XML or some
	     * other extension.  So recover the '<' and following character as
	     * data.  - FM & KW
	     */
	    context->state = S_text;
#ifdef USE_PRETTYSRC
	    if (psrc_view)
		PSRCSTART(badseq);
#endif
	    PUTC('<');
#ifdef USE_PRETTYSRC
	    if (psrc_view)
		PSRCSTOP(badseq);
#endif
	    goto top1;
	} else {		/* End of tag name */
	    /*
	     * Try to handle tag.  - FM
	     */
	    HTTag *t;

	    if (c == '/') {
		if (string->size == 0) {
		    context->state = S_end;
		    break;
		}
		CTRACE((tfp, "SGML: `<%.*s/' found!\n", string->size, string->data));
	    }
	    HTChunkTerminate(string);

	    t = SGMLFindTag(dtd, string->data);
	    if (t == context->unknown_tag &&
		((c == ':' &&
		  string->size == 4 && 0 == strcasecomp(string->data, "URL")) ||
		 (string->size > 4 && 0 == strncasecomp(string->data, "URL:", 4)))) {
		/*
		 * Treat <URL:  as text rather than a junk tag, so we display
		 * it and the URL (Lynxism 8-).  - FM
		 */
#ifdef USE_PRETTYSRC
		if (psrc_view)
		    PSRCSTART(badseq);
#endif
		PUTC('<');
		PUTS(string->data);	/* recover */
		PUTC(c);
#ifdef USE_PRETTYSRC
		if (psrc_view)
		    PSRCSTOP(badseq);
#endif
		CTRACE((tfp, "SGML: Treating <%s%c as text\n",
			string->data, c));
		string->size = 0;
		context->state = S_text;
		break;
	    }
	    if (c == '/' && t) {
		/*
		 * Element name was ended by '/'.  Remember the tag that ended
		 * thusly, we'll interpret this as either an indication of an
		 * empty element (if '>' follows directly) or do some
		 * SGMLshortref-ish treatment.  - kw
		 */
		context->slashedtag = t;
	    }
	    if (!t) {
		if (c == '?' && string->size <= 1) {
		    CTRACE((tfp, "SGML: Found PI, looking for '>'\n"));
#ifdef USE_PRETTYSRC
		    if (psrc_view) {
			PSRCSTART(abracket);
			PUTS("<?");
			PSRCSTOP(abracket);
		    }
#endif
		    string->size = 0;
		    context->state = S_pi;
		    HTChunkPutc(string, c);
		    break;
		}
		CTRACE((tfp, "SGML: *** Invalid element %s\n",
			string->data));

#ifdef USE_PRETTYSRC
		if (psrc_view) {
		    PSRCSTART(abracket);
		    PUTC('<');
		    PSRCSTOP(abracket);
		    PSRCSTART(badtag);
		    transform_tag(context, string);
		    PUTS(string->data);
		    if (c == '>') {
			PSRCSTOP(badtag);
			PSRCSTART(abracket);
			PUTC('>');
			PSRCSTOP(abracket);
		    } else {
			PUTC(c);
		    }
		}
#endif
		context->state = (c == '>') ? S_text : S_junk_tag;
		break;
	    } else if (t == context->unknown_tag) {
		CTRACE((tfp, "SGML: *** Unknown element \"%s\"\n",
			string->data));
		/*
		 * Fall through and treat like valid tag for attribute parsing. 
		 * - KW
		 */

	    }
	    context->current_tag = t;

#ifdef USE_PRETTYSRC
	    if (psrc_view) {
		PSRCSTART(abracket);
		PUTC('<');
		PSRCSTOP(abracket);
		if (t != context->unknown_tag)
		    PSRCSTART(tag);
		else
		    PSRCSTART(badtag);
		transform_tag(context, string);
		PUTS(string->data);
		if (t != context->unknown_tag)
		    PSRCSTOP(tag);
		else
		    PSRCSTOP(badtag);
	    }
	    if (!psrc_view)	/*don't waste time */
#endif
	    {
		/*
		 * Clear out attributes.
		 */
		memset((void *) context->present, 0, sizeof(BOOL) *
		         (unsigned) (context->current_tag->number_of_attributes));
	    }

	    string->size = 0;
	    context->current_attribute_number = INVALID;
#ifdef USE_PRETTYSRC
	    if (psrc_view) {
		if (c == '>' || c == '<' || (c == '/' && context->slashedtag)) {
		    if (c != '<') {
			PSRCSTART(abracket);
			PUTC(c);
			PSRCSTOP(abracket);
			context->state = (c == '>') ? S_text : S_tagname_slash;
		    } else {
			context->state = S_tag;
		    }
		} else {
		    if (!WHITE(c))
			PUTC(c);
		    context->state = S_tag_gap;
		}
	    } else
#endif
	    if (c == '>' || c == '<' || (c == '/' && context->slashedtag)) {
		if (context->current_tag->name)
		    start_element(context);
		context->state = (c == '>') ? S_text :
		    (c == '<') ? S_tag : S_tagname_slash;
	    } else {
		context->state = S_tag_gap;
	    }
	}
	break;

    case S_exclamation:
	if (context->lead_exclamation && c == '-') {
	    /*
	     * Set up for possible comment.  - FM
	     */
	    context->lead_exclamation = FALSE;
	    context->first_dash = TRUE;
	    HTChunkPutc(string, c);
	    break;
	}
	if (context->lead_exclamation && c == '[') {
	    /*
	     * Set up for possible marked section.  - FM
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
	     * Set up to handle comment.  - FM
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
	     * Try to handle identifier.  - FM
	     */
	    HTChunkTerminate(string);
#ifdef USE_PRETTYSRC
	    if (psrc_view) {
		PSRCSTART(sgmlspecial);
		PUTC('<');
		PUTS(string->data);
		PUTC('>');
		PSRCSTOP(sgmlspecial);
	    } else
#endif
		handle_identifier(context);
	    string->size = 0;
	    context->state = S_text;
	    break;
	}
	if (WHITE(c)) {
	    if (string->size == 8 &&
		!strncasecomp(string->data, "!DOCTYPE", 8)) {
		/*
		 * Set up for DOCTYPE declaration.  - FM
		 */
		HTChunkPutc(string, c);
		context->doctype_bracket = FALSE;
		context->state = S_doctype;
		break;
	    }
	    if (string->size == 7 &&
		!strncasecomp(string->data, "!ENTITY", 7)) {
		/*
		 * Set up for ENTITY declaration.  - FM
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
		 * Set up for ELEMENT declaration.  - FM
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
		 * Set up for ATTLIST declaration.  - FM
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
	     * Any '>' terminates.  - FM
	     */
	    if (c == '>') {
		HTChunkTerminate(string);
#ifdef USE_PRETTYSRC
		if (psrc_view) {
		    PSRCSTART(comm);
		    PUTC('<');
		    PUTS_TR(string->data);
		    PUTC('>');
		    PSRCSTOP(comm);
		} else
#endif
		    handle_comment(context);
		string->size = 0;
		context->end_comment = FALSE;
		context->first_dash = FALSE;
		context->state = S_text;
		break;
	    }
	    goto S_comment_put_c;
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
		 * Validly treat '--' pairs as successive comments (for
		 * minimal, any "--WHITE>" terminates).  - FM
		 */
		context->end_comment = FALSE;
	    break;
	}
	if (context->end_comment && c == '>') {
	    /*
	     * Terminate and handle the comment.  - FM
	     */
	    HTChunkTerminate(string);
#ifdef USE_PRETTYSRC
	    if (psrc_view) {
		PSRCSTART(comm);
		PUTC('<');
		PUTS_TR(string->data);
		PUTC('>');
		PSRCSTOP(comm);
	    } else
#endif
		handle_comment(context);
	    string->size = 0;
	    context->end_comment = FALSE;
	    context->first_dash = FALSE;
	    context->state = S_text;
	    break;
	}
	context->first_dash = FALSE;
	if (context->end_comment && !isspace(UCH(c)))
	    context->end_comment = FALSE;

      S_comment_put_c:
	if (context->T.decode_utf8 &&
	    *context->utf_buf) {
	    HTChunkPuts(string, context->utf_buf);
	    context->utf_buf_p = context->utf_buf;
	    *(context->utf_buf_p) = '\0';
	} else if (!IS_CJK_TTY &&
		   (context->T.output_utf8 ||
		    context->T.trans_from_uni)) {
	    if (clong == 0xfffd && saved_char_in &&
		HTPassEightBitRaw &&
		saved_char_in >=
		LYlowest_eightbit[context->outUCLYhndl]) {
		HTChunkPutUtf8Char(string,
				   (UCode_t) (0xf000 | saved_char_in));
	    } else {
		HTChunkPutUtf8Char(string, clong);
	    }
	} else if (saved_char_in && context->T.use_raw_char_in) {
	    HTChunkPutc(string, saved_char_in);
	} else {
	    HTChunkPutc(string, c);
	}
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
#ifdef USE_PRETTYSRC
	    if (psrc_view) {
		PSRCSTART(sgmlspecial);
		PUTC('<');
		PUTS(string->data);
		PUTC('>');
		PSRCSTOP(sgmlspecial);
	    } else
#endif
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
#ifdef USE_PRETTYSRC
	    if (psrc_view) {
		PSRCSTART(sgmlspecial);
		PUTC('<');
		PUTS(string->data);
		PUTC('>');
		PSRCSTOP(sgmlspecial);
	    } else
#endif
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
#ifdef USE_PRETTYSRC
	    if (psrc_view) {
		PSRCSTART(sgmlspecial);
		PUTC('<');
		PUTS(string->data);
		PUTC('>');
		PSRCSTOP(sgmlspecial);
	    } else
#endif
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
#ifdef USE_PRETTYSRC
	    if (psrc_view) {
		PSRCSTART(sgmlspecial);
		PUTC('<');
		PUTS(string->data);
		PUTC('>');
		PSRCSTOP(sgmlspecial);
	    } else
#endif
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
#ifdef USE_PRETTYSRC
	    if (psrc_view) {
		PSRCSTART(sgmlspecial);
		PUTC('<');
		PUTS(string->data);
		PUTC('>');
		PSRCSTOP(sgmlspecial);
	    } else
#endif
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
	if (WHITE(c)) {
	    /* PUTC(c); - no, done as special case */
	    break;		/* Gap between attributes */
	}
	if (c == '>') {		/* End of tag */
#ifdef USE_PRETTYSRC
	    if (!psrc_view)
#endif
		if (context->current_tag->name)
		    start_element(context);
#ifdef USE_PRETTYSRC
	    if (psrc_view) {
		PSRCSTART(abracket);
		PUTC('>');
		PSRCSTOP(abracket);
	    }
#endif
	    context->state = S_text;
	    break;
	}
	HTChunkPutc(string, c);
	context->state = S_attr;	/* Get attribute */
	break;

	/* accumulating value */
    case S_attr:
	if (WHITE(c) || (c == '>') || (c == '=')) {	/* End of word */
	    if ((c == '>')
		&& (string->size == 1)
		&& (string->data[0] == '/')) {
		if (context->extended_html
		    && ignore_when_empty(context->current_tag)) {
		    discard_empty(context);
		}
	    } else {
		HTChunkTerminate(string);
		handle_attribute_name(context, string->data);
	    }
#ifdef USE_PRETTYSRC
	    if (!psrc_view) {
#endif
		string->size = 0;
		if (c == '>') {	/* End of tag */
		    if (context->current_tag->name)
			start_element(context);
		    context->state = S_text;
		    break;
		}
#ifdef USE_PRETTYSRC
	    } else {
		PUTC(' ');
		if (context->current_attribute_number == INVALID)
		    PSRCSTART(badattr);
		else
		    PSRCSTART(attrib);
		if (attrname_transform != 1) {
		    if (attrname_transform == 0)
			LYLowerCase(string->data);
		    else
			LYUpperCase(string->data);
		}
		PUTS(string->data);
		if (c == '=' || WHITE(c))
		    PUTC(c);
		if (c == '=' || c == '>') {
		    if (context->current_attribute_number == INVALID) {
			PSRCSTOP(badattr);
		    } else {
			PSRCSTOP(attrib);
		    }
		}
		if (c == '>') {
		    PSRCSTART(abracket);
		    PUTC('>');
		    PSRCSTOP(abracket);
		    context->state = S_text;
		    break;
		}
		string->size = 0;
	    }
#endif
	    context->state = (c == '=' ? S_equals : S_attr_gap);
	} else {
	    HTChunkPutc(string, c);
	}
	break;

    case S_attr_gap:		/* Expecting attribute or '=' or '>' */
	if (WHITE(c)) {
	    PRETTYSRC_PUTC(c);
	    break;		/* Gap after attribute */
	}
	if (c == '>') {		/* End of tag */
#ifdef USE_PRETTYSRC
	    if (psrc_view) {
		if (context->current_attribute_number == INVALID) {
		    PSRCSTOP(badattr);
		} else {
		    PSRCSTOP(attrib);
		}
		PSRCSTART(abracket);
		PUTC('>');
		PSRCSTOP(abracket);
	    } else
#endif
	    if (context->current_tag->name)
		start_element(context);
	    context->state = S_text;
	    break;
	} else if (c == '=') {
#ifdef USE_PRETTYSRC
	    if (psrc_view) {
		PUTC('=');
		if (context->current_attribute_number == INVALID) {
		    PSRCSTOP(badattr);
		} else {
		    PSRCSTOP(attrib);
		}
	    }
#endif
	    context->state = S_equals;
	    break;
	}
	HTChunkPutc(string, c);
	context->state = S_attr;	/* Get next attribute */
	break;

    case S_equals:		/* After attr = */
	if (WHITE(c)) {
	    PRETTYSRC_PUTC(c);
	    break;		/* Before attribute value */
	}
	if (c == '>') {		/* End of tag */
	    CTRACE((tfp, "SGML: found = but no value\n"));
#ifdef USE_PRETTYSRC
	    if (psrc_view) {
		PSRCSTART(abracket);
		PUTC('>');
		PSRCSTOP(abracket);
	    } else
#endif
	    if (context->current_tag->name)
		start_element(context);
	    context->state = S_text;
	    break;

	} else if (c == '\'') {
#ifdef USE_PRETTYSRC
	    if (psrc_view) {
		PSRCSTART(attrval);
		PUTC(c);
	    }
#endif
	    context->state = S_squoted;
	    break;

	} else if (c == '"') {
#ifdef USE_PRETTYSRC
	    if (psrc_view) {
		PSRCSTART(attrval);
		PUTC(c);
	    }
#endif
	    context->state = S_dquoted;
	    break;
	}
#ifdef USE_PRETTYSRC
	if (psrc_view)
	    PSRCSTART(attrval);
#endif
	context->state = S_value;
	/*  no break!  fall through to S_value and process current `c`   */

    case S_value:
	if (WHITE(c) || (c == '>')) {	/* End of word */
	    HTChunkTerminate(string);
#ifdef USE_PRETTYSRC
	    if (!end_if_prettysrc(context, string, 0))
#endif
	    {
#ifdef CJK_EX			/* Quick hack. - JH7AYN */
		if (IS_CJK_TTY) {
		    if (string->data[0] == '$') {
			if (string->data[1] == 'B' || string->data[1] == '@') {
			    char *jis_buf = 0;

			    HTSprintf0(&jis_buf, "\033%s", string->data);
			    TO_EUC((const unsigned char *) jis_buf,
				   (unsigned char *) string->data);
			    FREE(jis_buf);
			}
		    }
		}
#endif
		handle_attribute_value(context, string->data);
	    }
	    string->size = 0;
	    if (c == '>') {	/* End of tag */
#ifdef USE_PRETTYSRC
		if (psrc_view) {
		    PSRCSTART(abracket);
		    PUTC('>');
		    PSRCSTOP(abracket);
		} else
#endif
		if (context->current_tag->name)
		    start_element(context);
		context->state = S_text;
		break;
	    } else
		context->state = S_tag_gap;
	} else if (context->T.decode_utf8 &&
		   *context->utf_buf) {
	    HTChunkPuts(string, context->utf_buf);
	    context->utf_buf_p = context->utf_buf;
	    *(context->utf_buf_p) = '\0';
	} else if (!IS_CJK_TTY &&
		   (context->T.output_utf8 ||
		    context->T.trans_from_uni)) {
	    if (clong == 0xfffd && saved_char_in &&
		HTPassEightBitRaw &&
		saved_char_in >=
		LYlowest_eightbit[context->outUCLYhndl]) {
		HTChunkPutUtf8Char(string,
				   (UCode_t) (0xf000 | saved_char_in));
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
	    HTChunkTerminate(string);
#ifdef USE_PRETTYSRC
	    if (!end_if_prettysrc(context, string, '\''))
#endif
		handle_attribute_value(context, string->data);
	    string->size = 0;
	    context->state = S_tag_gap;
	} else if (TOASCII(c) == '\033') {	/* S/390 -- gil -- 1213 */
	    /*
	     * Setting up for possible single quotes in CJK escape sequences. 
	     * - Takuya ASADA (asada@three-a.co.jp)
	     */
	    context->state = S_esc_sq;
	    HTChunkPutc(string, c);
	} else if (context->T.decode_utf8 &&
		   *context->utf_buf) {
	    HTChunkPuts(string, context->utf_buf);
	    context->utf_buf_p = context->utf_buf;
	    *(context->utf_buf_p) = '\0';
	} else if (!IS_CJK_TTY &&
		   (context->T.output_utf8 ||
		    context->T.trans_from_uni)) {
	    if (clong == 0xfffd && saved_char_in &&
		HTPassEightBitRaw &&
		saved_char_in >=
		LYlowest_eightbit[context->outUCLYhndl]) {
		HTChunkPutUtf8Char(string,
				   (UCode_t) (0xf000 | saved_char_in));
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
	if (c == '"' ||		/* Valid end of attribute value */
	    (soft_dquotes &&	/*  If emulating old Netscape bug, treat '>' */
	     c == '>')) {	/*  as a co-terminator of dquoted and tag    */
	    HTChunkTerminate(string);
#ifdef USE_PRETTYSRC
	    if (!end_if_prettysrc(context, string, (char) c))
#endif
		handle_attribute_value(context, string->data);
	    string->size = 0;
	    context->state = S_tag_gap;
	    if (c == '>')	/* We emulated the Netscape bug, so we go  */
		goto top1;	/* back and treat it as the tag terminator */
	} else if (TOASCII(c) == '\033') {	/* S/390 -- gil -- 1230 */
	    /*
	     * Setting up for possible double quotes in CJK escape sequences. 
	     * - Takuya ASADA (asada@three-a.co.jp)
	     */
	    context->state = S_esc_dq;
	    HTChunkPutc(string, c);
	} else if (context->T.decode_utf8 &&
		   *context->utf_buf) {
	    HTChunkPuts(string, context->utf_buf);
	    context->utf_buf_p = context->utf_buf;
	    *(context->utf_buf_p) = '\0';
	} else if (!IS_CJK_TTY &&
		   (context->T.output_utf8 ||
		    context->T.trans_from_uni)) {
	    if (clong == 0xfffd && saved_char_in &&
		HTPassEightBitRaw &&
		saved_char_in >=
		LYlowest_eightbit[context->outUCLYhndl]) {
		HTChunkPutUtf8Char(string,
				   (UCode_t) (0xf000 | saved_char_in));
	    } else {
		HTChunkPutUtf8Char(string, clong);
	    }
	} else if (saved_char_in && context->T.use_raw_char_in) {
	    HTChunkPutc(string, saved_char_in);
	} else {
	    HTChunkPutc(string, c);
	}
	break;

    case S_end:		/* </ */
	if (TOASCII(unsign_c) < 127 && (string->size ?	/* S/390 -- gil -- 1247 */
					IsNmChar(c) : IsNmStart(c))) {
	    HTChunkPutc(string, c);
	} else {		/* End of end tag name */
	    HTTag *t = 0;

#ifdef USE_PRETTYSRC
	    BOOL psrc_tagname_processed = FALSE;
#endif

	    HTChunkTerminate(string);
	    if (!*string->data) {	/* Empty end tag */
		if (context->element_stack)
		    t = context->element_stack->tag;
	    } else {
		t = SGMLFindTag(dtd, string->data);
	    }
	    if (!t || t == context->unknown_tag) {
		CTRACE((tfp, "Unknown end tag </%s>\n", string->data));
#ifdef USE_PRETTYSRC
		if (psrc_view) {
		    PSRCSTART(abracket);
		    PUTS("</");
		    PSRCSTOP(abracket);
		    PSRCSTART(badtag);
		    transform_tag(context, string);
		    PUTS(string->data);
		    if (c != '>') {
			PUTC(c);
		    } else {
			PSRCSTOP(badtag);
			PSRCSTART(abracket);
			PUTC('>');
			PSRCSTOP(abracket);
		    }
		    psrc_tagname_processed = TRUE;
		}
	    } else if (psrc_view) {
#endif
	    } else {
		BOOL tag_OK = (BOOL) (c == '>' || WHITE(c));
		HTMLElement e = TAGNUM_OF_TAGP(t);
		int branch = 2;	/* it can be 0,1,2 */

		context->current_tag = t;
		if (HAS_ALT_TAGNUM(TAGNUM_OF_TAGP(t)) &&
		    context->element_stack &&
		    ALT_TAGP(t) == context->element_stack->tag)
		    context->element_stack->tag = NORMAL_TAGP(context->element_stack->tag);

		if (tag_OK && Old_DTD) {
		    switch (e) {
		    case HTML_DD:
		    case HTML_DT:
		    case HTML_LI:
		    case HTML_LH:
		    case HTML_TD:
		    case HTML_TH:
		    case HTML_TR:
		    case HTML_THEAD:
		    case HTML_TFOOT:
		    case HTML_TBODY:
		    case HTML_COLGROUP:
			branch = 0;
			break;

		    case HTML_A:
		    case HTML_B:
		    case HTML_BLINK:
		    case HTML_CITE:
		    case HTML_EM:
		    case HTML_FONT:
		    case HTML_FORM:
		    case HTML_I:
		    case HTML_P:
		    case HTML_STRONG:
		    case HTML_TT:
		    case HTML_U:
			branch = 1;
			break;
		    default:
			break;
		    }
		}

		/*
		 * Just handle ALL end tags normally :-) - kw
		 */
		if (!Old_DTD) {
		    end_element(context, context->current_tag);
		} else if (tag_OK && (branch == 0)) {
		    /*
		     * Don't treat these end tags as invalid, nor act on them. 
		     * - FM
		     */
		    CTRACE((tfp, "SGML: `</%s%c' found!  Ignoring it.\n",
			    string->data, c));
		    string->size = 0;
		    context->current_attribute_number = INVALID;
		    if (c != '>') {
			context->state = S_junk_tag;
		    } else {
			context->current_tag = NULL;
			context->state = S_text;
		    }
		    break;
		} else if (tag_OK && (branch == 1)) {
		    /*
		     * Handle end tags for container elements declared as
		     * SGML_EMPTY to prevent "expected tag substitution" but
		     * still processed via HTML_end_element() in HTML.c with
		     * checks there to avoid throwing the HTML.c stack out of
		     * whack (Ugh, what a hack!  8-).  - FM
		     */
		    if (context->inSELECT) {
			/*
			 * We are in a SELECT block.  - FM
			 */
			if (strcasecomp(string->data, "FORM")) {
			    /*
			     * It is not at FORM end tag, so ignore it.  - FM
			     */
			    CTRACE((tfp,
				    "SGML: ***Ignoring end tag </%s> in SELECT block.\n",
				    string->data));
			} else {
			    /*
			     * End the SELECT block and then handle the FORM
			     * end tag.  - FM
			     */
			    CTRACE((tfp,
				    "SGML: ***Faking SELECT end tag before </%s> end tag.\n",
				    string->data));
			    end_element(context,
					SGMLFindTag(context->dtd, "SELECT"));
			    CTRACE((tfp, "SGML: End </%s>\n", string->data));

#ifdef USE_PRETTYSRC
			    if (!psrc_view)	/* Don't actually call if viewing psrc - kw */
#endif
				(*context->actions->end_element)
				    (context->target,
				     (int) TAGNUM_OF_TAGP(context->current_tag),
				     &context->include);
			}
		    } else if (!strcasecomp(string->data, "P")) {
			/*
			 * Treat a P end tag like a P start tag (Ugh, what a
			 * hack!  8-).  - FM
			 */
			CTRACE((tfp,
				"SGML: `</%s%c' found!  Treating as '<%s%c'.\n",
				string->data, c, string->data, c));
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
			CTRACE((tfp, "SGML: End </%s>\n", string->data));

#ifdef USE_PRETTYSRC
			if (!psrc_view)		/* Don't actually call if viewing psrc - kw */
#endif
			    (*context->actions->end_element)
				(context->target,
				 (int) TAGNUM_OF_TAGP(context->current_tag),
				 &context->include);
		    }
		    string->size = 0;
		    context->current_attribute_number = INVALID;
		    if (c != '>') {
			context->state = S_junk_tag;
		    } else {
			context->current_tag = NULL;
			context->state = S_text;
		    }
		    break;
		} else {
		    /*
		     * Handle all other end tags normally.  - FM
		     */
		    end_element(context, context->current_tag);
		}
	    }

#ifdef USE_PRETTYSRC
	    if (psrc_view && !psrc_tagname_processed) {
		PSRCSTART(abracket);
		PUTS("</");
		PSRCSTOP(abracket);
		PSRCSTART(tag);
		if (tagname_transform != 1) {
		    if (tagname_transform == 0)
			LYLowerCase(string->data);
		    else
			LYUpperCase(string->data);
		}
		PUTS(string->data);
		PSRCSTOP(tag);
		if (c != '>') {
		    PSRCSTART(badtag);
		    PUTC(c);
		} else {
		    PSRCSTART(abracket);
		    PUTC('>');
		    PSRCSTOP(abracket);
		}
	    }
#endif

	    string->size = 0;
	    context->current_attribute_number = INVALID;
	    if (c != '>') {
		if (!WHITE(c))
		    CTRACE((tfp, "SGML: `</%s%c' found!\n", string->data, c));
		context->state = S_junk_tag;
	    } else {
		context->current_tag = NULL;
		context->state = S_text;
	    }
	}
	break;

    case S_esc:		/* Expecting '$'or '(' following CJK ESC. */
	if (c == '$') {
	    context->state = S_dollar;
	} else if (c == '(') {
	    context->state = S_paren;
	} else {
	    context->state = S_text;
	}
	PUTC(c);
	break;

    case S_dollar:		/* Expecting '@', 'B', 'A' or '(' after CJK "ESC$". */
	if (c == '@' || c == 'B' || c == 'A') {
	    context->state = S_nonascii_text;
	} else if (c == '(') {
	    context->state = S_dollar_paren;
	}
	PUTC(c);
	break;

    case S_dollar_paren:	/* Expecting 'C' after CJK "ESC$(". */
	if (c == 'C') {
	    context->state = S_nonascii_text;
	} else {
	    context->state = S_text;
	}
	PUTC(c);
	break;

    case S_paren:		/* Expecting 'B', 'J', 'T' or 'I' after CJK "ESC(". */
	if (c == 'B' || c == 'J' || c == 'T') {
	    context->state = S_text;
	} else if (c == 'I') {
	    context->state = S_nonascii_text;
	} else {
	    context->state = S_text;
	}
	PUTC(c);
	break;

    case S_nonascii_text:	/* Expecting CJK ESC after non-ASCII text. */
	if (TOASCII(c) == '\033') {	/* S/390 -- gil -- 1264 */
	    context->state = S_esc;
	}
	PUTC(c);
	if (c < 32)
	    context->state = S_text;
	break;

    case S_esc_sq:		/* Expecting '$'or '(' following CJK ESC. */
	if (c == '$') {
	    context->state = S_dollar_sq;
	} else if (c == '(') {
	    context->state = S_paren_sq;
	} else {
	    context->state = S_squoted;
	}
	HTChunkPutc(string, c);
	break;

    case S_dollar_sq:		/* Expecting '@', 'B', 'A' or '(' after CJK "ESC$". */
	if (c == '@' || c == 'B' || c == 'A') {
	    context->state = S_nonascii_text_sq;
	} else if (c == '(') {
	    context->state = S_dollar_paren_sq;
	}
	HTChunkPutc(string, c);
	break;

    case S_dollar_paren_sq:	/* Expecting 'C' after CJK "ESC$(". */
	if (c == 'C') {
	    context->state = S_nonascii_text_sq;
	} else {
	    context->state = S_squoted;
	}
	HTChunkPutc(string, c);
	break;

    case S_paren_sq:		/* Expecting 'B', 'J', 'T' or 'I' after CJK "ESC(". */
	if (c == 'B' || c == 'J' || c == 'T') {
	    context->state = S_squoted;
	} else if (c == 'I') {
	    context->state = S_nonascii_text_sq;
	} else {
	    context->state = S_squoted;
	}
	HTChunkPutc(string, c);
	break;

    case S_nonascii_text_sq:	/* Expecting CJK ESC after non-ASCII text. */
	if (TOASCII(c) == '\033') {	/* S/390 -- gil -- 1281 */
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

    case S_dollar_dq:		/* Expecting '@', 'B', 'A' or '(' after CJK "ESC$". */
	if (c == '@' || c == 'B' || c == 'A') {
	    context->state = S_nonascii_text_dq;
	} else if (c == '(') {
	    context->state = S_dollar_paren_dq;
	}
	HTChunkPutc(string, c);
	break;

    case S_dollar_paren_dq:	/* Expecting 'C' after CJK "ESC$(". */
	if (c == 'C') {
	    context->state = S_nonascii_text_dq;
	} else {
	    context->state = S_dquoted;
	}
	HTChunkPutc(string, c);
	break;

    case S_paren_dq:		/* Expecting 'B', 'J', 'T' or 'I' after CJK "ESC(". */
	if (c == 'B' || c == 'J' || c == 'T') {
	    context->state = S_dquoted;
	} else if (c == 'I') {
	    context->state = S_nonascii_text_dq;
	} else {
	    context->state = S_dquoted;
	}
	HTChunkPutc(string, c);
	break;

    case S_nonascii_text_dq:	/* Expecting CJK ESC after non-ASCII text. */
	if (TOASCII(c) == '\033') {	/* S/390 -- gil -- 1298 */
	    context->state = S_esc_dq;
	}
	HTChunkPutc(string, c);
	break;

    case S_junk_tag:
    case S_pi:
	if (c == '>') {
	    HTChunkTerminate(string);
#ifdef USE_PRETTYSRC
	    if (psrc_view) {
		if (context->state == S_junk_tag) {
		    PSRCSTOP(badtag);
		}
		PSRCSTART(abracket);
		PUTC('>');
		PSRCSTOP(abracket);
	    }
#endif
	    if (context->state == S_pi)
		handle_processing_instruction(context);
	    string->size = 0;
	    context->current_tag = NULL;
	    context->state = S_text;
	} else {
	    HTChunkPutc(string, c);
#ifdef USE_PRETTYSRC
	    if (psrc_view) {
		PUTC(c);
	    }
#endif
	}

    }				/* switch on context->state */
    CTRACE2(TRACE_SGML, (tfp, "SGML after  %s|%.*s|%c|\n",
			 state_name(context->state),
			 string->size,
			 NonNull(string->data),
			 UCH(c)));

  after_switch:
    /*
     * Check whether an external function has added anything to the include
     * buffer.  If so, move the new stuff to the beginning of active_include. 
     * - kw
     */
    if (context->include != NULL) {
	if (context->include[0] == '\0') {
	    FREE(context->include);
	} else {
	    if (context->active_include &&
		context->active_include[context->include_index] != '\0')
		StrAllocCat(context->include,
			    context->active_include + context->include_index);
	    FREE(context->active_include);
	    context->active_include = context->include;
	    context->include_index = 0;
	    context->include = NULL;
	}
    }

    /*
     * Check whether we've added anything to the recover buffer.  - FM
     */
    if (context->recover != NULL) {
	if (context->recover[context->recover_index] == '\0') {
	    FREE(context->recover);
	    context->recover_index = 0;
	} else {
	    c = UCH(context->recover[context->recover_index]);
	    context->recover_index++;
	    goto top;
	}
    }

    /*
     * Check whether an external function had added anything to the include
     * buffer; it should now be in active_include.  - FM / kw
     */
    if (context->active_include != NULL) {
	if (context->active_include[context->include_index] == '\0') {
	    FREE(context->active_include);
	    context->include_index = 0;
	} else {
	    if (context->current_tag_charset == UTF8_handle ||
		context->T.trans_from_uni) {
		/*
		 * If it looks like we would have fed UTF-8 to the next
		 * processing stage, assume that whatever we were fed back is
		 * in UTF-8 form, too.  This won't be always true for all uses
		 * of the include buffer, but it's a start.  - kw
		 */
		char *puni = context->active_include + context->include_index;

		c = UCH(*puni);
		clong = UCGetUniFromUtf8String(&puni);
		if (clong < 256 && clong >= 0) {
		    c = UCH((clong & 0xff));
		}
		saved_char_in = '\0';
		context->include_index = (int) (puni
						- context->active_include
						+ 1);
		goto top1;
	    } else {
		/*
		 * Otherwise assume no UTF-8 - do charset-naive processing and
		 * hope for the best.  - kw
		 */
		c = UCH(context->active_include[context->include_index]);
		context->include_index++;
		goto top;
	    }
	}
    }

    /*
     * Check whether an external function has added anything to the csi buffer. 
     * - FM
     */
    if (context->csi != NULL) {
	if (context->csi[context->csi_index] == '\0') {
	    FREE(context->csi);
	    context->csi_index = 0;
	} else {
	    c = UCH(context->csi[context->csi_index]);
	    context->csi_index++;
	    goto top;
	}
    }
}				/* SGML_character */

static void InferUtfFromBom(HTStream *context, int chndl)
{
    HTAnchor_setUCInfoStage(context->node_anchor, chndl,
			    UCT_STAGE_PARSER,
			    UCT_SETBY_PARSER);
    change_chartrans_handling(context);
}

/*
 * Avoid rewrite of SGML_character() to handle hypothetical case of UTF-16
 * webpages, by pretending that the data is UTF-8.
 */
static void SGML_widechar(HTStream *context, int ch)
{
    if (!UCPutUtf8_charstring(context, SGML_character, (UCode_t) ch)) {
	SGML_character(context, ch);
    }
}

static void SGML_write(HTStream *context, const char *str, int l)
{
    const char *p;
    const char *e = str + l;

    if (sgml_offset == 0) {
	if (l > 3
	    && !MemCmp(str, "\357\273\277", 3)) {
	    CTRACE((tfp, "SGML_write found UTF-8 BOM\n"));
	    InferUtfFromBom(context, UTF8_handle);
	    str += 3;
	} else if (l > 2) {
	    if (!MemCmp(str, "\377\376", 2)) {
		CTRACE((tfp, "SGML_write found UCS-2 LE BOM\n"));
		InferUtfFromBom(context, UTF8_handle);
		str += 2;
		context->T.ucs_mode = -1;
	    } else if (!MemCmp(str, "\376\377", 2)) {
		CTRACE((tfp, "SGML_write found UCS-2 BE BOM\n"));
		InferUtfFromBom(context, UTF8_handle);
		str += 2;
		context->T.ucs_mode = 1;
	    }
	}
    }
    switch (context->T.ucs_mode) {
    case -1:
	for (p = str; p < e; p += 2)
	    SGML_widechar(context, (UCH(p[1]) << 8) | UCH(p[0]));
	break;
    case 1:
	for (p = str; p < e; p += 2)
	    SGML_widechar(context, (UCH(p[0]) << 8) | UCH(p[1]));
	break;
    default:
	for (p = str; p < e; p++)
	    SGML_character(context, *p);
	break;
    }
}

static void SGML_string(HTStream *context, const char *str)
{
    SGML_write(context, str, (int) strlen(str));
}

/*_______________________________________________________________________
*/

/*	Structured Object Class
 *	-----------------------
 */
const HTStreamClass SGMLParser =
{
    "SGMLParser",
    SGML_free,
    SGML_abort,
    SGML_character,
    SGML_string,
    SGML_write,
};

/*	Create SGML Engine
 *	------------------
 *
 * On entry,
 *	dtd		represents the DTD, along with
 *	actions		is the sink for the data as a set of routines.
 *
 */

HTStream *SGML_new(const SGML_dtd * dtd,
		   HTParentAnchor *anchor,
		   HTStructured * target)
{
    HTStream *context = typecalloc(struct _HTStream);

    if (!context)
	outofmem(__FILE__, "SGML_begin");

    assert(context != NULL);

    context->isa = &SGMLParser;
    context->string = HTChunkCreate(128);	/* Grow by this much */
    context->dtd = dtd;
    context->target = target;
    context->actions = (const HTStructuredClass *) (((HTStream *) target)->isa);
    /* Ugh: no OO */
    context->unknown_tag = &HTTag_unrecognized;
    context->current_tag = context->slashedtag = NULL;
    context->state = S_text;
#ifdef CALLERDATA
    context->callerData = (void *) callerData;
#endif /* CALLERDATA */

    context->node_anchor = anchor;	/* Could be NULL? */
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
#ifdef CAN_SWITCH_DISPLAY_CHARSET	/* Allow a switch to a more suitable display charset */
    else if (anchor->UCStages
	     && anchor->UCStages->s[UCT_STAGE_PARSER].LYhndl >= 0
	     && anchor->UCStages->s[UCT_STAGE_PARSER].LYhndl != current_char_set) {
	int o = anchor->UCStages->s[UCT_STAGE_PARSER].LYhndl;

	anchor->UCStages->s[UCT_STAGE_PARSER].LYhndl = -1;	/* Force reset */
	HTAnchor_resetUCInfoStage(anchor, o, UCT_STAGE_PARSER,
	/* Preserve change this: */
				  anchor->UCStages->s[UCT_STAGE_PARSER].lock);
    }
#endif

    context->inUCI = HTAnchor_getUCInfoStage(anchor,
					     UCT_STAGE_PARSER);
    set_chartrans_handling(context, anchor, -1);

    context->recover = NULL;
    context->recover_index = 0;
    context->include = NULL;
    context->active_include = NULL;
    context->include_index = 0;
    context->url = NULL;
    context->csi = NULL;
    context->csi_index = 0;

#ifdef USE_PRETTYSRC
    if (psrc_view) {
	psrc_view = FALSE;
	mark_htext_as_source = TRUE;
	SGML_string(context,
		    "<HTML><HEAD><TITLE>source</TITLE></HEAD><BODY><PRE>");
	psrc_view = TRUE;
	psrc_convert_string = FALSE;
	sgml_in_psrc_was_initialized = TRUE;
    }
#endif

    sgml_offset = 0;
    return context;
}

/*
 * Return the offset within the document where we're parsing.  This is used
 * to help identify anchors which shift around while reparsing.
 */
int SGML_offset(void)
{
    int result = sgml_offset;

#ifdef USE_PRETTYSRC
    result += psrc_view;
#endif
    return result;
}

/*		Asian character conversion functions
 *		====================================
 *
 *	Added 24-Mar-96 by FM, based on:
 *
 ////////////////////////////////////////////////////////////////////////
Copyright (c) 1993 Electrotechnical Laboratory (ETL)

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
Author:		Yutaka Sato <ysato@etl.go.jp>
Description:
History:
	930923	extracted from codeconv.c of cosmos
///////////////////////////////////////////////////////////////////////
*/

static int TREAT_SJIS = 1;

void JISx0201TO0208_EUC(unsigned IHI,
			unsigned ILO,
			unsigned char *OHI,
			unsigned char *OLO)
{
    static const char *table[] =
    {
	"\241\243",		/* A1,A3 */
	"\241\326",		/* A1,D6 */
	"\241\327",		/* A1,D7 */
	"\241\242",		/* A1,A2 */
	"\241\246",		/* A1,A6 */
	"\245\362",		/* A5,F2 */
	"\245\241",		/* A5,A1 */
	"\245\243",		/* A5,A3 */
	"\245\245",		/* A5,A5 */
	"\245\247",		/* A5,A7 */
	"\245\251",		/* A5,A9 */
	"\245\343",		/* A5,E3 */
	"\245\345",		/* A5,E5 */
	"\245\347",		/* A5,E7 */
	"\245\303",		/* A5,C3 */
	"\241\274",		/* A1,BC */
	"\245\242",		/* A5,A2 */
	"\245\244",		/* A5,A4 */
	"\245\246",		/* A5,A6 */
	"\245\250",		/* A5,A8 */
	"\245\252",		/* A5,AA */
	"\245\253",		/* A5,AB */
	"\245\255",		/* A5,AD */
	"\245\257",		/* A5,AF */
	"\245\261",		/* A5,B1 */
	"\245\263",		/* A5,B3 */
	"\245\265",		/* A5,B5 */
	"\245\267",		/* A5,B7 */
	"\245\271",		/* A5,B9 */
	"\245\273",		/* A5,BB */
	"\245\275",		/* A5,BD */
	"\245\277",		/* A5,BF */
	"\245\301",		/* A5,C1 */
	"\245\304",		/* A5,C4 */
	"\245\306",		/* A5,C6 */
	"\245\310",		/* A5,C8 */
	"\245\312",		/* A5,CA */
	"\245\313",		/* A5,CB */
	"\245\314",		/* A5,CC */
	"\245\315",		/* A5,CD */
	"\245\316",		/* A5,CE */
	"\245\317",		/* A5,CF */
	"\245\322",		/* A5,D2 */
	"\245\325",		/* A5,D5 */
	"\245\330",		/* A5,D8 */
	"\245\333",		/* A5,DB */
	"\245\336",		/* A5,DE */
	"\245\337",		/* A5,DF */
	"\245\340",		/* A5,E0 */
	"\245\341",		/* A5,E1 */
	"\245\342",		/* A5,E2 */
	"\245\344",		/* A5,E4 */
	"\245\346",		/* A5,E6 */
	"\245\350",		/* A5,E8 */
	"\245\351",		/* A5,E9 */
	"\245\352",		/* A5,EA */
	"\245\353",		/* A5,EB */
	"\245\354",		/* A5,EC */
	"\245\355",		/* A5,ED */
	"\245\357",		/* A5,EF */
	"\245\363",		/* A5,F3 */
	"\241\253",		/* A1,AB */
	"\241\254"		/* A1,AC */
    };

    if ((IHI == 0x8E) && (ILO >= 0xA1) && (ILO <= 0xDF)) {
	*OHI = UCH(table[ILO - 0xA1][0]);
	*OLO = UCH(table[ILO - 0xA1][1]);
    } else {
	*OHI = UCH(IHI);
	*OLO = UCH(ILO);
    }
}

static int IS_SJIS_STR(const unsigned char *str)
{
    const unsigned char *s;
    unsigned char ch;
    int is_sjis = 0;

    s = str;
    while ((ch = *s++) != '\0') {
	if (ch & 0x80)
	    if (IS_SJIS(ch, *s, is_sjis))
		return 1;
    }
    return 0;
}

unsigned char *SJIS_TO_JIS1(unsigned HI,
			    unsigned LO,
			    unsigned char *JCODE)
{
    HI = UCH(HI - (unsigned) UCH((HI <= 0x9F) ? 0x71 : 0xB1));
    HI = UCH((HI << 1) + 1);
    if (0x7F < LO)
	LO--;
    if (0x9E <= LO) {
	LO = UCH(LO - UCH(0x7D));
	HI++;
    } else {
	LO = UCH(LO - UCH(0x1F));
    }
    JCODE[0] = UCH(HI);
    JCODE[1] = UCH(LO);
    return JCODE;
}

unsigned char *JIS_TO_SJIS1(unsigned HI,
			    unsigned LO,
			    unsigned char *SJCODE)
{
    if (HI & 1)
	LO = UCH(LO + UCH(0x1F));
    else
	LO = UCH(LO + UCH(0x7D));
    if (0x7F <= LO)
	LO++;

    HI = UCH(((HI - 0x21) >> 1) + 0x81);
    if (0x9F < HI)
	HI = UCH(HI + UCH(0x40));
    SJCODE[0] = UCH(HI);
    SJCODE[1] = UCH(LO);
    return SJCODE;
}

unsigned char *EUC_TO_SJIS1(unsigned HI,
			    unsigned LO,
			    unsigned char *SJCODE)
{
    if (HI == 0x8E) {
	unsigned char HI_data[2];
	unsigned char LO_data[2];

	HI_data[0] = UCH(HI);
	LO_data[0] = UCH(LO);
	JISx0201TO0208_EUC(HI, LO, HI_data, LO_data);
    }
    JIS_TO_SJIS1(UCH(HI & 0x7F), UCH(LO & 0x7F), SJCODE);
    return SJCODE;
}

void JISx0201TO0208_SJIS(unsigned I,
			 unsigned char *OHI,
			 unsigned char *OLO)
{
    unsigned char SJCODE[2];

    JISx0201TO0208_EUC(0x8E, I, OHI, OLO);
    JIS_TO_SJIS1(UCH(*OHI & 0x7F), UCH(*OLO & 0x7F), SJCODE);
    *OHI = SJCODE[0];
    *OLO = SJCODE[1];
}

unsigned char *SJIS_TO_EUC1(unsigned HI,
			    unsigned LO,
			    unsigned char *data)
{
    SJIS_TO_JIS1(HI, LO, data);
    data[0] |= 0x80;
    data[1] |= 0x80;
    return data;
}

unsigned char *SJIS_TO_EUC(unsigned char *src,
			   unsigned char *dst)
{
    unsigned char hi, lo, *sp, *dp;
    int in_sjis = 0;

    in_sjis = IS_SJIS_STR(src);
    for (sp = src, dp = dst; (hi = sp[0]) != '\0';) {
	lo = sp[1];
	if (TREAT_SJIS && IS_SJIS(hi, lo, in_sjis)) {
	    SJIS_TO_JIS1(hi, lo, dp);
	    dp[0] |= 0x80;
	    dp[1] |= 0x80;
	    dp += 2;
	    sp += 2;
	} else
	    *dp++ = *sp++;
    }
    *dp = 0;
    return dst;
}

unsigned char *EUC_TO_SJIS(unsigned char *src,
			   unsigned char *dst)
{
    unsigned char *sp, *dp;

    for (sp = src, dp = dst; *sp;) {
	if (*sp & 0x80) {
	    if (sp[1] && (sp[1] & 0x80)) {
		JIS_TO_SJIS1(UCH(sp[0] & 0x7F), UCH(sp[1] & 0x7F), dp);
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

#define Strcpy(a,b)	(strcpy((char*)a,(const char*)b),&a[strlen((const char*)a)])

unsigned char *EUC_TO_JIS(unsigned char *src,
			  unsigned char *dst,
			  const char *toK,
			  const char *toA)
{
    unsigned char kana_mode = 0;
    unsigned char cch;
    unsigned char *sp = src;
    unsigned char *dp = dst;
    int is_JIS = 0;

    while ((cch = *sp++) != '\0') {
	if (cch & 0x80) {
	    if (!IS_EUC(cch, *sp)) {
		if (cch == 0xA0 && is_JIS)	/* ignore NBSP */
		    continue;
		is_JIS++;
		*dp++ = cch;
		continue;
	    }
	    if (!kana_mode) {
		kana_mode = UCH(~kana_mode);
		dp = Strcpy(dp, toK);
	    }
	    if (*sp & 0x80) {
		*dp++ = UCH(cch & ~0x80);
		*dp++ = UCH(*sp++ & ~0x80);
	    }
	} else {
	    if (kana_mode) {
		kana_mode = UCH(~kana_mode);
		dp = Strcpy(dp, toA);
	    }
	    *dp++ = cch;
	}
    }
    if (kana_mode)
	dp = Strcpy(dp, toA);

    if (dp)
	*dp = 0;
    return dst;
}

#define	IS_JIS7(c1,c2)	(0x20<(c1)&&(c1)<0x7F && 0x20<(c2)&&(c2)<0x7F)
#define SO		('N'-0x40)
#define SI		('O'-0x40)

static int repair_JIS = 0;

static const unsigned char *repairJIStoEUC(const unsigned char *src,
					   unsigned char **dstp)
{
    const unsigned char *s;
    unsigned char *d, ch1, ch2;

    d = *dstp;
    s = src;
    while ((ch1 = s[0]) && (ch2 = s[1])) {
	s += 2;
	if (ch1 == '(')
	    if (ch2 == 'B' || ch2 == 'J') {
		*dstp = d;
		return s;
	    }
	if (!IS_JIS7(ch1, ch2))
	    return 0;

	*d++ = UCH(0x80 | ch1);
	*d++ = UCH(0x80 | ch2);
    }
    return 0;
}

unsigned char *TO_EUC(const unsigned char *jis,
		      unsigned char *euc)
{
    const unsigned char *s;
    unsigned char c, jis_stat;
    unsigned char *d;
    int to1B, to2B;
    int in_sjis = 0;
    static int nje;
    int n8bits;
    int is_JIS;

    nje++;
    n8bits = 0;
    s = jis;
    d = euc;
    jis_stat = 0;
    to2B = TO_2BCODE;
    to1B = TO_1BCODE;
    in_sjis = IS_SJIS_STR(jis);
    is_JIS = 0;

    while ((c = *s++) != '\0') {
	if (c == 0x80)
	    continue;		/* ignore it */
	if (c == 0xA0 && is_JIS)
	    continue;		/* ignore Non-breaking space */

	if (c == to2B && jis_stat == 0 && repair_JIS) {
	    if (*s == 'B' || *s == '@') {
		const unsigned char *ts;

		if ((ts = repairJIStoEUC(s + 1, &d)) != NULL) {
		    s = ts;
		    continue;
		}
	    }
	}
	if (c == CH_ESC) {
	    if (*s == to2B) {
		if ((s[1] == 'B') || (s[1] == '@')) {
		    jis_stat = 0x80;
		    s += 2;
		    is_JIS++;
		    continue;
		}
		jis_stat = 0;
	    } else if (*s == to1B) {
		jis_stat = 0;
		if ((s[1] == 'B') || (s[1] == 'J') || (s[1] == 'H')) {
		    s += 2;
		    continue;
		}
	    } else if (*s == ',') {	/* MULE */
		jis_stat = 0;
	    }
	}
	if (c & 0x80)
	    n8bits++;

	if (IS_SJIS(c, *s, in_sjis)) {
	    SJIS_TO_EUC1(c, *s, d);
	    d += 2;
	    s++;
	    is_JIS++;
	} else if (jis_stat) {
	    if (c <= 0x20 || 0x7F <= c) {
		*d++ = c;
		if (c == '\n')
		    jis_stat = 0;
	    } else {
		if (IS_JIS7(c, *s)) {
		    *d++ = jis_stat | c;
		    *d++ = jis_stat | *s++;
		} else
		    *d++ = c;
	    }
	} else {
	    if (n8bits == 0 && (c == SI || c == SO)) {
	    } else {
		*d++ = c;
	    }
	}
    }
    *d = 0;
    return euc;
}

#define non94(ch) ((ch) <= 0x20 || (ch) == 0x7F)

static int is_EUC_JP(unsigned char *euc)
{
    unsigned char *cp;
    int ch1, ch2;

    for (cp = euc; (ch1 = *cp) != '\0'; cp++) {
	if (ch1 & 0x80) {
	    ch2 = cp[1] & 0xFF;
	    if ((ch2 & 0x80) == 0) {
		/* sv1log("NOT_EUC1[%x][%x]\n",ch1,ch2); */
		return 0;
	    }
	    if (non94(ch1 & 0x7F) || non94(ch2 & 0x7F)) {
		/* sv1log("NOT_EUC2[%x][%x]\n",ch1,ch2); */
		return 0;
	    }
	    cp++;
	}
    }
    return 1;
}

void TO_SJIS(const unsigned char *arg,
	     unsigned char *sjis)
{
    unsigned char *euc;

    euc = typeMallocn(unsigned char, strlen((const char *) arg) + 1);

#ifdef CJK_EX
    if (!euc)
	outofmem(__FILE__, "TO_SJIS");
#endif
    TO_EUC(arg, euc);
    if (is_EUC_JP(euc))
	EUC_TO_SJIS(euc, sjis);
    else
	strcpy((char *) sjis, (const char *) arg);
    free(euc);
}

void TO_JIS(const unsigned char *arg,
	    unsigned char *jis)
{
    unsigned char *euc;

    if (arg[0] == 0) {
	jis[0] = 0;
	return;
    }
    euc = typeMallocn(unsigned char, strlen((const char *)arg) + 1);
#ifdef CJK_EX
    if (!euc)
	outofmem(__FILE__, "TO_JIS");
#endif
    TO_EUC(arg, euc);
    is_EUC_JP(euc);
    EUC_TO_JIS(euc, jis, TO_KANJI, TO_ASCII);

    free(euc);
}
