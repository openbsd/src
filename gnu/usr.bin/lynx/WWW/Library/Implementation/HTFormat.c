/*		Manage different file formats			HTFormat.c
 *		=============================
 *
 * Bugs:
 *	Not reentrant.
 *
 *	Assumes the incoming stream is ASCII, rather than a local file
 *	format, and so ALWAYS converts from ASCII on non-ASCII machines.
 *	Therefore, non-ASCII machines can't read local files.
 *
 */

#include <HTUtils.h>

/* Implements:
*/
#include <HTFormat.h>

static float HTMaxSecs = 1e10;	/* No effective limit */

#ifdef UNIX
#ifdef NeXT
#define PRESENT_POSTSCRIPT "open %s; /bin/rm -f %s\n"
#else
#define PRESENT_POSTSCRIPT "(ghostview %s ; /bin/rm -f %s)&\n"
			   /* Full pathname would be better! */
#endif /* NeXT */
#endif /* UNIX */

#include <HTML.h>
#include <HTMLDTD.h>
#include <HText.h>
#include <HTAlert.h>
#include <HTList.h>
#include <HTInit.h>
#include <HTTCP.h>
#include <HTTP.h>
/*	Streams and structured streams which we use:
*/
#include <HTFWriter.h>
#include <HTPlain.h>
#include <SGML.h>
#include <HTMLGen.h>

#include <LYexit.h>
#include <LYUtils.h>
#include <GridText.h>
#include <LYGlobalDefs.h>
#include <LYLeaks.h>

#ifdef DISP_PARTIAL
#include <LYMainLoop.h>
#endif

BOOL HTOutputSource = NO;	/* Flag: shortcut parser to stdout */

#ifdef ORIGINAL
struct _HTStream {
    const HTStreamClass *isa;
    /* ... */
};
#endif /* ORIGINAL */

/* this version used by the NetToText stream */
struct _HTStream {
    const HTStreamClass *isa;
    BOOL had_cr;
    HTStream *sink;
};

/*	Presentation methods
 *	--------------------
 */
HTList *HTPresentations = NULL;
HTPresentation *default_presentation = NULL;

/*
 *	To free off the presentation list.
 */
#ifdef LY_FIND_LEAKS
static void HTFreePresentations(void);
#endif

/*	Define a presentation system command for a content-type
 *	-------------------------------------------------------
 */
void HTSetPresentation(const char *representation,
		       const char *command,
		       const char *testcommand,
		       double quality,
		       double secs,
		       double secs_per_byte,
		       long int maxbytes,
		       AcceptMedia media)
{
    HTPresentation *pres = typecalloc(HTPresentation);

    if (pres == NULL)
	outofmem(__FILE__, "HTSetPresentation");

    pres->rep = HTAtom_for(representation);
    pres->rep_out = WWW_PRESENT;	/* Fixed for now ... :-) */
    pres->converter = HTSaveAndExecute;		/* Fixed for now ...     */
    pres->quality = (float) quality;
    pres->secs = (float) secs;
    pres->secs_per_byte = (float) secs_per_byte;
    pres->maxbytes = maxbytes;
    pres->get_accept = 0;
    pres->accept_opt = media;

    pres->command = NULL;
    StrAllocCopy(pres->command, command);

    pres->testcommand = NULL;
    StrAllocCopy(pres->testcommand, testcommand);

    /*
     * Memory leak fixed.
     * 05-28-94 Lynx 2-3-1 Garrett Arch Blythe
     */
    if (!HTPresentations) {
	HTPresentations = HTList_new();
#ifdef LY_FIND_LEAKS
	atexit(HTFreePresentations);
#endif
    }

    if (strcmp(representation, "*") == 0) {
	FREE(default_presentation);
	default_presentation = pres;
    } else {
	HTList_addObject(HTPresentations, pres);
    }
}

/*	Define a built-in function for a content-type
 *	---------------------------------------------
 */
void HTSetConversion(const char *representation_in,
		     const char *representation_out,
		     HTConverter *converter,
		     float quality,
		     float secs,
		     float secs_per_byte,
		     long int maxbytes,
		     AcceptMedia media)
{
    HTPresentation *pres = typecalloc(HTPresentation);

    if (pres == NULL)
	outofmem(__FILE__, "HTSetConversion");

    pres->rep = HTAtom_for(representation_in);
    pres->rep_out = HTAtom_for(representation_out);
    pres->converter = converter;
    pres->command = NULL;
    pres->testcommand = NULL;
    pres->quality = quality;
    pres->secs = secs;
    pres->secs_per_byte = secs_per_byte;
    pres->maxbytes = maxbytes;
    pres->get_accept = TRUE;
    pres->accept_opt = media;

    /*
     * Memory Leak fixed.
     * 05-28-94 Lynx 2-3-1 Garrett Arch Blythe
     */
    if (!HTPresentations) {
	HTPresentations = HTList_new();
#ifdef LY_FIND_LEAKS
	atexit(HTFreePresentations);
#endif
    }

    HTList_addObject(HTPresentations, pres);
}

#ifdef LY_FIND_LEAKS
/*
 *	Purpose:	Free the presentation list.
 *	Arguments:	void
 *	Return Value:	void
 *	Remarks/Portability/Dependencies/Restrictions:
 *		Made to clean up Lynx's bad leakage.
 *	Revision History:
 *		05-28-94	created Lynx 2-3-1 Garrett Arch Blythe
 */
static void HTFreePresentations(void)
{
    HTPresentation *pres = NULL;

    /*
     * Loop through the list.
     */
    while (!HTList_isEmpty(HTPresentations)) {
	/*
	 * Free off each item.  May also need to free off it's items, but not
	 * sure as of yet.
	 */
	pres = (HTPresentation *) HTList_removeLastObject(HTPresentations);
	FREE(pres->command);
	FREE(pres->testcommand);
	FREE(pres);
    }
    /*
     * Free the list itself.
     */
    HTList_delete(HTPresentations);
    HTPresentations = NULL;
}
#endif /* LY_FIND_LEAKS */

/*	File buffering
 *	--------------
 *
 *	The input file is read using the macro which can read from
 *	a socket or a file.
 *	The input buffer size, if large will give greater efficiency and
 *	release the server faster, and if small will save space on PCs etc.
 */
#define INPUT_BUFFER_SIZE 4096	/* Tradeoff */
static char input_buffer[INPUT_BUFFER_SIZE];
static char *input_pointer;
static char *input_limit;
static int input_file_number;

/*	Set up the buffering
 *
 *	These routines are public because they are in fact needed by
 *	many parsers, and on PCs and Macs we should not duplicate
 *	the static buffer area.
 */
void HTInitInput(int file_number)
{
    input_file_number = file_number;
    input_pointer = input_limit = input_buffer;
}

int interrupted_in_htgetcharacter = 0;
int HTGetCharacter(void)
{
    char ch;

    interrupted_in_htgetcharacter = 0;
    do {
	if (input_pointer >= input_limit) {
	    int status = NETREAD(input_file_number,
				 input_buffer, INPUT_BUFFER_SIZE);

	    if (status <= 0) {
		if (status == 0)
		    return EOF;
		if (status == HT_INTERRUPTED) {
		    CTRACE((tfp, "HTFormat: Interrupted in HTGetCharacter\n"));
		    interrupted_in_htgetcharacter = 1;
		    return EOF;
		}
		CTRACE((tfp, "HTFormat: File read error %d\n", status));
		return EOF;	/* -1 is returned by UCX
				   at end of HTTP link */
	    }
	    input_pointer = input_buffer;
	    input_limit = input_buffer + status;
	}
	ch = *input_pointer++;
    } while (ch == (char) 13);	/* Ignore ASCII carriage return */

    return FROMASCII(UCH(ch));
}

#ifdef USE_SSL
char HTGetSSLCharacter(void *handle)
{
    char ch;

    interrupted_in_htgetcharacter = 0;
    if (!handle)
	return (char) EOF;
    do {
	if (input_pointer >= input_limit) {
	    int status = SSL_read((SSL *) handle,
				  input_buffer, INPUT_BUFFER_SIZE);

	    if (status <= 0) {
		if (status == 0)
		    return (char) EOF;
		if (status == HT_INTERRUPTED) {
		    CTRACE((tfp,
			    "HTFormat: Interrupted in HTGetSSLCharacter\n"));
		    interrupted_in_htgetcharacter = 1;
		    return (char) EOF;
		}
		CTRACE((tfp, "HTFormat: SSL_read error %d\n", status));
		return (char) EOF;	/* -1 is returned by UCX
					   at end of HTTP link */
	    }
	    input_pointer = input_buffer;
	    input_limit = input_buffer + status;
	}
	ch = *input_pointer++;
    } while (ch == (char) 13);	/* Ignore ASCII carriage return */

    return FROMASCII(ch);
}
#endif /* USE_SSL */

/* Match maintype to any MIME type starting with maintype, for example: 
 * image/gif should match image
 */
static int half_match(char *trial_type, char *target)
{
    char *cp = strchr(trial_type, '/');

    /* if no '/' or no '*' */
    if (!cp || *(cp + 1) != '*')
	return 0;

    CTRACE((tfp, "HTFormat: comparing %s and %s for half match\n",
	    trial_type, target));

    /* main type matches */
    if (!strncmp(trial_type, target, (cp - trial_type) - 1))
	return 1;

    return 0;
}

/*
 * Evaluate a deferred mailcap test command, i.e.,. one that substitutes the
 * document's charset or other values in %{name} format.
 */
static BOOL failsMailcap(HTPresentation *pres, HTParentAnchor *anchor)
{
    if (pres->testcommand != 0) {
	if (LYTestMailcapCommand(pres->testcommand,
				 anchor->content_type_params) != 0)
	    return TRUE;
    }
    return FALSE;
}

#define WWW_WILDCARD_REP_OUT HTAtom_for("*")

/*		Look up a presentation
 *		----------------------
 *
 *	If fill_in is NULL, only look for an exact match.
 *	If a wildcard match is made, *fill_in is used to store
 *	a possibly modified presentation, and a pointer to it is
 *	returned.  For an exact match, a pointer to the presentation
 *	in the HTPresentations list is returned.  Returns NULL if
 *	nothing found. - kw
 *
 */
static HTPresentation *HTFindPresentation(HTFormat rep_in,
					  HTFormat rep_out,
					  HTPresentation *fill_in,
					  HTParentAnchor *anchor)
{
    HTAtom *wildcard = NULL;	/* = HTAtom_for("*"); lookup when needed - kw */
    int n;
    int i;
    HTPresentation *pres;
    HTPresentation *match;
    HTPresentation *strong_wildcard_match = 0;
    HTPresentation *weak_wildcard_match = 0;
    HTPresentation *last_default_match = 0;
    HTPresentation *strong_subtype_wildcard_match = 0;

    CTRACE((tfp, "HTFormat: Looking up presentation for %s to %s\n",
	    HTAtom_name(rep_in), HTAtom_name(rep_out)));

    n = HTList_count(HTPresentations);
    for (i = 0; i < n; i++) {
	pres = (HTPresentation *) HTList_objectAt(HTPresentations, i);
	if (pres->rep == rep_in) {
	    if (pres->rep_out == rep_out) {
		if (failsMailcap(pres, anchor))
		    continue;
		CTRACE((tfp, "FindPresentation: found exact match: %s\n",
			HTAtom_name(pres->rep)));
		return pres;

	    } else if (!fill_in) {
		continue;
	    } else {
		if (!wildcard)
		    wildcard = WWW_WILDCARD_REP_OUT;
		if (pres->rep_out == wildcard) {
		    if (failsMailcap(pres, anchor))
			continue;
		    if (!strong_wildcard_match)
			strong_wildcard_match = pres;
		    /* otherwise use the first one */
		    CTRACE((tfp,
			    "StreamStack: found strong wildcard match: %s\n",
			    HTAtom_name(pres->rep)));
		}
	    }

	} else if (!fill_in) {
	    continue;

	} else if (half_match(HTAtom_name(pres->rep),
			      HTAtom_name(rep_in))) {
	    if (pres->rep_out == rep_out) {
		if (failsMailcap(pres, anchor))
		    continue;
		if (!strong_subtype_wildcard_match)
		    strong_subtype_wildcard_match = pres;
		/* otherwise use the first one */
		CTRACE((tfp,
			"StreamStack: found strong subtype wildcard match: %s\n",
			HTAtom_name(pres->rep)));
	    }
	}

	if (pres->rep == WWW_SOURCE) {
	    if (pres->rep_out == rep_out) {
		if (failsMailcap(pres, anchor))
		    continue;
		if (!weak_wildcard_match)
		    weak_wildcard_match = pres;
		/* otherwise use the first one */
		CTRACE((tfp,
			"StreamStack: found weak wildcard match: %s\n",
			HTAtom_name(pres->rep_out)));

	    } else if (!last_default_match) {
		if (!wildcard)
		    wildcard = WWW_WILDCARD_REP_OUT;
		if (pres->rep_out == wildcard) {
		    if (failsMailcap(pres, anchor))
			continue;
		    last_default_match = pres;
		    /* otherwise use the first one */
		}
	    }
	}
    }

    match = (strong_subtype_wildcard_match
	     ? strong_subtype_wildcard_match
	     : (strong_wildcard_match
		? strong_wildcard_match
		: (weak_wildcard_match
		   ? weak_wildcard_match
		   : last_default_match)));

    if (match) {
	*fill_in = *match;	/* Specific instance */
	fill_in->rep = rep_in;	/* yuk */
	fill_in->rep_out = rep_out;	/* yuk */
	return fill_in;
    }

    return NULL;
}

/*		Create a filter stack
 *		---------------------
 *
 *	If a wildcard match is made, a temporary HTPresentation
 *	structure is made to hold the destination format while the
 *	new stack is generated. This is just to pass the out format to
 *	MIME so far.  Storing the format of a stream in the stream might
 *	be a lot neater.
 *
 */
HTStream *HTStreamStack(HTFormat rep_in,
			HTFormat rep_out,
			HTStream *sink,
			HTParentAnchor *anchor)
{
    HTPresentation temp;
    HTPresentation *match;
    HTStream *result;

    CTRACE((tfp, "HTFormat: Constructing stream stack for %s to %s (%s)\n",
	    HTAtom_name(rep_in),
	    HTAtom_name(rep_out),
	    NONNULL(anchor->content_type_params)));

    /* don't return on WWW_SOURCE some people might like
     * to make use of the source!!!!  LJM
     */
#if 0
    if (rep_out == WWW_SOURCE || rep_out == rep_in)
	return sink;		/*  LJM */
#endif

    if (rep_out == rep_in) {
	result = sink;

    } else if ((match = HTFindPresentation(rep_in, rep_out, &temp, anchor))) {
	if (match == &temp) {
	    CTRACE((tfp, "StreamStack: Using %s\n", HTAtom_name(temp.rep_out)));
	} else {
	    CTRACE((tfp, "StreamStack: found exact match: %s\n",
		    HTAtom_name(match->rep)));
	}
	result = (*match->converter) (match, anchor, sink);
    } else {
	result = NULL;
    }
    if (TRACE) {
	if (result && result->isa && result->isa->name) {
	    CTRACE((tfp, "StreamStack: Returning \"%s\"\n", result->isa->name));
	} else if (result) {
	    CTRACE((tfp, "StreamStack: Returning *unknown* stream!\n"));
	} else {
	    CTRACE((tfp, "StreamStack: Returning NULL!\n"));
	    CTRACE_FLUSH(tfp);	/* a crash may be imminent... - kw */
	}
    }
    return result;
}

/*		Put a presentation near start of list
 *		-------------------------------------
 *
 *	Look up a presentation (exact match only) and, if found, reorder
 *	it to the start of the HTPresentations list. - kw
 */
void HTReorderPresentation(HTFormat rep_in,
			   HTFormat rep_out)
{
    HTPresentation *match;

    if ((match = HTFindPresentation(rep_in, rep_out, NULL, NULL))) {
	HTList_removeObject(HTPresentations, match);
	HTList_addObject(HTPresentations, match);
    }
}

/*
 * Setup 'get_accept' flag to denote presentations that are not redundant,
 * and will be listed in "Accept:" header.
 */
void HTFilterPresentations(void)
{
    int i, j;
    int n = HTList_count(HTPresentations);
    HTPresentation *p, *q;
    BOOL matched;
    char *s, *t;

    CTRACE((tfp, "HTFilterPresentations (AcceptMedia %#x)\n", LYAcceptMedia));
    for (i = 0; i < n; i++) {
	p = (HTPresentation *) HTList_objectAt(HTPresentations, i);
	s = HTAtom_name(p->rep);

	p->get_accept = FALSE;
	if ((LYAcceptMedia & p->accept_opt) != 0
	    && p->rep_out == WWW_PRESENT
	    && p->rep != WWW_SOURCE
	    && strcasecomp(s, "www/mime")
	    && strcasecomp(s, "www/compressed")
	    && p->quality <= 1.0 && p->quality >= 0.0) {
	    matched = TRUE;
	    for (j = 0; j < i; j++) {
		q = (HTPresentation *) HTList_objectAt(HTPresentations, j);
		t = HTAtom_name(q->rep);

		if (!strcasecomp(s, t)) {
		    matched = FALSE;
		    CTRACE((tfp, "  match %s %s\n", s, t));
		    break;
		}
	    }
	    p->get_accept = matched;
	}
    }
}

/*		Find the cost of a filter stack
 *		-------------------------------
 *
 *	Must return the cost of the same stack which StreamStack would set up.
 *
 * On entry,
 *	length	The size of the data to be converted
 */
float HTStackValue(HTFormat rep_in,
		   HTFormat rep_out,
		   float initial_value,
		   long int length)
{
    HTAtom *wildcard = WWW_WILDCARD_REP_OUT;

    CTRACE((tfp, "HTFormat: Evaluating stream stack for %s worth %.3f to %s\n",
	    HTAtom_name(rep_in), initial_value, HTAtom_name(rep_out)));

    if (rep_out == WWW_SOURCE || rep_out == rep_in)
	return 0.0;

    {
	int n = HTList_count(HTPresentations);
	int i;
	HTPresentation *pres;

	for (i = 0; i < n; i++) {
	    pres = (HTPresentation *) HTList_objectAt(HTPresentations, i);
	    if (pres->rep == rep_in &&
		(pres->rep_out == rep_out || pres->rep_out == wildcard)) {
		float value = initial_value * pres->quality;

		if (HTMaxSecs != 0.0)
		    value = value - (length * pres->secs_per_byte + pres->secs)
			/ HTMaxSecs;
		return value;
	    }
	}
    }

    return (float) -1e30;	/* Really bad */

}

/*	Display the page while transfer in progress
 *	-------------------------------------------
 *
 *   Repaint the page only when necessary.
 *   This is a traverse call for HText_pageDisplay() - it works!.
 *
 */
void HTDisplayPartial(void)
{
#ifdef DISP_PARTIAL
    if (display_partial) {
	/*
	 * HText_getNumOfLines() = "current" number of complete lines received
	 * NumOfLines_partial = number of lines at the moment of last repaint. 
	 * (we update NumOfLines_partial only when we repaint the display.)
	 *
	 * display_partial could only be enabled in HText_new() so a new
	 * HTMainText object available - all HText_ functions use it, lines
	 * counter HText_getNumOfLines() in particular.
	 *
	 * Otherwise HTMainText holds info from the previous document and we
	 * may repaint it instead of the new one:  prev doc scrolled to the
	 * first line (=Newline_partial) is not good looking :-) 23 Aug 1998
	 * Leonid Pauzner
	 *
	 * So repaint the page only when necessary:
	 */
	int Newline_partial = LYGetNewline();

	if (((Newline_partial + display_lines) - 1 > NumOfLines_partial)
	/* current page not complete... */
	    && (partial_threshold > 0 ?
		((Newline_partial + partial_threshold) - 1 <=
		 HText_getNumOfLines()) :
		((Newline_partial + display_lines) - 1 <= HText_getNumOfLines()))
	/*
	 * Originally we rendered by increments of 2 lines,
	 * but that got annoying on slow network connections.
	 * Then we switched to full-pages.  Now it's configurable.
	 * If partial_threshold <= 0, then it's a full page
	 */
	    ) {
	    if (LYMainLoop_pageDisplay(Newline_partial))
		NumOfLines_partial = HText_getNumOfLines();
	}
    }
#else /* nothing */
#endif /* DISP_PARTIAL */
}

/* Put this as early as possible, OK just after HTDisplayPartial() */
void HTFinishDisplayPartial(void)
{
#ifdef DISP_PARTIAL
    /*
     * End of incremental rendering stage here.
     */
    display_partial = FALSE;
#endif /* DISP_PARTIAL */
}

/*	Push data from a socket down a stream
 *	-------------------------------------
 *
 *   This routine is responsible for creating and PRESENTING any
 *   graphic (or other) objects described by the file.
 *
 *   The file number given is assumed to be a TELNET stream, i.e., containing
 *   CRLF at the end of lines which need to be stripped to LF for unix
 *   when the format is textual.
 *
 *  State of socket and target stream on entry:
 *			socket (file_number) assumed open,
 *			target (sink) assumed valid.
 *
 *  Return values:
 *	HT_INTERRUPTED  Interruption or error after some data received.
 *	-2		Unexpected disconnect before any data received.
 *	-1		Interruption or error before any data received, or
 *			(UNIX) other read error before any data received, or
 *			download cancelled.
 *	HT_LOADED	Normal close of socket (end of file indication
 *			received), or
 *			unexpected disconnect after some data received, or
 *			other read error after some data received, or
 *			(not UNIX) other read error before any data received.
 *
 *  State of socket and target stream on return depends on return value:
 *	HT_INTERRUPTED	socket still open, target aborted.
 *	-2		socket still open, target stream still valid.
 *	-1		socket still open, target aborted.
 *	otherwise	socket closed,	target stream still valid.
 */
int HTCopy(HTParentAnchor *anchor,
	   int file_number,
	   void *handle GCC_UNUSED,
	   HTStream *sink)
{
    HTStreamClass targetClass;
    BOOL suppress_readprogress = NO;
    int bytes;
    int rv = 0;

    /*  Push the data down the stream
     */
    targetClass = *(sink->isa);	/* Copy pointers to procedures */

    /*
     * Push binary from socket down sink
     *
     * This operation could be put into a main event loop
     */
    HTReadProgress(bytes = 0, 0);
    for (;;) {
	int status;

	if (LYCancelDownload) {
	    LYCancelDownload = FALSE;
	    (*targetClass._abort) (sink, NULL);
	    rv = -1;
	    goto finished;
	}

	if (HTCheckForInterrupt()) {
	    _HTProgress(TRANSFER_INTERRUPTED);
	    (*targetClass._abort) (sink, NULL);
	    if (bytes)
		rv = HT_INTERRUPTED;
	    else
		rv = -1;
	    goto finished;
	}
#ifdef USE_SSL
	if (handle)
	    status = SSL_read((SSL *) handle, input_buffer, INPUT_BUFFER_SIZE);
	else
	    status = NETREAD(file_number, input_buffer, INPUT_BUFFER_SIZE);
#else
	status = NETREAD(file_number, input_buffer, INPUT_BUFFER_SIZE);
#endif /* USE_SSL */

	if (status <= 0) {
	    if (status == 0) {
		break;
	    } else if (status == HT_INTERRUPTED) {
		_HTProgress(TRANSFER_INTERRUPTED);
		(*targetClass._abort) (sink, NULL);
		if (bytes)
		    rv = HT_INTERRUPTED;
		else
		    rv = -1;
		goto finished;
	    } else if (SOCKET_ERRNO == ENOTCONN ||
#ifdef _WINDOWS			/* 1997/11/10 (Mon) 16:57:18 */
		       SOCKET_ERRNO == ETIMEDOUT ||
#endif
		       SOCKET_ERRNO == ECONNRESET ||
		       SOCKET_ERRNO == EPIPE) {
		/*
		 * Arrrrgh, HTTP 0/1 compatibility problem, maybe.
		 */
		if (bytes <= 0) {
		    /*
		     * Don't have any data, so let the calling function decide
		     * what to do about it.  - FM
		     */
		    rv = -2;
		    goto finished;
		} else {
#ifdef UNIX
		    /*
		     * Treat what we've received already as the complete
		     * transmission, but not without giving the user an alert. 
		     * I don't know about all the different TCP stacks for VMS
		     * etc., so this is currently only for UNIX.  - kw
		     */
		    HTInetStatus("NETREAD");
		    HTAlert("Unexpected server disconnect.");
		    CTRACE((tfp,
			    "HTCopy: Unexpected server disconnect. Treating as completed.\n"));
		    status = 0;
		    break;
#else /* !UNIX */
		    /*
		     * Treat what we've gotten already as the complete
		     * transmission.  - FM
		     */
		    CTRACE((tfp,
			    "HTCopy: Unexpected server disconnect.  Treating as completed.\n"));
		    status = 0;
		    break;
#endif /* UNIX */
		}
#ifdef UNIX
	    } else {		/* status < 0 and other errno */
		/*
		 * Treat what we've received already as the complete
		 * transmission, but not without giving the user an alert.  I
		 * don't know about all the different TCP stacks for VMS etc.,
		 * so this is currently only for UNIX.  - kw
		 */
		HTInetStatus("NETREAD");
		HTAlert("Unexpected read error.");
		if (bytes) {
		    (void) NETCLOSE(file_number);
		    rv = HT_LOADED;
		} else {
		    (*targetClass._abort) (sink, NULL);
		    rv = -1;
		}
		goto finished;
#endif
	    }
	    break;
	}

	/*
	 * Suppress ReadProgress messages when collecting a redirection
	 * message, at least initially (unless/until anchor->content_type gets
	 * changed, probably by the MIME message parser).  That way messages
	 * put up by the HTTP module or elsewhere can linger in the statusline
	 * for a while.  - kw
	 */
	suppress_readprogress = (anchor && anchor->content_type &&
				 !strcmp(anchor->content_type,
					 "message/x-http-redirection"));
#ifdef NOT_ASCII
	{
	    char *p;

	    for (p = input_buffer; p < input_buffer + status; p++) {
		*p = FROMASCII(*p);
	    }
	}
#endif /* NOT_ASCII */

	(*targetClass.put_block) (sink, input_buffer, status);
	bytes += status;
	if (!suppress_readprogress)
	    HTReadProgress(bytes, anchor ? anchor->content_length : 0);
	HTDisplayPartial();

    }				/* next bufferload */

    _HTProgress(TRANSFER_COMPLETE);
    (void) NETCLOSE(file_number);
    rv = HT_LOADED;

  finished:
    HTFinishDisplayPartial();
    return (rv);
}

/*	Push data from a file pointer down a stream
 *	-------------------------------------
 *
 *   This routine is responsible for creating and PRESENTING any
 *   graphic (or other) objects described by the file.
 *
 *
 *  State of file and target stream on entry:
 *			FILE* (fp) assumed open,
 *			target (sink) assumed valid.
 *
 *  Return values:
 *	HT_INTERRUPTED  Interruption after some data read.
 *	HT_PARTIAL_CONTENT	Error after some data read.
 *	-1		Error before any data read.
 *	HT_LOADED	Normal end of file indication on reading.
 *
 *  State of file and target stream on return:
 *	always		fp still open, target stream still valid.
 */
int HTFileCopy(FILE *fp, HTStream *sink)
{
    HTStreamClass targetClass;
    int status, bytes;
    int rv = HT_OK;

    /*  Push the data down the stream
     */
    targetClass = *(sink->isa);	/* Copy pointers to procedures */

    /*  Push binary from socket down sink
     */
    HTReadProgress(bytes = 0, 0);
    for (;;) {
	status = fread(input_buffer, 1, INPUT_BUFFER_SIZE, fp);
	if (status == 0) {	/* EOF or error */
	    if (ferror(fp) == 0) {
		rv = HT_LOADED;
		break;
	    }
	    CTRACE((tfp, "HTFormat: Read error, read returns %d\n",
		    ferror(fp)));
	    if (bytes) {
		rv = HT_PARTIAL_CONTENT;
	    } else {
		rv = -1;
	    }
	    break;
	}

	(*targetClass.put_block) (sink, input_buffer, status);
	bytes += status;
	HTReadProgress(bytes, 0);
	/* Suppress last screen update in partial mode - a regular update under
	 * control of mainloop() should follow anyway.  - kw
	 */
#ifdef DISP_PARTIAL
	if (display_partial && bytes != HTMainAnchor->content_length)
	    HTDisplayPartial();
#endif

	if (HTCheckForInterrupt()) {
	    _HTProgress(TRANSFER_INTERRUPTED);
	    if (bytes) {
		rv = HT_INTERRUPTED;
	    } else {
		rv = -1;
	    }
	    break;
	}
    }				/* next bufferload */

    HTFinishDisplayPartial();
    return rv;
}

#ifdef USE_SOURCE_CACHE
/*	Push data from an HTChunk down a stream
 *	---------------------------------------
 *
 *   This routine is responsible for creating and PRESENTING any
 *   graphic (or other) objects described by the file.
 *
 *  State of memory and target stream on entry:
 *			HTChunk* (chunk) and target (sink) assumed valid.
 *
 *  Return values:
 *	HT_LOADED	All data sent.
 *	HT_INTERRUPTED  Interruption after some data read.
 *
 *  State of memory and target stream on return:
 *	always		chunk unchanged, target stream still valid.
 */
int HTMemCopy(HTChunk *chunk, HTStream *sink)
{
    HTStreamClass targetClass;
    int bytes = 0;
    int rv = HT_OK;

    targetClass = *(sink->isa);
    HTReadProgress(0, 0);
    for (; chunk != NULL; chunk = chunk->next) {

	/* Push the data down the stream a piece at a time, in case we're
	 * running a large document on a slow machine.
	 */
	(*targetClass.put_block) (sink, chunk->data, chunk->size);
	bytes += chunk->size;

	HTReadProgress(bytes, 0);
	HTDisplayPartial();

	if (HTCheckForInterrupt()) {
	    _HTProgress(TRANSFER_INTERRUPTED);
	    if (bytes) {
		rv = HT_INTERRUPTED;
	    } else {
		rv = -1;
	    }
	    break;
	}
    }

    HTFinishDisplayPartial();
    return rv;
}
#endif

#ifdef USE_ZLIB
/*	Push data from a gzip file pointer down a stream
 *	-------------------------------------
 *
 *   This routine is responsible for creating and PRESENTING any
 *   graphic (or other) objects described by the file.
 *
 *
 *  State of file and target stream on entry:
 *		      gzFile (gzfp) assumed open (should have gzipped content),
 *		      target (sink) assumed valid.
 *
 *  Return values:
 *	HT_INTERRUPTED  Interruption after some data read.
 *	HT_PARTIAL_CONTENT	Error after some data read.
 *	-1		Error before any data read.
 *	HT_LOADED	Normal end of file indication on reading.
 *
 *  State of file and target stream on return:
 *	always		gzfp still open, target stream still valid.
 */
static int HTGzFileCopy(gzFile gzfp, HTStream *sink)
{
    HTStreamClass targetClass;
    int status, bytes;
    int gzerrnum;
    int rv = HT_OK;

    /*  Push the data down the stream
     */
    targetClass = *(sink->isa);	/* Copy pointers to procedures */

    /*  read and inflate gzip'd file, and push binary down sink
     */
    HTReadProgress(bytes = 0, 0);
    for (;;) {
	status = gzread(gzfp, input_buffer, INPUT_BUFFER_SIZE);
	if (status <= 0) {	/* EOF or error */
	    if (status == 0) {
		rv = HT_LOADED;
		break;
	    }
	    CTRACE((tfp, "HTGzFileCopy: Read error, gzread returns %d\n",
		    status));
	    CTRACE((tfp, "gzerror   : %s\n",
		    gzerror(gzfp, &gzerrnum)));
	    if (TRACE) {
		if (gzerrnum == Z_ERRNO)
		    perror("gzerror   ");
	    }
	    if (bytes) {
		rv = HT_PARTIAL_CONTENT;
	    } else {
		rv = -1;
	    }
	    break;
	}

	(*targetClass.put_block) (sink, input_buffer, status);
	bytes += status;
	HTReadProgress(bytes, -1);
	HTDisplayPartial();

	if (HTCheckForInterrupt()) {
	    _HTProgress(TRANSFER_INTERRUPTED);
	    if (bytes) {
		rv = HT_INTERRUPTED;
	    } else {
		rv = -1;
	    }
	    break;
	}
    }				/* next bufferload */

    HTFinishDisplayPartial();
    return rv;
}

#ifndef HAVE_ZERROR
#define zError(s) LynxZError(s)
static const char *zError(int status)
{
    static char result[80];
    sprintf(result, "zlib error %d", status);
    return result;
}
#endif

/*	Push data from a deflate file pointer down a stream
 *	-------------------------------------
 *
 *  This routine is responsible for creating and PRESENTING any
 *  graphic (or other) objects described by the file.  The code is
 *  loosely based on the inflate.c file from w3m.
 *
 *
 *  State of file and target stream on entry:
 *		      FILE (zzfp) assumed open (should have deflated content),
 *		      target (sink) assumed valid.
 *
 *  Return values:
 *	HT_INTERRUPTED  Interruption after some data read.
 *	HT_PARTIAL_CONTENT	Error after some data read.
 *	-1		Error before any data read.
 *	HT_LOADED	Normal end of file indication on reading.
 *
 *  State of file and target stream on return:
 *	always		zzfp still open, target stream still valid.
 */
static int HTZzFileCopy(FILE *zzfp, HTStream *sink)
{
    static char dummy_head[1 + 1] =
    {
	0x8 + 0x7 * 0x10,
	(((0x8 + 0x7 * 0x10) * 0x100 + 30) / 31 * 31) & 0xFF,
    };

    z_stream s;
    HTStreamClass targetClass;
    int bytes;
    int rv = HT_OK;
    char output_buffer[INPUT_BUFFER_SIZE];
    int status;
    int flush;
    int retry = 0;
    int len = 0;

    /*  Push the data down the stream
     */
    targetClass = *(sink->isa);	/* Copy pointers to procedures */

    s.zalloc = Z_NULL;
    s.zfree = Z_NULL;
    s.opaque = Z_NULL;
    status = inflateInit(&s);
    if (status != Z_OK) {
	CTRACE((tfp, "HTZzFileCopy inflateInit() %s\n", zError(status)));
	exit_immediately(1);
    }
    s.avail_in = 0;
    s.next_out = (Bytef *) output_buffer;
    s.avail_out = sizeof(output_buffer);
    flush = Z_NO_FLUSH;

    /*  read and inflate deflate'd file, and push binary down sink
     */
    HTReadProgress(bytes = 0, 0);
    for (;;) {
	if (s.avail_in == 0) {
	    s.next_in = (Bytef *) input_buffer;
	    len = s.avail_in = fread(input_buffer, 1, INPUT_BUFFER_SIZE, zzfp);
	}
	status = inflate(&s, flush);
	if (status == Z_STREAM_END || status == Z_BUF_ERROR) {
	    len = sizeof(output_buffer) - s.avail_out;
	    if (len > 0) {
		(*targetClass.put_block) (sink, output_buffer, len);
		bytes += len;
		HTReadProgress(bytes, -1);
		HTDisplayPartial();
	    }
	    rv = HT_LOADED;
	    break;
	} else if (status == Z_DATA_ERROR && !retry++) {
	    status = inflateReset(&s);
	    if (status != Z_OK) {
		CTRACE((tfp, "HTZzFileCopy inflateReset() %s\n", zError(status)));
		rv = bytes ? HT_PARTIAL_CONTENT : -1;
		break;
	    }
	    s.next_in = (Bytef *) dummy_head;
	    s.avail_in = sizeof(dummy_head);
	    status = inflate(&s, flush);
	    s.next_in = (Bytef *) input_buffer;
	    s.avail_in = len;
	    continue;
	} else if (status != Z_OK) {
	    CTRACE((tfp, "HTZzFileCopy inflate() %s\n", zError(status)));
	    rv = bytes ? HT_PARTIAL_CONTENT : -1;
	    break;
	} else if (s.avail_out == 0) {
	    len = sizeof(output_buffer);
	    s.next_out = (Bytef *) output_buffer;
	    s.avail_out = sizeof(output_buffer);

	    (*targetClass.put_block) (sink, output_buffer, len);
	    bytes += len;
	    HTReadProgress(bytes, -1);
	    HTDisplayPartial();

	    if (HTCheckForInterrupt()) {
		_HTProgress(TRANSFER_INTERRUPTED);
		rv = bytes ? HT_INTERRUPTED : -1;
		break;
	    }
	}
	retry = 1;
    }				/* next bufferload */

    inflateEnd(&s);
    HTFinishDisplayPartial();
    return rv;
}
#endif /* USE_ZLIB */

#ifdef USE_BZLIB
/*	Push data from a bzip file pointer down a stream
 *	-------------------------------------
 *
 *   This routine is responsible for creating and PRESENTING any
 *   graphic (or other) objects described by the file.
 *
 *
 *  State of file and target stream on entry:
 *		      BZFILE (bzfp) assumed open (should have bzipped content),
 *		      target (sink) assumed valid.
 *
 *  Return values:
 *	HT_INTERRUPTED  Interruption after some data read.
 *	HT_PARTIAL_CONTENT	Error after some data read.
 *	-1		Error before any data read.
 *	HT_LOADED	Normal end of file indication on reading.
 *
 *  State of file and target stream on return:
 *	always		bzfp still open, target stream still valid.
 */
static int HTBzFileCopy(BZFILE * bzfp, HTStream *sink)
{
    HTStreamClass targetClass;
    int status, bytes;
    int bzerrnum;
    int rv = HT_OK;

    /*  Push the data down the stream
     */
    targetClass = *(sink->isa);	/* Copy pointers to procedures */

    /*  read and inflate bzip'd file, and push binary down sink
     */
    HTReadProgress(bytes = 0, 0);
    for (;;) {
	status = BZ2_bzread(bzfp, input_buffer, INPUT_BUFFER_SIZE);
	if (status <= 0) {	/* EOF or error */
	    if (status == 0) {
		rv = HT_LOADED;
		break;
	    }
	    CTRACE((tfp, "HTBzFileCopy: Read error, bzread returns %d\n",
		    status));
	    CTRACE((tfp, "bzerror   : %s\n",
		    BZ2_bzerror(bzfp, &bzerrnum)));
	    if (bytes) {
		rv = HT_PARTIAL_CONTENT;
	    } else {
		rv = -1;
	    }
	    break;
	}

	(*targetClass.put_block) (sink, input_buffer, status);
	bytes += status;
	HTReadProgress(bytes, -1);
	HTDisplayPartial();

	if (HTCheckForInterrupt()) {
	    _HTProgress(TRANSFER_INTERRUPTED);
	    if (bytes) {
		rv = HT_INTERRUPTED;
	    } else {
		rv = -1;
	    }
	    break;
	}
    }				/* next bufferload */

    HTFinishDisplayPartial();
    return rv;
}
#endif /* USE_BZLIB */

/*	Push data from a socket down a stream STRIPPING CR
 *	--------------------------------------------------
 *
 *   This routine is responsible for creating and PRESENTING any
 *   graphic (or other) objects described by the socket.
 *
 *   The file number given is assumed to be a TELNET stream ie containing
 *   CRLF at the end of lines which need to be stripped to LF for unix
 *   when the format is textual.
 *
 */
void HTCopyNoCR(HTParentAnchor *anchor GCC_UNUSED,
		int file_number,
		HTStream *sink)
{
    HTStreamClass targetClass;
    int character;

    /*  Push the data, ignoring CRLF, down the stream
     */
    targetClass = *(sink->isa);	/* Copy pointers to procedures */

    /*
     * Push text from telnet socket down sink
     *
     * @@@@@ To push strings could be faster?  (especially is we cheat and
     * don't ignore CR!  :-}
     */
    HTInitInput(file_number);
    for (;;) {
	character = HTGetCharacter();
	if (character == EOF)
	    break;
	(*targetClass.put_character) (sink, UCH(character));
    }
}

/*	Parse a socket given format and file number
 *
 *   This routine is responsible for creating and PRESENTING any
 *   graphic (or other) objects described by the file.
 *
 *   The file number given is assumed to be a TELNET stream ie containing
 *   CRLF at the end of lines which need to be stripped to LF for unix
 *   when the format is textual.
 *
 *  State of socket and target stream on entry:
 *			socket (file_number) assumed open,
 *			target (sink) usually NULL (will call stream stack).
 *
 *  Return values:
 *	HT_INTERRUPTED  Interruption or error after some data received.
 *	-501		Stream stack failed (cannot present or convert).
 *	-2		Unexpected disconnect before any data received.
 *	-1		Stream stack failed (cannot present or convert), or
 *			Interruption or error before any data received, or
 *			(UNIX) other read error before any data received, or
 *			download cancelled.
 *	HT_LOADED	Normal close of socket (end of file indication
 *			received), or
 *			unexpected disconnect after some data received, or
 *			other read error after some data received, or
 *			(not UNIX) other read error before any data received.
 *
 *  State of socket and target stream on return depends on return value:
 *	HT_INTERRUPTED	socket still open, target aborted.
 *	-501		socket still open, target stream NULL.
 *	-2		socket still open, target freed.
 *	-1		socket still open, target stream aborted or NULL.
 *	otherwise	socket closed,	target stream freed.
 */
int HTParseSocket(HTFormat rep_in,
		  HTFormat format_out,
		  HTParentAnchor *anchor,
		  int file_number,
		  HTStream *sink)
{
    HTStream *stream;
    HTStreamClass targetClass;
    int rv;

    stream = HTStreamStack(rep_in, format_out, sink, anchor);

    if (!stream) {
	char *buffer = 0;

	if (LYCancelDownload) {
	    LYCancelDownload = FALSE;
	    return -1;
	}
	HTSprintf0(&buffer, CANNOT_CONVERT_I_TO_O,
		   HTAtom_name(rep_in), HTAtom_name(format_out));
	CTRACE((tfp, "HTFormat: %s\n", buffer));
	rv = HTLoadError(sink, 501, buffer);	/* returns -501 */
	FREE(buffer);
    } else {
	/*
	 * Push the data, don't worry about CRLF we can strip them later.
	 */
	targetClass = *(stream->isa);	/* Copy pointers to procedures */
	rv = HTCopy(anchor, file_number, NULL, stream);
	if (rv != -1 && rv != HT_INTERRUPTED)
	    (*targetClass._free) (stream);
    }
    return rv;
    /* Originally:  full: HT_LOADED;  partial: HT_INTERRUPTED;  no bytes: -1 */
}

/*	Parse a file given format and file pointer
 *
 *   This routine is responsible for creating and PRESENTING any
 *   graphic (or other) objects described by the file.
 *
 *   The file number given is assumed to be a TELNET stream ie containing
 *   CRLF at the end of lines which need to be stripped to \n for unix
 *   when the format is textual.
 *
 *  State of file and target stream on entry:
 *			FILE* (fp) assumed open,
 *			target (sink) usually NULL (will call stream stack).
 *
 *  Return values:
 *	-501		Stream stack failed (cannot present or convert).
 *	-1		Download cancelled.
 *	HT_NO_DATA	Error before any data read.
 *	HT_PARTIAL_CONTENT	Interruption or error after some data read.
 *	HT_LOADED	Normal end of file indication on reading.
 *
 *  State of file and target stream on return:
 *	always		fp still open; target freed, aborted, or NULL.
 */
int HTParseFile(HTFormat rep_in,
		HTFormat format_out,
		HTParentAnchor *anchor,
		FILE *fp,
		HTStream *sink)
{
    HTStream *stream;
    HTStreamClass targetClass;
    int rv;

#ifdef SH_EX			/* 1998/01/04 (Sun) 16:04:09 */
    if (fp == NULL)
	return HT_LOADED;
#endif

    stream = HTStreamStack(rep_in, format_out, sink, anchor);

    if (!stream) {
	char *buffer = 0;

	if (LYCancelDownload) {
	    LYCancelDownload = FALSE;
	    return -1;
	}
	HTSprintf0(&buffer, CANNOT_CONVERT_I_TO_O,
		   HTAtom_name(rep_in), HTAtom_name(format_out));
	CTRACE((tfp, "HTFormat(in HTParseFile): %s\n", buffer));
	rv = HTLoadError(sink, 501, buffer);
	FREE(buffer);
	return rv;
    }

    /*
     * Push the data down the stream
     *
     * @@ Bug:  This decision ought to be made based on "encoding" rather than
     * on content-type.  @@@ When we handle encoding.  The current method
     * smells anyway.
     */
    targetClass = *(stream->isa);	/* Copy pointers to procedures */
    rv = HTFileCopy(fp, stream);
    if (rv == -1 || rv == HT_INTERRUPTED) {
	(*targetClass._abort) (stream, NULL);
    } else {
	(*targetClass._free) (stream);
    }

    if (rv == -1)
	return HT_NO_DATA;
    else if (rv == HT_INTERRUPTED || (rv > 0 && rv != HT_LOADED))
	return HT_PARTIAL_CONTENT;
    else
	return HT_LOADED;
}

#ifdef USE_SOURCE_CACHE
/*	Parse a document in memory given format and memory block pointer
 *
 *   This routine is responsible for creating and PRESENTING any
 *   graphic (or other) objects described by the file.
 *
 *  State of memory and target stream on entry:
 *			HTChunk* (chunk) assumed valid,
 *			target (sink) usually NULL (will call stream stack).
 *
 *  Return values:
 *	-501		Stream stack failed (cannot present or convert).
 *	HT_LOADED	All data sent.
 *
 *  State of memory and target stream on return:
 *	always		chunk unchanged; target freed, aborted, or NULL.
 */
int HTParseMem(HTFormat rep_in,
	       HTFormat format_out,
	       HTParentAnchor *anchor,
	       HTChunk *chunk,
	       HTStream *sink)
{
    HTStream *stream;
    HTStreamClass targetClass;
    int rv;

    stream = HTStreamStack(rep_in, format_out, sink, anchor);
    if (!stream) {
	char *buffer = 0;

	HTSprintf0(&buffer, CANNOT_CONVERT_I_TO_O,
		   HTAtom_name(rep_in), HTAtom_name(format_out));
	CTRACE((tfp, "HTFormat(in HTParseMem): %s\n", buffer));
	rv = HTLoadError(sink, 501, buffer);
	FREE(buffer);
	return rv;
    }

    /* Push the data down the stream
     */
    targetClass = *(stream->isa);
    rv = HTMemCopy(chunk, stream);
    (*targetClass._free) (stream);
    return HT_LOADED;
}
#endif

#ifdef USE_ZLIB
static int HTCloseGzFile(gzFile gzfp)
{
    int gzres;

    if (gzfp == NULL)
	return 0;
    gzres = gzclose(gzfp);
    if (TRACE) {
	if (gzres == Z_ERRNO) {
	    perror("gzclose   ");
	} else if (gzres != Z_OK) {
	    CTRACE((tfp, "gzclose   : error number %d\n", gzres));
	}
    }
    return (gzres);
}

/*	HTParseGzFile
 *
 *  State of file and target stream on entry:
 *			gzFile (gzfp) assumed open,
 *			target (sink) usually NULL (will call stream stack).
 *
 *  Return values:
 *	-501		Stream stack failed (cannot present or convert).
 *	-1		Download cancelled.
 *	HT_NO_DATA	Error before any data read.
 *	HT_PARTIAL_CONTENT	Interruption or error after some data read.
 *	HT_LOADED	Normal end of file indication on reading.
 *
 *  State of file and target stream on return:
 *	always		gzfp closed; target freed, aborted, or NULL.
 */
int HTParseGzFile(HTFormat rep_in,
		  HTFormat format_out,
		  HTParentAnchor *anchor,
		  gzFile gzfp,
		  HTStream *sink)
{
    HTStream *stream;
    HTStreamClass targetClass;
    int rv;

    stream = HTStreamStack(rep_in, format_out, sink, anchor);

    if (!stream) {
	char *buffer = 0;

	HTCloseGzFile(gzfp);
	if (LYCancelDownload) {
	    LYCancelDownload = FALSE;
	    return -1;
	}
	HTSprintf0(&buffer, CANNOT_CONVERT_I_TO_O,
		   HTAtom_name(rep_in), HTAtom_name(format_out));
	CTRACE((tfp, "HTFormat(in HTParseGzFile): %s\n", buffer));
	rv = HTLoadError(sink, 501, buffer);
	FREE(buffer);
	return rv;
    }

    /*
     * Push the data down the stream
     *
     * @@ Bug:  This decision ought to be made based on "encoding" rather than
     * on content-type.  @@@ When we handle encoding.  The current method
     * smells anyway.
     */
    targetClass = *(stream->isa);	/* Copy pointers to procedures */
    rv = HTGzFileCopy(gzfp, stream);
    if (rv == -1 || rv == HT_INTERRUPTED) {
	(*targetClass._abort) (stream, NULL);
    } else {
	(*targetClass._free) (stream);
    }

    HTCloseGzFile(gzfp);
    if (rv == -1)
	return HT_NO_DATA;
    else if (rv == HT_INTERRUPTED || (rv > 0 && rv != HT_LOADED))
	return HT_PARTIAL_CONTENT;
    else
	return HT_LOADED;
}

/*	HTParseZzFile
 *
 *  State of file and target stream on entry:
 *			FILE (zzfp) assumed open,
 *			target (sink) usually NULL (will call stream stack).
 *
 *  Return values:
 *	-501		Stream stack failed (cannot present or convert).
 *	-1		Download cancelled.
 *	HT_NO_DATA	Error before any data read.
 *	HT_PARTIAL_CONTENT	Interruption or error after some data read.
 *	HT_LOADED	Normal end of file indication on reading.
 *
 *  State of file and target stream on return:
 *	always		zzfp closed; target freed, aborted, or NULL.
 */
int HTParseZzFile(HTFormat rep_in,
		  HTFormat format_out,
		  HTParentAnchor *anchor,
		  FILE *zzfp,
		  HTStream *sink)
{
    HTStream *stream;
    HTStreamClass targetClass;
    int rv;

    stream = HTStreamStack(rep_in, format_out, sink, anchor);

    if (!stream) {
	char *buffer = 0;

	fclose(zzfp);
	if (LYCancelDownload) {
	    LYCancelDownload = FALSE;
	    return -1;
	}
	HTSprintf0(&buffer, CANNOT_CONVERT_I_TO_O,
		   HTAtom_name(rep_in), HTAtom_name(format_out));
	CTRACE((tfp, "HTFormat(in HTParseGzFile): %s\n", buffer));
	rv = HTLoadError(sink, 501, buffer);
	FREE(buffer);
	return rv;
    }

    /*
     * Push the data down the stream
     *
     * @@ Bug:  This decision ought to be made based on "encoding" rather than
     * on content-type.  @@@ When we handle encoding.  The current method
     * smells anyway.
     */
    targetClass = *(stream->isa);	/* Copy pointers to procedures */
    rv = HTZzFileCopy(zzfp, stream);
    if (rv == -1 || rv == HT_INTERRUPTED) {
	(*targetClass._abort) (stream, NULL);
    } else {
	(*targetClass._free) (stream);
    }

    fclose(zzfp);
    if (rv == -1)
	return HT_NO_DATA;
    else if (rv == HT_INTERRUPTED || (rv > 0 && rv != HT_LOADED))
	return HT_PARTIAL_CONTENT;
    else
	return HT_LOADED;
}
#endif /* USE_ZLIB */

#ifdef USE_BZLIB
static void HTCloseBzFile(BZFILE * bzfp)
{
    if (bzfp)
	BZ2_bzclose(bzfp);
}

/*	HTParseBzFile
 *
 *  State of file and target stream on entry:
 *			bzFile (bzfp) assumed open,
 *			target (sink) usually NULL (will call stream stack).
 *
 *  Return values:
 *	-501		Stream stack failed (cannot present or convert).
 *	-1		Download cancelled.
 *	HT_NO_DATA	Error before any data read.
 *	HT_PARTIAL_CONTENT	Interruption or error after some data read.
 *	HT_LOADED	Normal end of file indication on reading.
 *
 *  State of file and target stream on return:
 *	always		bzfp closed; target freed, aborted, or NULL.
 */
int HTParseBzFile(HTFormat rep_in,
		  HTFormat format_out,
		  HTParentAnchor *anchor,
		  BZFILE * bzfp,
		  HTStream *sink)
{
    HTStream *stream;
    HTStreamClass targetClass;
    int rv;

    stream = HTStreamStack(rep_in, format_out, sink, anchor);

    if (!stream) {
	char *buffer = 0;

	HTCloseBzFile(bzfp);
	if (LYCancelDownload) {
	    LYCancelDownload = FALSE;
	    return -1;
	}
	HTSprintf0(&buffer, CANNOT_CONVERT_I_TO_O,
		   HTAtom_name(rep_in), HTAtom_name(format_out));
	CTRACE((tfp, "HTFormat(in HTParseBzFile): %s\n", buffer));
	rv = HTLoadError(sink, 501, buffer);
	FREE(buffer);
	return rv;
    }

    /*
     * Push the data down the stream
     *
     * @@ Bug:  This decision ought to be made based on "encoding" rather than
     * on content-type.  @@@ When we handle encoding.  The current method
     * smells anyway.
     */
    targetClass = *(stream->isa);	/* Copy pointers to procedures */
    rv = HTBzFileCopy(bzfp, stream);
    if (rv == -1 || rv == HT_INTERRUPTED) {
	(*targetClass._abort) (stream, NULL);
    } else {
	(*targetClass._free) (stream);
    }

    HTCloseBzFile(bzfp);
    if (rv == -1)
	return HT_NO_DATA;
    else if (rv == HT_INTERRUPTED || (rv > 0 && rv != HT_LOADED))
	return HT_PARTIAL_CONTENT;
    else
	return HT_LOADED;
}
#endif /* USE_BZLIB */

/*	Converter stream: Network Telnet to internal character text
 *	-----------------------------------------------------------
 *
 *	The input is assumed to be in ASCII, with lines delimited
 *	by (13,10) pairs, These pairs are converted into (CR,LF)
 *	pairs in the local representation.  The (CR,LF) sequence
 *	when found is changed to a '\n' character, the internal
 *	C representation of a new line.
 */

static void NetToText_put_character(HTStream *me, char net_char)
{
    char c = FROMASCII(net_char);

    if (me->had_cr) {
	if (c == LF) {
	    me->sink->isa->put_character(me->sink, '\n');	/* Newline */
	    me->had_cr = NO;
	    return;
	} else {
	    me->sink->isa->put_character(me->sink, CR);		/* leftover */
	}
    }
    me->had_cr = (BOOL) (c == CR);
    if (!me->had_cr)
	me->sink->isa->put_character(me->sink, c);	/* normal */
}

static void NetToText_put_string(HTStream *me, const char *s)
{
    const char *p;

    for (p = s; *p; p++)
	NetToText_put_character(me, *p);
}

static void NetToText_put_block(HTStream *me, const char *s, int l)
{
    const char *p;

    for (p = s; p < (s + l); p++)
	NetToText_put_character(me, *p);
}

static void NetToText_free(HTStream *me)
{
    (me->sink->isa->_free) (me->sink);	/* Close rest of pipe */
    FREE(me);
}

static void NetToText_abort(HTStream *me, HTError e)
{
    me->sink->isa->_abort(me->sink, e);		/* Abort rest of pipe */
    FREE(me);
}

/*	The class structure
*/
static HTStreamClass NetToTextClass =
{
    "NetToText",
    NetToText_free,
    NetToText_abort,
    NetToText_put_character,
    NetToText_put_string,
    NetToText_put_block
};

/*	The creation method
*/
HTStream *HTNetToText(HTStream *sink)
{
    HTStream *me = typecalloc(HTStream);

    if (me == NULL)
	outofmem(__FILE__, "NetToText");
    me->isa = &NetToTextClass;

    me->had_cr = NO;
    me->sink = sink;
    return me;
}

static HTStream HTBaseStreamInstance;	/* Made static */

/*
 *	ERROR STREAM
 *	------------
 *	There is only one error stream shared by anyone who wants a
 *	generic error returned from all stream methods.
 */
static void HTErrorStream_put_character(HTStream *me GCC_UNUSED, char c GCC_UNUSED)
{
    LYCancelDownload = TRUE;
}

static void HTErrorStream_put_string(HTStream *me GCC_UNUSED, const char *s)
{
    if (s && *s)
	LYCancelDownload = TRUE;
}

static void HTErrorStream_write(HTStream *me GCC_UNUSED, const char *s, int l)
{
    if (l && s)
	LYCancelDownload = TRUE;
}

static void HTErrorStream_free(HTStream *me GCC_UNUSED)
{
    return;
}

static void HTErrorStream_abort(HTStream *me GCC_UNUSED, HTError e GCC_UNUSED)
{
    return;
}

static const HTStreamClass HTErrorStreamClass =
{
    "ErrorStream",
    HTErrorStream_free,
    HTErrorStream_abort,
    HTErrorStream_put_character,
    HTErrorStream_put_string,
    HTErrorStream_write
};

HTStream *HTErrorStream(void)
{
    CTRACE((tfp, "ErrorStream. Created\n"));
    HTBaseStreamInstance.isa = &HTErrorStreamClass;	/* The rest is random */
    return &HTBaseStreamInstance;
}
