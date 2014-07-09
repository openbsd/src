/*
 * $LynxId: HTWSRC.c,v 1.29 2013/05/01 22:32:59 tom Exp $
 *
 *			Parse WAIS Source file			HTWSRC.c
 *			======================
 *
 *	This module parses a stream with WAIS source file
 *	format information on it and creates a structured stream.
 *	That structured stream is then converted into whatever.
 *
 *	3 June 93	Bug fix: Won't crash if no description
 */

#define HTSTREAM_INTERNAL 1

#include <HTUtils.h>

#include <HTWSRC.h>
#include <LYUtils.h>

#include <HTML.h>
#include <HTParse.h>

#include <LYLeaks.h>

#define BIG 10000		/* Arbitrary limit to value length */
#define PARAM_MAX BIG
#define CACHE_PERIOD (7*86400)	/* Time to keep .src file in seconds */

struct _HTStructured {
    const HTStructuredClass *isa;
    /* ... */
};

#define PUTC(c) (*me->target->isa->put_character)(me->target, c)
#define PUTS(s) (*me->target->isa->put_string)(me->target, s)
#define START(e) (*me->target->isa->start_element)(me->target, e, 0, 0, -1, 0)
#define END(e) (*me->target->isa->end_element)(me->target, e, 0)
#define MAYBE_END(e) if (HTML_dtd.tags[e].contents != SGML_EMPTY) \
			(*me->target->isa->end_element)(me->target, e, 0)

/*	Here are the parameters which can be specified in a  source file
*/
static const char *par_name[] =
{
    "version",
    "ip-address",
#define PAR_IP_NAME 2
    "ip-name",
#define PAR_TCP_PORT 3
    "tcp-port",
#define PAR_DATABASE_NAME 4
    "database-name",
#define PAR_COST 5
    "cost",
#define PAR_COST_UNIT 6
    "cost-unit",
#define PAR_FREE 7
    "free",
#define PAR_MAINTAINER 8
    "maintainer",
#define PAR_DESCRIPTION 9
    "description",
    "keyword-list",
    "source",
    "window-geometry",
    "configuration",
    "script",
    "update-time",
    "contact-at",
    "last-contacted",
    "confidence",
    "num-docs-to-request",
    "font",
    "font-size",
#define PAR_UNKNOWN 22
    "unknown",
    0,				/* Terminate list */
#define PAR_COUNT 23
};

enum tokenstate {
    beginning,
    before_tag,
    colon,
    before_value,
    value,
    bracketed_value,
    quoted_value,
    escape_in_quoted,
    done
};

/*		Stream Object
 *		------------
 *
 *	The target is the structured stream down which the
 *	parsed results will go.
 *
 *	all the static stuff below should go in here to make it reentrant
 */

struct _HTStream {
    const HTStreamClass *isa;
    HTStructured *target;
    char *par_value[PAR_COUNT];
    enum tokenstate state;
    char param[BIG + 1];
    int param_number;
    int param_count;
};

/*	Decode one hex character
*/
char from_hex(char c)
{
    return (char) ((c >= '0') && (c <= '9') ? c - '0'
		   : (c >= 'A') && (c <= 'F') ? c - 'A' + 10
		   : (c >= 'a') && (c <= 'f') ? c - 'a' + 10
		   : 0);
}

/*			State machine
 *			-------------
 *
 * On entry,
 *	me->state	is a valid state (see WSRC_init)
 *	c		is the next character
 * On exit,
 *	returns 1	Done with file
 *		0	Continue. me->state is updated if necessary.
 *		-1	Syntax error error
 */

/*		Treat One Character
 *		-------------------
 */
static void WSRCParser_put_character(HTStream *me, int c)
{
    switch (me->state) {
    case beginning:
	if (c == '(')
	    me->state = before_tag;
	break;

    case before_tag:
	if (c == ')') {
	    me->state = done;
	    return;		/* Done with input file */
	} else if (c == ':') {
	    me->param_count = 0;
	    me->state = colon;
	}			/* Ignore other text */
	break;

    case colon:
	if (WHITE(c)) {
	    me->param[me->param_count++] = 0;	/* Terminate */
	    for (me->param_number = 0;
		 par_name[me->param_number];
		 me->param_number++) {
		if (0 == strcmp(par_name[me->param_number], me->param)) {
		    break;
		}
	    }
	    if (!par_name[me->param_number]) {	/* Unknown field */
		CTRACE((tfp, "HTWSRC: Unknown field `%s' in source file\n",
			me->param));
		me->param_number = PAR_UNKNOWN;
		me->state = before_value;	/* Could be better ignore */
		return;
	    }
	    me->state = before_value;
	} else {
	    if (me->param_count < PARAM_MAX)
		me->param[me->param_count++] = (char) c;
	}
	break;

    case before_value:
	if (c == ')') {
	    me->state = done;
	    return;		/* Done with input file */
	}
	if (WHITE(c))
	    return;		/* Skip white space */
	me->param_count = 0;
	if (c == '"') {
	    me->state = quoted_value;
	} else {
	    me->state = ((c == '(')
			 ? bracketed_value
			 : value);
	    me->param[me->param_count++] = (char) c;	/* Don't miss first character */
	}
	break;

    case value:
	if (WHITE(c)) {
	    me->param[me->param_count] = 0;
	    StrAllocCopy(me->par_value[me->param_number], me->param);
	    me->state = before_tag;
	} else {
	    if (me->param_count < PARAM_MAX)
		me->param[me->param_count++] = (char) c;
	}
	break;

    case bracketed_value:
	if (c == ')') {
	    me->param[me->param_count] = 0;
	    StrAllocCopy(me->par_value[me->param_number], me->param);
	    me->state = before_tag;
	    break;
	}
	if (me->param_count < PARAM_MAX)
	    me->param[me->param_count++] = (char) c;
	break;

    case quoted_value:
	if (c == '"') {
	    me->param[me->param_count] = 0;
	    StrAllocCopy(me->par_value[me->param_number], me->param);
	    me->state = before_tag;
	    break;
	}

	if (c == '\\') {	/* Ignore escape but switch state */
	    me->state = escape_in_quoted;
	    break;
	}
	/* Fall through! */

    case escape_in_quoted:
	if (me->param_count < PARAM_MAX)
	    me->param[me->param_count++] = (char) c;
	me->state = quoted_value;
	break;

    case done:			/* Ignore anything after EOF */
	return;

    }				/* switch me->state */
}

/*			Open Cache file
 *			===============
 *
 *   Bugs: Maybe for filesystem-challenged platforms (MSDOS for example) we
 *   should make a hash code for the filename.
 */

#ifdef CACHE_FILE_PREFIX
static BOOL write_cache(HTStream *me)
{
    FILE *fp;
    char *cache_file_name = NULL;
    char *www_database;
    int result = NO;

    if (!me->par_value[PAR_DATABASE_NAME]
	|| !me->par_value[PAR_IP_NAME]
	)
	return NO;

    www_database = HTEscape(me->par_value[PAR_DATABASE_NAME], URL_XALPHAS);
    HTSprintf0(&cache_file_name, "%sWSRC-%s:%s:%.100s.txt",
	       CACHE_FILE_PREFIX,
	       me->par_value[PAR_IP_NAME],
	       (me->par_value[PAR_TCP_PORT]
		? me->par_value[PAR_TCP_PORT]
		: "210"),
	       www_database);

    if ((fp = fopen(cache_file_name, TXT_W)) != 0) {
	result = YES;
	if (me->par_value[PAR_DESCRIPTION])
	    fputs(me->par_value[PAR_DESCRIPTION], fp);
	else
	    fputs("Description not available\n", fp);
	fclose(fp);
    }
    FREE(www_database);
    FREE(cache_file_name);
    return result;
}
#endif

/*			Output equivalent HTML
 *			----------------------
 *
 */

static void give_parameter(HTStream *me, int p)
{
    PUTS(par_name[p]);
    if (me->par_value[p]) {
	PUTS(": ");
	PUTS(me->par_value[p]);
	PUTS("; ");
    } else {
	PUTS(gettext(" NOT GIVEN in source file; "));
    }
}

/*			Generate Outout
 *			===============
 */
static void WSRC_gen_html(HTStream *me, int source_file)
{
    if (me->par_value[PAR_DATABASE_NAME]) {
	char *shortname = 0;
	int l;

	StrAllocCopy(shortname, me->par_value[PAR_DATABASE_NAME]);
	l = (int) strlen(shortname);
	if (l > 4 && !strcasecomp(shortname + l - 4, ".src")) {
	    shortname[l - 4] = 0;	/* Chop of .src -- boring! */
	}

	START(HTML_HEAD);
	PUTC('\n');
	START(HTML_TITLE);
	PUTS(shortname);
	PUTS(source_file ? gettext(" WAIS source file") : INDEX_SEGMENT);
	END(HTML_TITLE);
	PUTC('\n');
	END(HTML_HEAD);

	START(HTML_H1);
	PUTS(shortname);
	PUTS(source_file ? gettext(" description") : INDEX_SEGMENT);
	END(HTML_H1);
	PUTC('\n');
	FREE(shortname);
    }

    START(HTML_DL);		/* Definition list of details */

    if (source_file) {
	START(HTML_DT);
	PUTS(gettext("Access links"));
	MAYBE_END(HTML_DT);
	START(HTML_DD);
	if (me->par_value[PAR_IP_NAME] &&
	    me->par_value[PAR_DATABASE_NAME]) {

	    char *WSRC_address = NULL;
	    char *www_database;

	    www_database = HTEscape(me->par_value[PAR_DATABASE_NAME],
				    URL_XALPHAS);
	    HTSprintf0(&WSRC_address, "%s//%s%s%s/%s",
		       STR_WAIS_URL,
		       me->par_value[PAR_IP_NAME],
		       me->par_value[PAR_TCP_PORT] ? ":" : "",
		       (me->par_value[PAR_TCP_PORT]
			? me->par_value[PAR_TCP_PORT]
			: ""),
		       www_database);

	    HTStartAnchor(me->target, NULL, WSRC_address);
	    PUTS(gettext("Direct access"));
	    END(HTML_A);
	    /** Proxy will be used if defined, so let user know that - FM **/
	    PUTS(gettext(" (or via proxy server, if defined)"));

	    FREE(www_database);
	    FREE(WSRC_address);

	} else {
	    give_parameter(me, PAR_IP_NAME);
	    give_parameter(me, PAR_DATABASE_NAME);
	}
	MAYBE_END(HTML_DD);

    }
    /* end if source_file */
    if (me->par_value[PAR_MAINTAINER]) {
	START(HTML_DT);
	PUTS(gettext("Maintainer"));
	MAYBE_END(HTML_DT);
	START(HTML_DD);
	PUTS(me->par_value[PAR_MAINTAINER]);
	MAYBE_END(HTML_DD);
    }
    if (me->par_value[PAR_IP_NAME]) {
	START(HTML_DT);
	PUTS(gettext("Host"));
	MAYBE_END(HTML_DT);
	START(HTML_DD);
	PUTS(me->par_value[PAR_IP_NAME]);
	MAYBE_END(HTML_DD);
    }

    END(HTML_DL);

    if (me->par_value[PAR_DESCRIPTION]) {
	START(HTML_PRE);	/* Preformatted description */
	PUTS(me->par_value[PAR_DESCRIPTION]);
	END(HTML_PRE);
    }

    (*me->target->isa->_free) (me->target);

    return;
}				/* generate html */

static void WSRCParser_put_string(HTStream *context, const char *str)
{
    const char *p;

    for (p = str; *p; p++)
	WSRCParser_put_character(context, *p);
}

static void WSRCParser_write(HTStream *context, const char *str,
			     int l)
{
    const char *p;
    const char *e = str + l;

    for (p = str; p < e; p++)
	WSRCParser_put_character(context, *p);
}

static void WSRCParser_free(HTStream *me)
{
    WSRC_gen_html(me, YES);
#ifdef CACHE_FILE_PREFIX
    write_cache(me);
#endif
    {
	int p;

	for (p = 0; par_name[p]; p++) {		/* Clear out old values */
	    FREE(me->par_value[p]);
	}
    }
    FREE(me);
}

static void WSRCParser_abort(HTStream *me, HTError e GCC_UNUSED)
{
    WSRCParser_free(me);
}

/*		Stream subclass		-- method routines
 *		---------------
 */

static HTStreamClass WSRCParserClass =
{
    "WSRCParser",
    WSRCParser_free,
    WSRCParser_abort,
    WSRCParser_put_character,
    WSRCParser_put_string,
    WSRCParser_write
};

/*		Converter from WAIS Source to whatever
 *		--------------------------------------
 */
HTStream *HTWSRCConvert(HTPresentation *pres, HTParentAnchor *anchor,
			HTStream *sink)
{
    HTStream *me = (HTStream *) malloc(sizeof(*me));

    if (!me)
	outofmem(__FILE__, "HTWSRCConvert");

    assert(me != NULL);

    me->isa = &WSRCParserClass;
    me->target = HTML_new(anchor, pres->rep_out, sink);

    {
	int p;

	for (p = 0; p < PAR_COUNT; p++) {	/* Clear out parameter values */
	    me->par_value[p] = 0;
	}
    }
    me->state = beginning;

    return me;
}
