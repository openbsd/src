/*			GOPHER ACCESS				HTGopher.c
**			=============
**
**  History:
**	26 Sep 90	Adapted from other accesses (News, HTTP) TBL
**	29 Nov 91	Downgraded to C, for portable implementation.
**	10 Mar 96	Foteos Macrides (macrides@sci.wfbr.edu).  Added a
**			  form-based CSO/PH gateway.  Can be invoked via a
**			  "cso://host[:port]/" or "gopher://host:105/2"
**			  URL.	If a gopher URL is used with a query token
**			  ('?'), the old ISINDEX procedure will be used
**			  instead of the form-based gateway.
**	15 Mar 96	Foteos Macrides (macrides@sci.wfbr.edu).  Pass
**			  port 79, gtype 0 gopher URLs to the finger
**			  gateway.
*/

#include <HTUtils.h>		/* Coding convention macros */

#ifndef DISABLE_GOPHER
#include <HTAlert.h>
#include <HTParse.h>
#include <HTTCP.h>
#include <HTFinger.h>

/*
**  Implements.
*/
#include <HTGopher.h>

#define GOPHER_PORT 70		/* See protocol spec */
#define CSO_PORT 105		/* See protocol spec */
#define BIG 1024		/* Bug */
#define LINE_LENGTH 256		/* Bug */

/*
**  Gopher entity types.
*/
#define GOPHER_TEXT		'0'
#define GOPHER_MENU		'1'
#define GOPHER_CSO		'2'
#define GOPHER_ERROR		'3'
#define GOPHER_MACBINHEX	'4'
#define GOPHER_PCBINARY		'5'
#define GOPHER_UUENCODED	'6'
#define GOPHER_INDEX		'7'
#define GOPHER_TELNET		'8'
#define GOPHER_BINARY		'9'
#define GOPHER_GIF		'g'
#define GOPHER_HTML		'h'		/* HTML */
#define GOPHER_CHTML		'H'		/* HTML */
#define GOPHER_SOUND		's'
#define GOPHER_WWW		'w'		/* W3 address */
#define GOPHER_IMAGE		'I'
#define GOPHER_TN3270		'T'
#define GOPHER_INFO		'i'
#define GOPHER_DUPLICATE	'+'
#define GOPHER_PLUS_IMAGE	':'		/* Addition from Gopher Plus */
#define GOPHER_PLUS_MOVIE	';'
#define GOPHER_PLUS_SOUND	'<'
#define GOPHER_PLUS_PDF		'P'

#include <HTFormat.h>

/*
**  Hypertext object building machinery.
*/
#include <HTML.h>

#include <LYStrings.h>
#include <LYUtils.h>
#include <LYLeaks.h>

#define PUTC(c) (*targetClass.put_character)(target, c)
#define PUTS(s) (*targetClass.put_string)(target, s)
#define START(e) (*targetClass.start_element)(target, e, 0, 0, -1, 0)
#define END(e) (*targetClass.end_element)(target, e, 0)
#define FREE_TARGET (*targetClass._free)(target)

#define GOPHER_PROGRESS(foo) HTAlert(foo)

#define NEXT_CHAR HTGetCharacter()

/*
**  Module-wide variables.
*/
PRIVATE int s;				/* Socket for gopher or CSO host */

struct _HTStructured {
	CONST HTStructuredClass * isa;	/* For gopher streams */
	/* ... */
};

PRIVATE HTStructured *target;		/* the new gopher hypertext */
PRIVATE HTStructuredClass targetClass;	/* Its action routines */

struct _HTStream
{
  HTStreamClass * isa;			/* For form-based CSO  gateway - FM */
};

typedef struct _CSOfield_info {		/* For form-based CSO gateway - FM */
    struct _CSOfield_info *	next;
    char *			name;
    char *			attributes;
    char *			description;
    int				id;
    int				lookup;
    int				indexed;
    int				url;
    int				max_size;
    int				defreturn;
    int				explicit_return;
    int				reserved;
    int				public;
    char			name_buf[16];	/* Avoid malloc if we can */
    char			desc_buf[32];	/* Avoid malloc if we can */
    char			attr_buf[80];	/* Avoid malloc if we can */
} CSOfield_info;

PRIVATE CSOfield_info *CSOfields = NULL; /* For form-based CSO gateway - FM */

typedef struct _CSOformgen_context {	 /* For form-based CSO gateway - FM */
    char *		host;
    char *		seek;
    CSOfield_info *	fld;
    int			port;
    int			cur_line;
    int			cur_off;
    int			rep_line;
    int			rep_off;
    int			public_override;
    int			field_select;
} CSOformgen_context;

/*	Matrix of allowed characters in filenames
**	=========================================
*/
PRIVATE BOOL acceptable[256];
PRIVATE BOOL acceptable_inited = NO;

PRIVATE void init_acceptable NOARGS
{
    unsigned int i;
    char * good =
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789./-_$";
    for(i = 0; i < 256; i++)
	acceptable[i] = NO;
    for(; *good; good++)
	acceptable[(unsigned int)*good] = YES;
    acceptable_inited = YES;
}

/*	Decode one hex character
**	========================
*/
PRIVATE CONST char hex[17] = "0123456789abcdef";

PRIVATE char from_hex ARGS1(char, c)
{
    return		  (c>='0')&&(c<='9') ? c-'0'
			: (c>='A')&&(c<='F') ? c-'A'+10
			: (c>='a')&&(c<='f') ? c-'a'+10
			:		       0;
}

/*	Paste in an Anchor
**	==================
**
**	The title of the destination is set, as there is no way
**	of knowing what the title is when we arrive.
**
** On entry,
**	HT	is in append mode.
**	text	points to the text to be put into the file, 0 terminated.
**	addr	points to the hypertext refernce address 0 terminated.
*/
PUBLIC BOOLEAN HT_Is_Gopher_URL=FALSE;

PRIVATE void write_anchor ARGS2(CONST char *,text, CONST char *,addr)
{
    BOOL present[HTML_A_ATTRIBUTES];
    CONST char * value[HTML_A_ATTRIBUTES];

    int i;

    for (i = 0; i < HTML_A_ATTRIBUTES; i++)
	present[i] = 0;
    present[HTML_A_HREF] = YES;
    ((CONST char **)value)[HTML_A_HREF] = addr;
    present[HTML_A_TITLE] = YES;
    ((CONST char **)value)[HTML_A_TITLE] = text;

    CTRACE(tfp,"HTGopher: adding URL: %s\n",addr);

    HT_Is_Gopher_URL = TRUE;  /* tell HTML.c that this is a Gopher URL */
    (*targetClass.start_element)(target, HTML_A, present,
				 (CONST char **)value, -1, 0);

    PUTS(text);
    END(HTML_A);
}

/*	Parse a Gopher Menu document
**	============================
*/
PRIVATE void parse_menu ARGS2(
	CONST char *,		arg GCC_UNUSED,
	HTParentAnchor *,	anAnchor)
{
    char gtype;
    int ich;
    char line[BIG];
    char *name = NULL, *selector = NULL;	/* Gopher menu fields */
    char *host = NULL;
    char *port;
    char *p = line;
    CONST char *title;
    int bytes = 0;
    int BytesReported = 0;
    char buffer[128];

#define TAB		'\t'
#define HEX_ESCAPE	'%'


    START(HTML_HTML);
    PUTC('\n');
    START(HTML_HEAD);
    PUTC('\n');
    START(HTML_TITLE);
    if ((title = HTAnchor_title(anAnchor)))
	PUTS(title);
    else
	PUTS(GOPHER_MENU_TITLE);
    END(HTML_TITLE);
    PUTC('\n');
    END(HTML_HEAD);
    PUTC('\n');

    START(HTML_BODY);
    PUTC('\n');
    START(HTML_H1);
    if ((title = HTAnchor_title(anAnchor)))
	PUTS(title);
    else
	PUTS(GOPHER_MENU_TITLE);
    END(HTML_H1);
    PUTC('\n');
    START(HTML_PRE);
    while ((ich=NEXT_CHAR) != EOF) {

	if (interrupted_in_htgetcharacter) {
	    CTRACE(tfp, "HTGopher: Interrupted in HTGetCharacter, apparently.\n");
	    goto end_html;
	}

	if ((char)ich != LF) {
	    *p = ich;		/* Put character in line */
	    if (p< &line[BIG-1]) p++;

	} else {
	    *p++ = '\0';	/* Terminate line */
	    bytes += p-line;	/* add size */
	    p = line;		/* Scan it to parse it */
	    port = 0;		/* Flag "not parsed" */
	    CTRACE(tfp, "HTGopher: Menu item: %s\n", line);
	    gtype = *p++;

	    if (bytes > BytesReported + 1024) {
		sprintf(buffer, TRANSFERRED_X_BYTES, bytes);
		HTProgress(buffer);
		BytesReported = bytes;
	    }

	    /* Break on line with a dot by itself */
	    if ((gtype=='.') && ((*p=='\r') || (*p==0)))
		break;

	    if (gtype && *p) {
		name = p;
		selector = strchr(name, TAB);
		if (selector) {
		    *selector++ = '\0'; /* Terminate name */
		    /*
		     * Gopher+ Type=0+ objects can be binary, and will
		     * have 9 or 5 beginning their selector.  Make sure
		     * we don't trash the terminal by treating them as
		     * text. - FM
		     */
		    if (gtype == GOPHER_TEXT && (*selector == GOPHER_BINARY ||
						 *selector == GOPHER_PCBINARY))
			gtype = *selector;
		    host = strchr(selector, TAB);
		    if (host) {
			*host++ = '\0'; /* Terminate selector */
			port = strchr(host, TAB);
			if (port) {
			    char *junk;
			    port[0] = ':';	/* delimit host a la W3 */
			    junk = strchr(port, TAB);
			    if (junk) *junk++ = '\0';	/* Chop port */
			    if ((port[1]=='0') && (!port[2]))
				port[0] = '\0'; /* 0 means none */
			} /* no port */
		    } /* host ok */
		} /* selector ok */
	    } /* gtype and name ok */

	    /* Nameless files are a separator line */
	    if (gtype == GOPHER_TEXT) {
		int i = strlen(name)-1;
		while (name[i] == ' ' && i >= 0)
		    name[i--] = '\0';
		if (i < 0)
		    gtype = GOPHER_INFO;
	    }

	    if (gtype == GOPHER_WWW) {	/* Gopher pointer to W3 */
		PUTS("(HTML) ");
		write_anchor(name, selector);

	    } else if (gtype == GOPHER_INFO) {
	    /* Information or separator line */
		PUTS("       ");
		PUTS(name);

	    } else if (port) {		/* Other types need port */
		char *address = 0;

		if (gtype == GOPHER_TELNET) {
		    PUTS(" (TEL) ");
		    if (*selector)
			HTSprintf0(&address, "telnet://%s@%s/",
					   selector, host);
		    else
			HTSprintf0(&address, "telnet://%s/", host);
		}
		else if (gtype == GOPHER_TN3270)
		{
		    PUTS("(3270) ");
		    if (*selector)
			HTSprintf0(&address, "tn3270://%s@%s/",
				selector, host);
		    else
			HTSprintf0(&address, "tn3270://%s/", host);
		}
		else {			/* If parsed ok */
		    char *r;

		    switch(gtype) {
			case GOPHER_TEXT:
			    PUTS("(FILE) ");
			    break;
			case GOPHER_MENU:
			    PUTS(" (DIR) ");
			    break;
			case GOPHER_CSO:
			    PUTS(" (CSO) ");
			    break;
			case GOPHER_PCBINARY:
			    PUTS(" (BIN) ");
			    break;
			case GOPHER_UUENCODED:
			    PUTS(" (UUE) ");
			    break;
			case GOPHER_INDEX:
			    PUTS("  (?)  ");
			    break;
			case GOPHER_BINARY:
			    PUTS(" (BIN) ");
			    break;
			case GOPHER_GIF:
			case GOPHER_IMAGE:
			case GOPHER_PLUS_IMAGE:
			    PUTS(" (IMG) ");
			    break;
			case GOPHER_SOUND:
			case GOPHER_PLUS_SOUND:
			    PUTS(" (SND) ");
			    break;
			case GOPHER_MACBINHEX:
			    PUTS(" (HQX) ");
			    break;
			case GOPHER_HTML:
			case GOPHER_CHTML:
			    PUTS("(HTML) ");
			    break;
			case 'm':
			    PUTS("(MIME) ");
			    break;
			case GOPHER_PLUS_MOVIE:
			    PUTS(" (MOV) ");
			    break;
			case GOPHER_PLUS_PDF:
			    PUTS(" (PDF) ");
			    break;
			default:
			    PUTS("(UNKN) ");
			    break;
		    }

		    HTSprintf0(&address, "//%s/%c", host, gtype);

		    for(r = selector; *r; r++) { /* Encode selector string */
			if (acceptable[(unsigned char)*r]) {
			    HTSprintf(&address, "%c", *r);
			} else {
			    HTSprintf(&address, "%c%c%c",
				HEX_ESCAPE,	/* Means hex coming */
				hex[(TOASCII(*r)) >> 4],
				hex[(TOASCII(*r)) & 15]);
			}
		    }
		}
		/* Error response from Gopher doesn't deserve to
		   be a hyperlink. */
		if (strcmp (address, "gopher://error.host:1/0"))
		    write_anchor(name, address);
		else
		    PUTS(name);
		FREE(address);
	    } else { /* parse error */
		CTRACE(tfp, "HTGopher: Bad menu item.\n");
		PUTS(line);

	    } /* parse error */

	    PUTC('\n');
	    p = line;	/* Start again at beginning of line */

	} /* if end of line */

    } /* Loop over characters */

end_html:
    END(HTML_PRE);
    PUTC('\n');
    END(HTML_BODY);
    PUTC('\n');
    END(HTML_HTML);
    PUTC('\n');
    FREE_TARGET;

    return;
}

/*	Parse a Gopher CSO document from an ISINDEX query.
**	==================================================
**
**   Accepts an open socket to a CSO server waiting to send us
**   data and puts it on the screen in a reasonable manner.
**
**   Perhaps this data can be automatically linked to some
**   other source as well???
**
**  Taken from hacking by Lou Montulli@ukanaix.cc.ukans.edu
**  on XMosaic-1.1, and put on libwww 2.11 by Arthur Secret,
**  secret@dxcern.cern.ch .
*/
PRIVATE void parse_cso ARGS2(
	CONST char *,		arg,
	HTParentAnchor *,	anAnchor)
{
    int ich;
    char line[BIG];
    char *p = line;
    char *second_colon, last_char='\0';
    CONST char *title;

    START(HTML_HEAD);
    PUTC('\n');
    START(HTML_TITLE);
    if ((title = HTAnchor_title(anAnchor)))
	PUTS(title);
    else
	PUTS(GOPHER_CSO_SEARCH_RESULTS);
    END(HTML_TITLE);
    PUTC('\n');
    END(HTML_HEAD);
    PUTC('\n');
    START(HTML_H1);
    if ((title = HTAnchor_title(anAnchor)))
	PUTS(title);
    else {
	PUTS(arg);
	PUTS(GOPHER_SEARCH_RESULTS);
    }
    END(HTML_H1);
    PUTC('\n');
    START(HTML_PRE);

    /*
    **	Start grabbing chars from the network.
    */
    while ((ich=NEXT_CHAR) != EOF)
	{
	    if ((char)ich != LF)
		{
		    *p = ich;		/* Put character in line */
		    if (p< &line[BIG-1]) p++;
		}
	    else
		{
		    *p = '\0';		/* Terminate line */
		    p = line;		/* Scan it to parse it */
		    /*
		    **	OK we now have a line in 'p'.
		    **	Lets parse it and print it.
		    */

		    /*
		    **	Break on line that begins with a 2.
		    **	It's the end of data.
		    */
		    if (*p == '2')
			break;

		    /*
		    **	Lines beginning with 5 are errors.
		    **	Print them and quit.
		    */
		    if (*p == '5') {
			START(HTML_H2);
			PUTS(p+4);
			END(HTML_H2);
			break;
		    }

		    if (*p == '-') {
			/*
			**  Data lines look like  -200:#:
			**  where # is the search result number and can be
			**  multiple digits (infinite?).
			**  Find the second colon and check the digit to the
			**  left of it to see if they are diferent.
			**  If they are then a different person is starting.
			**  Make this line an <h2>.
			*/

			/*
			**  Find the second_colon.
			*/
			second_colon = strchr( strchr(p,':')+1, ':');

			if(second_colon != NULL) {  /* error check */

			    if (*(second_colon-1) != last_char)
				/* print seperator */
			    {
				END(HTML_PRE);
				START(HTML_H2);
			    }


			    /*
			    **	Right now the record appears with the alias
			    **	(first line) as the header and the rest as
			    **	<pre> text.
			    **	It might look better with the name as the
			    **	header and the rest as a <ul> with <li> tags.
			    **	I'm not sure whether the name field comes in
			    **	any special order or if its even required in
			    **	a record, so for now the first line is the
			    **	header no matter what it is (it's almost
			    **	always the alias).
			    **	A <dl> with the first line as the <DT> and
			    **	the rest as some form of <DD> might good also?
			    */

			    /*
			    **	Print data.
			    */
			    PUTS(second_colon+1);
			    PUTC('\n');

			    if (*(second_colon-1) != last_char)
				/* end seperator */
			    {
				END(HTML_H2);
				START(HTML_PRE);
			    }

			    /*
			    **	Save the char before the second colon
			    **	for comparison on the next pass.
			    */
			    last_char =  *(second_colon-1) ;

			} /* end if second_colon */
		    } /* end if *p == '-' */
		} /* if end of line */

	} /* Loop over characters */

    /* end the text block */
    PUTC('\n');
    END(HTML_PRE);
    PUTC('\n');
    FREE_TARGET;

    return;  /* all done */
} /* end of procedure */

/*	Display a Gopher CSO ISINDEX cover page.
**	========================================
*/
PRIVATE void display_cso ARGS2(
	CONST char *,		arg,
	HTParentAnchor *,	anAnchor)
{
    CONST char * title;

    START(HTML_HEAD);
    PUTC('\n');
    START(HTML_TITLE);
    if ((title = HTAnchor_title(anAnchor)))
	PUTS(title);
    else
	PUTS(GOPHER_CSO_INDEX);
    END(HTML_TITLE);
    PUTC('\n');
    START(HTML_ISINDEX);
    PUTC('\n');
    END(HTML_HEAD);
    PUTC('\n');
    START(HTML_H1);
    if ((title = HTAnchor_title(anAnchor)))
	PUTS(title);
    else {
       PUTS(arg);
       PUTS(INDEX_SEGMENT);
    }
    END(HTML_H1);
    PUTS(GOPHER_CSO_INDEX_SUBTITLE);
    START(HTML_P);
    PUTS(GOPHER_CSO_SOLICIT_KEYWORDS);
    START(HTML_P);
    PUTS(SEGMENT_KEYWORDS_WILL);
    PUTS(SEGMENT_PERSONS_DB_NAME);

    if (!HTAnchor_title(anAnchor))
	HTAnchor_setTitle(anAnchor, arg);

    FREE_TARGET;
    return;
}

/*	Display a Gopher Index document.
**	================================
*/
PRIVATE void display_index ARGS2(
				  CONST char *, arg,
				  HTParentAnchor *,anAnchor)
{
    CONST char * title;

    START(HTML_HEAD);
    PUTC('\n');
    PUTC('\n');
    START(HTML_TITLE);
    if ((title = HTAnchor_title(anAnchor)))
	PUTS(title);
    else
	PUTS(GOPHER_INDEX_TITLE);
    END(HTML_TITLE);
    PUTC('\n');
    START(HTML_ISINDEX);
    PUTC('\n');
    END(HTML_HEAD);
    PUTC('\n');
    START(HTML_H1);
    if ((title = HTAnchor_title(anAnchor)))
	PUTS(title);
    else {
       PUTS(arg);
       PUTS(INDEX_SEGMENT);
    }
    END(HTML_H1);
    PUTS(GOPHER_INDEX_SUBTITLE);
    START(HTML_P);
    PUTS(GOPHER_SOLICIT_KEYWORDS);

    if (!HTAnchor_title(anAnchor))
	HTAnchor_setTitle(anAnchor, arg);

    FREE_TARGET;
    return;
}

/*	De-escape a selector into a command.
**	====================================
**
**	The % hex escapes are converted. Otheriwse, the string is copied.
*/
PRIVATE void de_escape ARGS2(char *, command, CONST char *, selector)
{
    CONST char * p = selector;
    char * q = command;
	if (command == NULL)
	    outofmem(__FILE__, "HTLoadGopher");
    while (*p) {		/* Decode hex */
	if (*p == HEX_ESCAPE) {
	    char c;
	    unsigned int b;
	    p++;
	    c = *p++;
	    b =   from_hex(c);
	    c = *p++;
	    if (!c) break;	/* Odd number of chars! */
	    *q++ = FROMASCII((b<<4) + from_hex(c));
	} else {
	    *q++ = *p++;	/* Record */
	}
    }
    *q++ = '\0';	/* Terminate command */
}


/*	Free the CSOfields structures. - FM
**	===================================
*/
PRIVATE void free_CSOfields NOPARAMS
{
    CSOfield_info *cur = CSOfields;
    CSOfield_info *prev;

    while (cur) {
	if (cur->name != cur->name_buf)
	    FREE(cur->name);
	if (cur->attributes != cur->attr_buf)
	    FREE(cur->attributes);
	if (cur->description != cur->desc_buf)
	    FREE(cur->description);
	prev = cur;
	cur = cur->next;
	FREE(prev);
    }

    return;
}

/*	Interpret CSO/PH form template keys. - FM
**	=========================================
*/
PRIVATE void interpret_cso_key ARGS5(
	char *,			key,
	char *,			buf,
	int *,			length,
	CSOformgen_context *,	ctx,
	HTStream *,		Target)
{
    CSOfield_info *fld;

    if ((fld = ctx->fld) != 0) {
	/*
	**  Most substitutions only recognized inside of loops.
	*/
	int error = 0;
	if (0 == strncmp(key, "$(FID)", 6)) {
	    sprintf(buf, "%d", fld->id);
	} else if (0 == strncmp(key, "$(FDESC)", 8)) {
	    sprintf(buf, "%s%s%s", fld->description,
		    ctx->public_override ? /***" "***/"" : "",
		    ctx->public_override ? /***fld->attributes***/"" : "");
	} else if (0 == strncmp(key, "$(FDEF)", 7)) {
	    strcpy(buf, fld->defreturn ? " checked" : "");
	} else if (0 == strncmp(key, "$(FNDX)", 7)) {
	    strcpy(buf, fld->indexed ? "*" : "");
	} else if (0 == strncmp(key, "$(FSIZE)", 8)) {
	    sprintf(buf, " size=%d maxlength=%d",
		    fld->max_size > 55 ? 55 : fld->max_size,
		    fld->max_size);
	} else if (0 == strncmp(key, "$(FSIZE2)", 9)) {
	    sprintf(buf, " maxlength=%d", fld->max_size);
	} else {
	    error = 1;
	}
	if (!error) {
	    *length = strlen(buf);
	    return;
	}
    }
    buf[0] = '\0';
    if (0 == strncmp(key, "$(NEXTFLD)", 10)) {
	if (!ctx->fld)
	    fld = CSOfields;
	else
	    fld = ctx->fld->next;
	switch (ctx->field_select) {
	  case 0:
	    /*
	    **	'Query' fields, public and lookup attributes.
	    */
	    for (; fld; fld = fld->next)
		 if (fld->public && (fld->lookup==1))
		     break;
	    break;
	  case 1:
	    /*
	    **	'Query' fields, accept lookup attribute.
	    */
	    for (; fld; fld = fld->next)
		if (fld->lookup == 1)
		    break;
	    break;
	  case 2:
	    /*
	    **	'Return' fields, public only.
	    */
	    for (; fld; fld = fld->next)
		if (fld->public)
		    break;
	    break;
	  case 3:
	    /*
	    **	All fields.
	    */
	    break;
	}
	if (fld) {
	    ctx->cur_line = ctx->rep_line;
	    ctx->cur_off = ctx->rep_off;
	}
	ctx->fld = fld;

    } else if ((0 == strncmp(key, "$(QFIELDS)", 10)) ||
	       (0 == strncmp(key, "$(RFIELDS)", 10))) {
	/*
	**  Begin iteration sequence.
	*/
	ctx->rep_line = ctx->cur_line;
	ctx->rep_off = ctx->cur_off;
	ctx->fld = (CSOfield_info *) 0;
	ctx->seek = "$(NEXTFLD)";
	ctx->field_select = (key[2] == 'Q') ? 0 : 2;
	if (ctx->public_override)
	    ctx->field_select++;

    } else if (0 == strncmp(key, "$(NAMEFLD)", 10)) {
	/*
	**  Special, locate name field.  Flag lookup so QFIELDS will skip it.
	*/
	for (fld = CSOfields; fld; fld = fld->next)
	    if (strcmp(fld->name, "name") == 0 ||
		strcmp(fld->name, "Name") == 0) {
		if (fld->lookup)
		    fld->lookup = 2;
		break;
	    }
	ctx->fld = fld;
    } else if (0 == strncmp (key, "$(HOST)", 7)) {
	strcpy (buf, ctx->host);
    } else if (0 == strncmp (key, "$(PORT)", 7)) {
	sprintf(buf, "%d", ctx->port);
    } else {
	/*
	**  No match, dump key to buffer so client sees it for debugging.
	*/
	size_t out = 0;
	while (*key && (*key != ')')) {
	    buf[out++] = (*key++);
	    if (out > sizeof(buf)-2) {
		buf[out] = '\0';
		(*Target->isa->put_block)(Target, buf, strlen(buf));
		out = 0;
	    }
	}
	buf[out++] = ')';
	buf[out] = '\0';
	*length = strlen(buf);
	return;
    }
    *length = strlen(buf);
    return;
}

/*	Parse the elements in a CSO/PH fields structure. - FM
**	=====================================================
*/
PRIVATE int parse_cso_field_info ARGS1(
	CSOfield_info *,	blk)
{
    char *info, *max_spec;

    /*
    ** Initialize all fields to default values.
    */
    blk->indexed = blk->lookup = blk->reserved = blk->max_size = blk->url = 0;
    blk->defreturn = blk->explicit_return = blk->public = 0;

    /*
    **	Search for keywords in info string and set values.  Attributes
    **	are converted to all lower-case for comparison.
    */
    info = blk->attributes;
    LYLowerCase(info);
    if (strstr(info, "indexed "))
	blk->indexed = 1;
    if (strstr(info, "default "))
	blk->defreturn = 1;
    if (strstr(info, "public "))
	blk->public = 1;
    if (strstr(info, "lookup "))
	blk->lookup = 1;
    if (strstr(info, "url ")) {
	blk->url = 1;
	blk->defreturn = 1;
    }
    max_spec = strstr(info, "max ");
    if (max_spec) {
	sscanf(&max_spec[4], "%d", &blk->max_size);
    } else {
	blk->max_size = 32;
    }

    return 0;
}

/*	Parse a reply from a CSO/PH fields request. - FM
**	================================================
*/
PRIVATE int parse_cso_fields ARGS2(
	char *,		buf,
	int,		size)
{
    int ich;
    char *p = buf;
    int i, code = 0, prev_code;
    size_t alen;
    char *indx, *name;
    CSOfield_info *last, *new;

    last = CSOfields = (CSOfield_info *) 0;
    prev_code = -2555;
    buf[0] = '\0';

    /*
    **	Start grabbing chars from the network.
    */
    while ((ich = NEXT_CHAR) != EOF) {
	if (interrupted_in_htgetcharacter) {
	    CTRACE(tfp, "HTLoadCSO: Interrupted in HTGetCharacter, apparently.\n");
	    free_CSOfields();
	    buf[0] = '\0';
	    return HT_INTERRUPTED;
	}

	if ((char)ich != LF) {
	    *p = ich;		/* Put character in buffer */
	    if (p < &buf[size-1]) {
		p++;
	    }
	} else {
	    *p = '\0';		/* Terminate line */
	    p = buf;		/* Scan it to parse it */

	    /* OK we now have a line in 'p' lets parse it.
	     */

	    /*
	    **	Break on line that begins with a 2.
	    **	It's the end of data.
	    */
	    if (*p == '2')
		break;

	    /*
	    **	Lines beginning with 5 are errors.
	    **	Print them and quit.
	    */
	    if (*p == '5') {
		strcpy (buf, p);
		return 5;
	    }

	    if (*p == '-') {
		/*
		**  Data lines look like  -200:#:
		**  where # is the search result number and can be
		**  multiple digits (infinite?).
		*/

	    /*
	    **	Check status, ignore any non-success.
	    */
	    if (p[1] != '2' )
		continue;

	    /*
	    **	Parse fields within returned line into status, ndx, name, data.
	    */
	    indx = NULL;
	    name = NULL;
	    for (i = 0; p[i]; i++)
		if (p[i] == ':' ) {
		    p[i] = '\0';
		    if (!indx) {
			indx = (char *)&p[i+1];
			code = atoi (indx);
		    } else if (!name) {
			name = (char *)&p[i+1];
		    } else {
		       i++;
		       break;
		    }
		}
		/*
		**  Add data to field structure.
		*/
		if (name) {
		    if (code == prev_code) {
			/*
			**  Remaining data are description.
			**  Save in current info block.
			*/
			alen = strlen((char *)&p[i]) + 1;
			if (alen > sizeof(last->desc_buf)) {
			    if (last->description != last->desc_buf)
				FREE(last->description);
			    if (!(last->description = (char *)malloc(alen))) {
				outofmem(__FILE__, "HTLoadCSO");
			    }
			}
			strcpy(last->description, (char *)&p[i]);
		    } else {
			/*
			**  Initialize new block, append to end of list
			**  to preserve order.
			*/
			new = (CSOfield_info *)calloc(1, sizeof(CSOfield_info));
			if (!new) {
			    outofmem(__FILE__, "HTLoadCSO");
			}
			if (last)
			    last->next = new;
			else
			    CSOfields = new;
			last = new;

			new->next = (CSOfield_info *) 0;
			new->name = new->name_buf;
			alen = strlen(name) + 1;
			if (alen > sizeof(new->name_buf)) {
			    if (!(new->name = (char *)malloc(alen))) {
				outofmem(__FILE__, "HTLoadCSO");
			    }
			}
			strcpy (new->name, name);

			new->attributes = new->attr_buf;
			alen = strlen((char *)&p[i]) + 2;
			if (alen > sizeof(new->attr_buf)) {
			    if (!(new->attributes = (char *)malloc(alen))) {
				outofmem(__FILE__, "HTLoadCSO");
			    }
			}
			strcpy(new->attributes, (char *)&p[i]);
			strcpy((char *)&new->attributes[alen-2], " ");
			new->description = new->desc_buf;
			new->desc_buf[0] = '\0';
			new->id = atoi(indx);
			/*
			**  Scan for keywords.
			*/
			parse_cso_field_info(new);
		    }
		    prev_code = code;
		} else
		    break;
	    } /* end if *p == '-' */
	} /* if end of line */

    } /* Loop over characters */

    /* end the text block */

    if (buf[0] == '\0') {
	return -1; /* no response */
    }
    buf[0] = '\0';
    return 0;  /* all done */
} /* end of procedure */

/*	Generate a form for submitting CSO/PH searches. - FM
**	====================================================
*/
PRIVATE int generate_cso_form ARGS4(
	char *,		host,
	int,		port,
	char *,		buf,
	HTStream *,	Target)
{
    int i, j, length;
    size_t out;
    int full_flag = 1;
    char *key, *line;
    CSOformgen_context ctx;
    static char *template[] = {
   "<HEAD>\n<TITLE>CSO/PH Query Form for $(HOST)</TITLE>\n</HEAD>\n<BODY>",
   "<H2><I>CSO/PH Query Form</I> for <EM>$(HOST)</EM></H2>",
   "To search the database for a name, fill in one or more of the fields",
   "in the form below and activate the 'Submit query' button.  At least",
   "one of the entered fields must be flagged as indexed.",
   "<HR><FORM method=\"POST\" action=\"cso://$(HOST)/\">",
   "[ <input type=\"submit\" value=\"Submit query\"> | ",
   "<input type=\"reset\" value=\"Clear fields\"> ]",
   "<P><DL>",
   "   <DT>Search parameters (* indicates indexed field):",
   "   <DD>", "$(NAMEFLD)    <DL COMPACT>\n    <DT><I>$(FDESC)</I>$(FNDX)",
   "    <DD>Last: <input name=\"q_$(FID)\" type=\"text\" size=49$(FSIZE2)>",
   "    <DD>First: <input name=\"q_$(FID)\" type=\"text\" size=48$(FSIZE2)>",
   "$(QFIELDS)    <DT><I>$(FDESC)</I>$(FNDX)",
   "    <DD><input name=\"q_$(FID)\" type=\"text\" $(FSIZE)>\n$(NEXTFLD)",
   "    </DL>",
   "   </DL>\n<P><DL>",
   "   <DT>Output format:",
   "   <DD>Returned data option: <select name=\"return\">",
   "    <option>default<option selected>all<option>selected</select><BR>",
   "$(RFIELDS)    <input type=\"checkbox\" name=\"r_$(FID)\"$(FDEF)> $(FDESC)<BR>",
   "$(NEXTFLD)    ",
   "   </DL></FORM><HR>\n</BODY>\n</HTML>",
   (char *) 0
    };

    out = 0;
    ctx.host = host;
    ctx.seek = (char *) 0;
    ctx.port = port;
    ctx.fld = (CSOfield_info *) 0;
    ctx.public_override = full_flag;
    /*
    **	Parse the strings in the template array to produce HTML document
    **	to send to client.  First line is skipped for 'full' lists.
    */
    out = 0;
    buf[out] = '\0';
    for (i = full_flag ? /***1***/ 0 : 0; template[i]; i++) {
	/*
	**  Search the current string for substitution, flagged by $(
	*/
	for (line=template[i], j = 0; line[j]; j++) {
	    if ((line[j] == '$') && (line[j+1] == '(')) {
		/*
		** Command detected, flush output buffer and find closing ')'
		** that delimits the command.
		*/
		buf[out] = '\0';
		if (out > 0)
		    (*Target->isa->put_block)(Target, buf, strlen(buf));
		out = 0;
		for (key = &line[j]; line[j+1] && (line[j] != ')'); j++)
		    ;
		/*
		**  Save context, interpet command and restore updated context.
		*/
		ctx.cur_line = i;
		ctx.cur_off = j;
		interpret_cso_key(key, buf, &length, &ctx, Target);
		i = ctx.cur_line;
		j = ctx.cur_off;
		line = template[i];
		out = length;

		if (ctx.seek) {
		    /*
		    **	Command wants us to skip (forward) to indicated token.
		    **	Start at current position.
		    */
		    int slen = strlen(ctx.seek);
		    for (; template[i]; i++) {
			for (line = template[i]; line[j]; j++) {
			    if (line[j] == '$')
				if (0 == strncmp(ctx.seek, &line[j], slen)) {
				    if (j == 0)
					j = strlen(template[--i])-1;
				    else
					--j;
				    line = template[i];
				    ctx.seek = (char *) 0;
				    break;
				}
			}
			if (!ctx.seek)
			    break;
			j = 0;
		    }
		    if (ctx.seek) {
			char *temp = 0;
			HTSprintf0(&temp, GOPHER_CSO_SEEK_FAILED, ctx.seek);
			(*Target->isa->put_block)(Target, temp, strlen(temp));
			FREE(temp);
		    }
		}
	    } else {
		/*
		**  Non-command text, add to output buffer.
		*/
		buf[out++] = line[j];
		if (out > (sizeof(buf)-3)) {
		    buf[out] = '\0';
			(*Target->isa->put_block)(Target, buf, strlen(buf));
		    out = 0;
		}
	    }
	}
	buf[out++] = '\n';
	buf[out] = '\0';
    }
    if (out > 0)
	(*Target->isa->put_block)(Target, buf, strlen(buf));

    return 0;
}

/*	Generate a results report for CSO/PH form-based searches. - FM
**	==============================================================
*/
PRIVATE int generate_cso_report ARGS1(
	HTStream *,	Target)
{
    int ich;
    char line[BIG];
    char *buf = 0;
    char *p = line, *href = NULL;
    int len, i, prev_ndx, ndx;
    char *rcode, *ndx_str, *fname, *fvalue, *l;
    CSOfield_info *fld;
    BOOL stop = FALSE;

    /*
    **	Read lines until non-negative status.
    */
    prev_ndx = -100;
    /*
    **	Start grabbing chars from the network.
    */
    while (!stop && (ich = NEXT_CHAR) != EOF) {
	if (interrupted_in_htgetcharacter) {
	    CTRACE(tfp, "HTLoadCSO: Interrupted in HTGetCharacter, apparently.\n");
	    _HTProgress (CONNECTION_INTERRUPTED);
	    goto end_CSOreport;
	}

	if ((char)ich != LF) {
	    *p = ich;		/* Put character in line */
	    if (p < &line[BIG-1]) {
		p++;
	    }
	} else {
	    *p = '\0';		/* Terminate line */
	    /*
	    **	OK we now have a line.
	    **	Load it as 'p' and parse it.
	    */
	    p = line;
	    if (p[0] != '-' && p[0] != '1') {
		stop = TRUE;
	    }
	    rcode = (p[0] == '-') ? &p[1] : p;
	    ndx_str = fname = NULL;
	    len = strlen(p);
	    for (i = 0; i < len; i++) {
		if (p[i] == ':') {
		    p[i] = '\0';
		    if (!ndx_str) {
			fname = ndx_str = &p[i+1];
		    } else {
			fname = &p[i+1];
			break;
		    }
		}
	    }
	    if (ndx_str) {
		ndx = atoi(ndx_str);
		if (prev_ndx != ndx) {
		    if (prev_ndx != -100) {
			HTSprintf0(&buf, "</DL></DL>\n");
			(*Target->isa->put_block)(Target, buf, strlen(buf));
		    }
		    if (ndx == 0) {
			HTSprintf0(&buf,
		  "<HR><DL><DT>Information/status<DD><DL><DT>\n");
			(*Target->isa->put_block)(Target, buf, strlen(buf));
		    } else {
			HTSprintf0(&buf,
	      "<HR><DL><DT>Entry %d:<DD><DL COMPACT><DT>\n", ndx);
			(*Target->isa->put_block)(Target, buf, strlen(buf));
		    }
		    prev_ndx = ndx;
		}
	    } else {
		HTSprintf0(&buf, "<DD>%s\n", rcode);
		(*Target->isa->put_block)(Target, buf, strlen(buf));
		continue;
	    }
	    if ((*rcode >= '2') && (*rcode <= '5') && (fname != ndx_str)) {
		while (*fname == ' ') {
		    fname++;	/* trim leading spaces */
		}
		for (fvalue = fname; *fvalue; fvalue++) {
		    if (*fvalue == ':') {
			*fvalue++ = '\0';
			i = strlen(fname) - 1;
			while (i >= 0 && fname[i] == ' ') {
			    fname[i--] = '\0'; /* trim trailing */
			}
			break;
		    }
		}
		if (fvalue) {
		    while (*fvalue == ' ') {
			fvalue++;	/* trim leading spaces */
		    }
		}
		if (*fname) {
		    for (fld = CSOfields; fld; fld = fld->next) {
			if (!strcmp(fld->name, fname)) {
			    if (fld->description) {
				fname = fld->description;
			    }
			    break;
			}
		    }
		    if (fld && fld->url) {
			HTSprintf0(&buf,
				"<DT><I>%s</I><DD><A HREF=\"%s\">%s</A>\n",
				fname, fvalue, fvalue);
			(*Target->isa->put_block)(Target, buf, strlen(buf));
		    } else {
			HTSprintf0(&buf, "<DT><I>%s</I><DD>", fname);
			(*Target->isa->put_block)(Target, buf, strlen(buf));
			buf[0] = '\0';
			l = fvalue;
			while (*l) {
			    if (*l == '<') {
				StrAllocCat(buf, "&lt;");
				l++;
			    } else if (*l == '>') {
				StrAllocCat(buf, "&gt;");
				l++;
			    } else if (strncmp(l, "news:", 5) &&
				       strncmp(l, "snews://", 8) &&
				       strncmp(l, "nntp://", 7) &&
				       strncmp(l, "snewspost:", 10) &&
				       strncmp(l, "snewsreply:", 11) &&
				       strncmp(l, "newspost:", 9) &&
				       strncmp(l, "newsreply:", 10) &&
				       strncmp(l, "ftp://", 6) &&
				       strncmp(l, "file:/", 6) &&
				       strncmp(l, "finger://", 9) &&
				       strncmp(l, "http://", 7) &&
				       strncmp(l, "https://", 8) &&
				       strncmp(l, "wais://", 7) &&
				       strncmp(l, "mailto:", 7) &&
				       strncmp(l, "cso://", 6) &&
				       strncmp(l, "gopher://", 9)) {
				HTSprintf(&buf, "%c", *l++);
			    } else {
				StrAllocCat(buf, "<a href=\"");
				StrAllocCopy(href, l);
				StrAllocCat(buf, strtok(href, " \r\n\t,>)\""));
				StrAllocCat(buf, "\">");
				while (*l && !strchr(" \r\n\t,>)\"", *l)) {
				    HTSprintf(&buf, "%c", *l++);
				}
				StrAllocCat(buf, "</a>");
				FREE(href);
			    }
			}
			StrAllocCat(buf, "\n");
			(*Target->isa->put_block)(Target, buf, strlen(buf));
		    }
		} else {
		    HTSprintf0(&buf, "<DD>");
		    (*Target->isa->put_block)(Target, buf, strlen(buf));
		    buf[0] = '\0';
		    l = fvalue;
		    while (*l) {
			if (*l == '<') {
			    StrAllocCat(buf, "&lt;");
			    l++;
			} else if (*l == '>') {
			    StrAllocCat(buf, "&gt;");
			    l++;
			} else if (strncmp(l, "news:", 5) &&
				   strncmp(l, "snews://", 8) &&
				   strncmp(l, "nntp://", 7) &&
				   strncmp(l, "snewspost:", 10) &&
				   strncmp(l, "snewsreply:", 11) &&
				   strncmp(l, "newspost:", 9) &&
				   strncmp(l, "newsreply:", 10) &&
				   strncmp(l, "ftp://", 6) &&
				   strncmp(l, "file:/", 6) &&
				   strncmp(l, "finger://", 9) &&
				   strncmp(l, "http://", 7) &&
				   strncmp(l, "https://", 8) &&
				   strncmp(l, "wais://", 7) &&
				   strncmp(l, "mailto:", 7) &&
				   strncmp(l, "cso://", 6) &&
				   strncmp(l, "gopher://", 9)) {
			    HTSprintf(&buf, "%c", *l++);
			} else {
			    StrAllocCat(buf, "<a href=\"");
			    StrAllocCopy(href, l);
			    StrAllocCat(buf, strtok(href, " \r\n\t,>)\""));
			    StrAllocCat(buf, "\">");
			    while (*l && !strchr(" \r\n\t,>)\"", *l)) {
				HTSprintf(&buf, "%c", *l++);
			    }
			    StrAllocCat(buf, "</a>");
			    FREE(href);
			}
		    }
		    StrAllocCat(buf, "\n");
		    (*Target->isa->put_block)(Target, buf, strlen(buf));
		}
	    } else {
		HTSprintf0(&buf, "<DD>%s\n", fname ? fname : rcode );
		(*Target->isa->put_block)(Target, buf, strlen(buf));
	    }
	}
    }
end_CSOreport:
    if (prev_ndx != -100) {
	HTSprintf0(&buf, "</DL></DL>\n");
	(*Target->isa->put_block)(Target, buf, strlen(buf));
    }
    FREE(buf);
    return 0;
}

/*	CSO/PH form-based search gateway - FM			HTLoadCSO
**	=====================================
*/
PRIVATE int HTLoadCSO ARGS4(
	CONST char *,		arg,
	HTParentAnchor *,	anAnchor,
	HTFormat,		format_out,
	HTStream*,		sink)
{
    static CONST char end_form[] = "</BODY>\n</HTML>\n";
    char *host, *cp;
    int port = CSO_PORT;
    int status;				/* tcp return */
    char *command = NULL;
    char *content = NULL;
    int len, i, j, start, finish, flen, ndx;
    int return_type, has_indexed;
    CSOfield_info *fld;
    char buf[2048];
    HTFormat format_in = WWW_HTML;
    HTStream *Target = NULL;

    if (!acceptable_inited)
	 init_acceptable();

    if (!arg)
	return -3;		/* Bad if no name sepcified	*/
    if (!*arg)
	return -2;		/* Bad if name had zero length	*/
    CTRACE(tfp, "HTLoadCSO: Looking for %s\n", arg);

    /*
    **	Set up a socket to the server for the data.
    */
    status = HTDoConnect (arg, "cso", CSO_PORT, &s);
    if (status == HT_INTERRUPTED) {
	/*
	**  Interrupt cleanly.
	*/
	CTRACE(tfp, "HTLoadCSO: Interrupted on connect; recovering cleanly.\n");
	_HTProgress (CONNECTION_INTERRUPTED);
	return HT_NOT_LOADED;
    }
    if (status < 0) {
	CTRACE(tfp, "HTLoadCSO: Unable to connect to remote host for `%s'.\n",
		    arg);
	return HTInetStatus("connect");
    }

    HTInitInput(s);		/* Set up input buffering */

    HTSprintf0(&command, "fields%c%c", CR, LF);
    CTRACE(tfp, "HTLoadCSO: Connected, writing command `%s' to socket %d\n",
		command, s);
    _HTProgress (GOPHER_SENDING_CSO_REQUEST);
    status = NETWRITE(s, command, (int)strlen(command));
    FREE(command);
    if (status < 0) {
	CTRACE(tfp, "HTLoadCSO: Unable to send command.\n");
	return HTInetStatus("send");
    }
    _HTProgress (GOPHER_SENT_CSO_REQUEST);

    /*
    **	Now read the data from the socket.
    */
    status = parse_cso_fields(buf, sizeof(buf));
    if (status) {
	NETCLOSE(s);
	if (status == HT_INTERRUPTED) {
	    _HTProgress (CONNECTION_INTERRUPTED);
	} else if (buf[0] != '\0') {
	    HTAlert(buf);
	} else {
	    HTAlert(FAILED_NO_RESPONSE);
	}
	return HT_NOT_LOADED;
    }
    Target = HTStreamStack(format_in,
			   format_out,
			   sink, anAnchor);
    if (!Target || Target == NULL) {
	char *temp = 0;
	HTSprintf0(&temp, CANNOT_CONVERT_I_TO_O,
		   HTAtom_name(format_in), HTAtom_name(format_out));
	HTAlert(temp);
	FREE(temp);
	NETCLOSE(s);
	return HT_NOT_LOADED;
    }
    host = HTParse(arg, "", PARSE_HOST);
    if ((cp=strchr(host, ':')) != NULL) {
	if (cp[1] >= '0' && cp[1] <= '9') {
	    port = atoi((cp+1));
	    if (port == CSO_PORT) {
		*cp = '\0';
	    }
	}
    }
    anAnchor->safe = TRUE;
    if (!(anAnchor->post_data && *anAnchor->post_data)) {
	generate_cso_form(host, port, buf, Target);
	(*Target->isa->_free)(Target);
	FREE(host);
	NETCLOSE(s);
	free_CSOfields();
	return HT_LOADED;
    }

    HTSprintf0(&command,
     "<HTML>\n<HEAD>\n<TITLE>CSO/PH Results on %s</TITLE>\n</HEAD>\n<BODY>\n",
	    host);
    (*Target->isa->put_block)(Target, command, strlen(command));
    FREE(command);
    FREE(host);

    StrAllocCopy(content, anAnchor->post_data);
    if (content[strlen(content)-1] != '&')
	StrAllocCat(content, "&");
    len = strlen(content);
    for (i = 0; i < len; i++) {
	if (content[i] == '+') {
	    content[i] = ' ';
	}
    }
    HTUnEscape(content);
    len = strlen(content);
    return_type = 0;
    has_indexed = 0;
    start = finish = 0;
    for (i = 0; i < len; i++) {
	if (!content[i] || content[i] == '&') {
	    /*
	    **	Value parsed.  Unescape characters and look for first '='
	    **	to delimit field name from value.
	    */
	    flen = i - start;
	    finish = start + flen;
	    content[finish] = '\0';
	    for (j = start; j < finish; j++) {
		if (content[j] == '=') {
		    /*
		    **	content[start..j-1] is field name,
		    **	[j+1..finish-1] is value.
		    */
		    if ((content[start+1] == '_') &&
			((content[start] == 'r') || (content[start] == 'q'))) {
			/*
			**  Decode fields number and lookup field info.
			*/
			sscanf (&content[start+2], "%d=", &ndx);
			for (fld = CSOfields; fld; fld = fld->next) {
			    if (ndx==fld->id) {
				if ((j+1) >= finish)
				    break;	/* ignore nulls */
				if (content[start] == 'q') {
				    /*
				     * Append field to query line.
				     */
				    if (fld->lookup) {
					if (fld->indexed)
					    has_indexed = 1;
					if (command == 0 || *command == 0) {
					    StrAllocCopy(command, "query ");
					} else {
					    StrAllocCat(command, " ");
					}
					HTSprintf(&command, "%s=\"%s\"",
						  fld->name, &content[j+1]);
				    } else {
					strcpy(buf,
				"Warning: non-lookup field ignored<BR>\n");
					(*Target->isa->put_block)(Target,
								  buf,
								  strlen(buf));
				    }
				} else if (content[start] == 'r') {
				    fld->explicit_return = 1;
				}
				break;
			    }
			}
		    } else if (!strncmp(&content[start],"return=",7)) {
			if (!strcmp(&content[start+7],"all")) {
			    return_type = 1;
			} else if (!strcmp(&content[start+7],"selected")) {
			    return_type = 2;
			}
		    }
		}
	    }
	    start = i + 1;
	}
    }
    FREE(content);
    if ((command == 0 || *command == 0) || !has_indexed) {
	NETCLOSE(s);
	strcpy(buf,
  "<EM>Error:</EM> At least one indexed field value must be specified!\n");
	(*Target->isa->put_block)(Target, buf, strlen(buf));
	strcpy(buf, "</BODY>\n</HTML>\n");
	(*Target->isa->put_block)(Target, buf, strlen(buf));
	(*Target->isa->_free)(Target);
	free_CSOfields();
	return HT_LOADED;
    }
    /*
    **	Append return fields.
    */
    if (return_type == 1) {
	StrAllocCat(command, " return all");
    } else if (return_type == 2) {
	StrAllocCat(command, " return");
	for (fld = CSOfields; fld; fld = fld->next) {
	    if (fld->explicit_return) {
		HTSprintf(&command, " %s", fld->name);
	    }
	}
    }
    HTSprintf(&command, "%c%c", CR, LF);
    strcpy(buf, "<H2>\n<EM>CSO/PH command:</EM> ");
    (*Target->isa->put_block)(Target, buf, strlen(buf));
    (*Target->isa->put_block)(Target, command, strlen(command));
    strcpy(buf, "</H2>\n");
    (*Target->isa->put_block)(Target, buf, strlen(buf));
    CTRACE(tfp, "HTLoadCSO: Writing command `%s' to socket %d\n",
		command, s);
    status = NETWRITE(s, command, strlen(command));
    FREE(command);
    if (status < 0) {
	CTRACE(tfp, "HTLoadCSO: Unable to send command.\n");
	free_CSOfields();
	return HTInetStatus("send");
    }
    generate_cso_report(Target);
    NETCLOSE(s);
    (*Target->isa->put_block)(Target, end_form, sizeof(end_form)-1);
    (*Target->isa->_free)(Target);
    FREE(host);
    free_CSOfields();
    return HT_LOADED;
}

/*	Load by name.						HTLoadGopher
**	=============
**
**  Bug:  No decoding of strange data types as yet.
**
*/
PRIVATE int HTLoadGopher ARGS4(
	CONST char *,		arg,
	HTParentAnchor *,	anAnchor,
	HTFormat,		format_out,
	HTStream*,		sink)
{
    char *command;			/* The whole command */
    int status;				/* tcp return */
    char gtype;				/* Gopher Node type */
    char * selector;			/* Selector string */

    if (!acceptable_inited)
	 init_acceptable();

    if (!arg)
	return -3;		/* Bad if no name sepcified	*/
    if (!*arg)
	return -2;		/* Bad if name had zero length	*/
    CTRACE(tfp, "HTGopher: Looking for %s\n", arg);

    /*
    **	If it's a port 105 GOPHER_CSO gtype with no ISINDEX token ('?'),
    **	use the form-based CSO gateway (otherwise, return an ISINDEX
    **	cover page or do the ISINDEX search). - FM
    */
    {
	int len;

	if ((len = strlen(arg)) > 5) {
	    if (0 == strcmp((CONST char *)&arg[len-6], ":105/2")) {
		/* Use CSO gateway. */
		CTRACE(tfp, "HTGopher: Passing to CSO/PH gateway.\n");
		return HTLoadCSO(arg, anAnchor, format_out, sink);
	    }
	}
    }

    /*
    **	If it's a port 79/0[/...] URL, use the finger gateway. - FM
    */
    if (strstr(arg, ":79/0") != NULL) {
#ifndef DISABLE_FINGER
	CTRACE(tfp, "HTGopher: Passing to finger gateway.\n");
	return HTLoadFinger(arg, anAnchor, format_out, sink);
#else /* finger is disabled */
	HTAlert(gettext("Unable to access document!"));
	return HT_NOT_LOADED;
#endif /* DISABLE_FINGER */
    }

    /*
    **	Get entity type, and selector string.
    */
    {
	char * p1 = HTParse(arg, "", PARSE_PATH|PARSE_PUNCTUATION);
	gtype = '1';		/* Default = menu */
	selector = p1;
	if ((*selector++=='/') && (*selector)) {	/* Skip first slash */
	    gtype = *selector++;			/* Pick up gtype */
	}
	if (gtype == GOPHER_INDEX) {
	    char * query;
	    /*
	    **	Search is allowed.
	    */
	    HTAnchor_setIndex(anAnchor, anAnchor->address);
	    query = strchr(selector, '?');	/* Look for search string */
	    if (!query || !query[1]) {		/* No search required */
		target = HTML_new(anAnchor, format_out, sink);
		targetClass = *target->isa;
		display_index(arg, anAnchor);	/* Display "cover page" */
		return HT_LOADED;		/* Local function only */
	    }
	    *query++ = '\0';			/* Skip '?'	*/
	    command =
		    (char *)malloc(strlen(selector)+ 1 + strlen(query)+ 2 + 1);
	      if (command == NULL)
		  outofmem(__FILE__, "HTLoadGopher");

	    de_escape(command, selector);	/* Bug fix TBL 921208 */

	    strcat(command, "\t");

	    {					/* Remove plus signs 921006 */
		char *p;
		for (p=query; *p; p++) {
		    if (*p == '+') *p = ' ';
		}
	    }

	    de_escape(&command[strlen(command)], query);/* bug fix LJM 940415 */
	} else if (gtype == GOPHER_CSO) {
	    char * query;
	    /*
	    **	Search is allowed.
	    */
	    query = strchr(selector, '?');	/* Look for search string */
	    if (!query || !query[1]) {		/* No search required */
		target = HTML_new(anAnchor, format_out, sink);
		targetClass = *target->isa;
		display_cso(arg, anAnchor);	/* Display "cover page" */
		return HT_LOADED;		/* Local function only */
	    }
	    HTAnchor_setIndex(anAnchor, anAnchor->address);
	    *query++ = '\0';			/* Skip '?'	*/
	    command = (char *)malloc(strlen("query")+1 + strlen(query)+2+1);
	      if (command == NULL)
		  outofmem(__FILE__, "HTLoadGopher");

	    de_escape(command, selector);	/* Bug fix TBL 921208 */

	    strcpy(command, "query ");

	    {					/* Remove plus signs 921006 */
		char *p;
		for (p=query; *p; p++) {
		    if (*p == '+') *p = ' ';
		}
	    }
	    de_escape(&command[strlen(command)], query);/* bug fix LJM 940415 */

	} else {				/* Not index */
	    command = (char *)malloc(strlen(selector)+2+1);
	    if (command == NULL)
		outofmem(__FILE__, "HTLoadGopher");
	    de_escape(command, selector);
	}
	FREE(p1);
    }

    {
	char * p = command + strlen(command);
	*p++ = CR;		/* Macros to be correct on Mac */
	*p++ = LF;
	*p++ = '\0';
    }

    /*
    **	Set up a socket to the server for the data.
    */
    status = HTDoConnect (arg, "gopher", GOPHER_PORT, &s);
    if (status == HT_INTERRUPTED) {
	/*
	**  Interrupt cleanly.
	*/
	CTRACE(tfp, "HTGopher: Interrupted on connect; recovering cleanly.\n");
	_HTProgress (CONNECTION_INTERRUPTED);
	FREE(command);
	return HT_NOT_LOADED;
    }
    if (status < 0) {
	CTRACE(tfp, "HTGopher: Unable to connect to remote host for `%s'.\n",
		    arg);
	FREE(command);
	return HTInetStatus("connect");
    }

    HTInitInput(s);		/* Set up input buffering */

    CTRACE(tfp, "HTGopher: Connected, writing command `%s' to socket %d\n",
		command, s);

#ifdef NOT_ASCII
    {
	char * p;
	for (p = command; *p; p++) {
	    *p = TOASCII(*p);
	}
    }
#endif

    _HTProgress (GOPHER_SENDING_REQUEST);

    status = NETWRITE(s, command, (int)strlen(command));
    FREE(command);
    if (status < 0) {
	CTRACE(tfp, "HTGopher: Unable to send command.\n");
	return HTInetStatus("send");
    }

    _HTProgress (GOPHER_SENT_REQUEST);

    /*
    **	Now read the data from the socket.
    */
    switch (gtype) {

    case GOPHER_TEXT :
	HTParseSocket(WWW_PLAINTEXT, format_out, anAnchor, s, sink);
	break;

    case GOPHER_HTML :
    case GOPHER_CHTML :
	HTParseSocket(WWW_HTML, format_out, anAnchor, s, sink);
	break;

    case GOPHER_GIF:
    case GOPHER_IMAGE:
    case GOPHER_PLUS_IMAGE:
	HTParseSocket(HTAtom_for("image/gif"),
			   format_out, anAnchor, s, sink);
	break;

    case GOPHER_MENU :
    case GOPHER_INDEX :
	target = HTML_new(anAnchor, format_out, sink);
	targetClass = *target->isa;
	parse_menu(arg, anAnchor);
	break;

    case GOPHER_CSO:
	target = HTML_new(anAnchor, format_out, sink);
	targetClass = *target->isa;
	parse_cso(arg, anAnchor);
	break;

    case GOPHER_SOUND :
    case GOPHER_PLUS_SOUND :
	HTParseSocket(WWW_AUDIO, format_out, anAnchor, s, sink);
	break;

    case GOPHER_PLUS_MOVIE:
	HTParseSocket(HTAtom_for("video/mpeg"), format_out, anAnchor, s, sink);
	break;

    case GOPHER_PLUS_PDF:
	HTParseSocket(HTAtom_for("application/pdf"), format_out, anAnchor,
				  s, sink);
	break;

    case GOPHER_MACBINHEX:
    case GOPHER_PCBINARY:
    case GOPHER_UUENCODED:
    case GOPHER_BINARY:
    default:
	/*
	**  Specifying WWW_UNKNOWN forces dump to local disk.
	*/
	HTParseSocket (WWW_UNKNOWN, format_out, anAnchor, s, sink);
	break;

    } /* switch(gtype) */

    NETCLOSE(s);
    return HT_LOADED;
}

#ifdef GLOBALDEF_IS_MACRO
#define _HTGOPHER_C_1_INIT { "gopher", HTLoadGopher, NULL }
GLOBALDEF (HTProtocol, HTGopher, _HTGOPHER_C_1_INIT);
#define _HTCSO_C_1_INIT { "cso", HTLoadCSO, NULL }
GLOBALDEF (HTProtocol, HTCSO, _HTCSO_C_1_INIT);
#else
GLOBALDEF PUBLIC HTProtocol HTGopher = { "gopher", HTLoadGopher, NULL };
GLOBALDEF PUBLIC HTProtocol HTCSO = { "cso", HTLoadCSO, NULL };
#endif /* GLOBALDEF_IS_MACRO */

#endif /* not DISABLE_GOPHER */
