/*		Manage different file formats			HTFormat.c
**		=============================
**
** Bugs:
**	Not reentrant.
**
**	Assumes the incoming stream is ASCII, rather than a local file
**	format, and so ALWAYS converts from ASCII on non-ASCII machines.
**	Therefore, non-ASCII machines can't read local files.
**
*/

#include <HTUtils.h>

/* Implements:
*/
#include <HTFormat.h>

PUBLIC float HTMaxSecs = 1e10;		/* No effective limit */
PUBLIC float HTMaxLength = 1e10;	/* No effective limit */
PUBLIC long int HTMaxBytes  = 0;	/* No effective limit */

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

PUBLIC	BOOL HTOutputSource = NO;	/* Flag: shortcut parser to stdout */

#ifdef ORIGINAL
struct _HTStream {
      CONST HTStreamClass*	isa;
      /* ... */
};
#endif /* ORIGINAL */

/* this version used by the NetToText stream */
struct _HTStream {
	CONST HTStreamClass *	isa;
	BOOL			had_cr;
	HTStream *		sink;
};

/*	Presentation methods
**	--------------------
*/
PUBLIC	HTList * HTPresentations = NULL;
PUBLIC	HTPresentation * default_presentation = NULL;

/*
 *	To free off the presentation list.
 */
#ifdef LY_FIND_LEAKS
PRIVATE void HTFreePresentations NOPARAMS;
#endif

/*	Define a presentation system command for a content-type
**	-------------------------------------------------------
*/
PUBLIC void HTSetPresentation ARGS6(
	CONST char *,	representation,
	CONST char *,	command,
	double,		quality,
	double,		secs,
	double,		secs_per_byte,
	long int,	maxbytes)
{
    HTPresentation * pres = typecalloc(HTPresentation);
    if (pres == NULL)
	outofmem(__FILE__, "HTSetPresentation");

    pres->rep = HTAtom_for(representation);
    pres->rep_out = WWW_PRESENT;		/* Fixed for now ... :-) */
    pres->converter = HTSaveAndExecute;		/* Fixed for now ...	 */
    pres->quality = (float) quality;
    pres->secs = (float) secs;
    pres->secs_per_byte = (float) secs_per_byte;
    pres->maxbytes = maxbytes;
    pres->command = NULL;
    StrAllocCopy(pres->command, command);

    /*
     *	Memory leak fixed.
     *	05-28-94 Lynx 2-3-1 Garrett Arch Blythe
     */
    if (!HTPresentations)	{
	HTPresentations = HTList_new();
#ifdef LY_FIND_LEAKS
	atexit(HTFreePresentations);
#endif
    }

    if (strcmp(representation, "*")==0) {
	FREE(default_presentation);
	default_presentation = pres;
    } else {
	HTList_addObject(HTPresentations, pres);
    }
}

/*	Define a built-in function for a content-type
**	---------------------------------------------
*/
PUBLIC void HTSetConversion ARGS7(
	CONST char *,	representation_in,
	CONST char *,	representation_out,
	HTConverter*,	converter,
	float,		quality,
	float,		secs,
	float,		secs_per_byte,
	long int,	maxbytes)
{
    HTPresentation * pres = typecalloc(HTPresentation);
    if (pres == NULL)
	outofmem(__FILE__, "HTSetConversion");

    pres->rep = HTAtom_for(representation_in);
    pres->rep_out = HTAtom_for(representation_out);
    pres->converter = converter;
    pres->command = NULL;		/* Fixed */
    pres->quality = quality;
    pres->secs = secs;
    pres->secs_per_byte = secs_per_byte;
    pres->maxbytes = maxbytes;
    pres->command = NULL;

    /*
     *	Memory Leak fixed.
     *	05-28-94 Lynx 2-3-1 Garrett Arch Blythe
     */
    if (!HTPresentations)	{
	HTPresentations = HTList_new();
#ifdef LY_FIND_LEAKS
	atexit(HTFreePresentations);
#endif
    }

    HTList_addObject(HTPresentations, pres);
}

#ifdef LY_FIND_LEAKS
/*
**	Purpose:	Free the presentation list.
**	Arguments:	void
**	Return Value:	void
**	Remarks/Portability/Dependencies/Restrictions:
**		Made to clean up Lynx's bad leakage.
**	Revision History:
**		05-28-94	created Lynx 2-3-1 Garrett Arch Blythe
*/
PRIVATE void HTFreePresentations NOARGS
{
    HTPresentation * pres = NULL;

    /*
     *	Loop through the list.
     */
    while (!HTList_isEmpty(HTPresentations)) {
	/*
	 *  Free off each item.
	 *  May also need to free off it's items, but not sure
	 *  as of yet.
	 */
	pres = (HTPresentation *)HTList_removeLastObject(HTPresentations);
	FREE(pres->command);
	FREE(pres);
    }
    /*
     *	Free the list itself.
     */
    HTList_delete(HTPresentations);
    HTPresentations = NULL;
}
#endif /* LY_FIND_LEAKS */

/*	File buffering
**	--------------
**
**	The input file is read using the macro which can read from
**	a socket or a file.
**	The input buffer size, if large will give greater efficiency and
**	release the server faster, and if small will save space on PCs etc.
*/
#define INPUT_BUFFER_SIZE 4096		/* Tradeoff */
PRIVATE char input_buffer[INPUT_BUFFER_SIZE];
PRIVATE char * input_pointer;
PRIVATE char * input_limit;
PRIVATE int input_file_number;

/*	Set up the buffering
**
**	These routines are public because they are in fact needed by
**	many parsers, and on PCs and Macs we should not duplicate
**	the static buffer area.
*/
PUBLIC void HTInitInput ARGS1 (int,file_number)
{
    input_file_number = file_number;
    input_pointer = input_limit = input_buffer;
}

PUBLIC int interrupted_in_htgetcharacter = 0;
PUBLIC int HTGetCharacter NOARGS
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
		return EOF; /* -1 is returned by UCX
			       at end of HTTP link */
	    }
	    input_pointer = input_buffer;
	    input_limit = input_buffer + status;
	}
	ch = *input_pointer++;
    } while (ch == (char) 13); /* Ignore ASCII carriage return */

    return FROMASCII(UCH(ch));
}

#ifdef USE_SSL
PUBLIC char HTGetSSLCharacter ARGS1(void *, handle)
{
    char ch;
    interrupted_in_htgetcharacter = 0;
    if(!handle)
	return (char)EOF;
    do {
	if (input_pointer >= input_limit) {
	    int status = SSL_read((SSL *)handle,
				 input_buffer, INPUT_BUFFER_SIZE);
	    if (status <= 0) {
		if (status == 0)
		    return (char)EOF;
		if (status == HT_INTERRUPTED) {
		    CTRACE((tfp, "HTFormat: Interrupted in HTGetSSLCharacter\n"));
		    interrupted_in_htgetcharacter = 1;
		    return (char)EOF;
		}
		CTRACE((tfp, "HTFormat: SSL_read error %d\n", status));
		return (char)EOF; /* -1 is returned by UCX
				     at end of HTTP link */
	    }
	    input_pointer = input_buffer;
	    input_limit = input_buffer + status;
	}
	ch = *input_pointer++;
    } while (ch == (char) 13); /* Ignore ASCII carriage return */

    return FROMASCII(ch);
}
#endif /* USE_SSL */

/*  Match maintype to any MIME type starting with maintype,
 *  for example:  image/gif should match image
 */
PRIVATE int half_match ARGS2(char *,trial_type, char *,target)
{
    char *cp = strchr(trial_type, '/');

    /* if no '/' or no '*' */
    if (!cp || *(cp+1) != '*')
	return 0;

    CTRACE((tfp, "HTFormat: comparing %s and %s for half match\n",
		trial_type, target));

	/* main type matches */
    if (!strncmp(trial_type, target, (cp-trial_type)-1))
	return 1;

    return 0;
}

#define WWW_WILDCARD_REP_OUT HTAtom_for("*")

/*		Look up a presentation
**		----------------------
**
**	If fill_in is NULL, only look for an exact match.
**	If a wildcard match is made, *fill_in is used to store
**	a possibly modified presentation, and a pointer to it is
**	returned.  For an exact match, a pointer to the presentation
**	in the HTPresentations list is returned.  Returns NULL if
**	nothing found. - kw
**
*/
PRIVATE HTPresentation * HTFindPresentation ARGS3(
	HTFormat,		rep_in,
	HTFormat,		rep_out,
	HTPresentation*,	fill_in)
{
    HTAtom * wildcard = NULL; /* = HTAtom_for("*"); lookup when needed - kw */

    CTRACE((tfp, "HTFormat: Looking up presentation for %s to %s\n",
		HTAtom_name(rep_in), HTAtom_name(rep_out)));

    /* don't do anymore do it in the Lynx code at startup LJM */
    /* if (!HTPresentations) HTFormatInit(); */ /* set up the list */

    {
	int n = HTList_count(HTPresentations);
	int i;
	HTPresentation * pres, *match,
			*strong_wildcard_match=0,
			*weak_wildcard_match=0,
			*last_default_match=0,
			*strong_subtype_wildcard_match=0;

	for (i = 0; i < n; i++) {
	    pres = (HTPresentation *)HTList_objectAt(HTPresentations, i);
	    if (pres->rep == rep_in) {
		if (pres->rep_out == rep_out) {
		    CTRACE((tfp, "FindPresentation: found exact match: %s\n",
				HTAtom_name(pres->rep)));
		    return pres;

		} else if (!fill_in) {
		    continue;
		} else {
		    if (!wildcard) wildcard = WWW_WILDCARD_REP_OUT;
		    if (pres->rep_out == wildcard) {
			if (!strong_wildcard_match)
			    strong_wildcard_match = pres;
			/* otherwise use the first one */
			CTRACE((tfp, "StreamStack: found strong wildcard match: %s\n",
				    HTAtom_name(pres->rep)));
		    }
		}

	    } else if (!fill_in) {
		continue;

	    } else if (half_match(HTAtom_name(pres->rep),
					      HTAtom_name(rep_in))) {
		if (pres->rep_out == rep_out) {
		    if (!strong_subtype_wildcard_match)
			strong_subtype_wildcard_match = pres;
		    /* otherwise use the first one */
		    CTRACE((tfp, "StreamStack: found strong subtype wildcard match: %s\n",
				HTAtom_name(pres->rep)));
		}
	    }

	    if (pres->rep == WWW_SOURCE) {
		if (pres->rep_out == rep_out) {
		    if (!weak_wildcard_match)
			weak_wildcard_match = pres;
		    /* otherwise use the first one */
		    CTRACE((tfp, "StreamStack: found weak wildcard match: %s\n",
				HTAtom_name(pres->rep_out)));

		} else if (!last_default_match) {
		    if (!wildcard) wildcard = WWW_WILDCARD_REP_OUT;
		    if (pres->rep_out == wildcard)
			 last_default_match = pres;
		    /* otherwise use the first one */
		}
	    }
	}

	match = strong_subtype_wildcard_match ? strong_subtype_wildcard_match :
		strong_wildcard_match ? strong_wildcard_match :
		weak_wildcard_match ? weak_wildcard_match :
		last_default_match;

	if (match) {
	    *fill_in = *match;		/* Specific instance */
	    fill_in->rep = rep_in;		/* yuk */
	    fill_in->rep_out = rep_out; /* yuk */
	    return fill_in;
	}
    }

    return NULL;
}

/*		Create a filter stack
**		---------------------
**
**	If a wildcard match is made, a temporary HTPresentation
**	structure is made to hold the destination format while the
**	new stack is generated. This is just to pass the out format to
**	MIME so far.  Storing the format of a stream in the stream might
**	be a lot neater.
**
*/
PUBLIC HTStream * HTStreamStack ARGS4(
	HTFormat,		rep_in,
	HTFormat,		rep_out,
	HTStream*,		sink,
	HTParentAnchor*,	anchor)
{
    HTPresentation temp;
    HTPresentation *match;
    HTStream *result;

    CTRACE((tfp, "HTFormat: Constructing stream stack for %s to %s\n",
		HTAtom_name(rep_in), HTAtom_name(rep_out)));

    /* don't return on WWW_SOURCE some people might like
     * to make use of the source!!!!  LJM
     */
#if 0
    if (rep_out == WWW_SOURCE || rep_out == rep_in)
	return sink;	/*  LJM */
#endif

    if (rep_out == rep_in) {
	result = sink;

    } else if ((match = HTFindPresentation(rep_in, rep_out, &temp))) {
	if (match == &temp) {
	    CTRACE((tfp, "StreamStack: Using %s\n", HTAtom_name(temp.rep_out)));
	} else {
	    CTRACE((tfp, "StreamStack: found exact match: %s\n",
			HTAtom_name(match->rep)));
	}
	result = (*match->converter)(match, anchor, sink);
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
**		-------------------------------------
**
**	Look up a presentation (exact match only) and, if found, reorder
**	it to the start of the HTPresentations list. - kw
*/
PUBLIC void HTReorderPresentation ARGS2(
	HTFormat,		rep_in,
	HTFormat,		rep_out)
{
    HTPresentation *match;
    if ((match = HTFindPresentation(rep_in, rep_out, NULL))) {
	HTList_removeObject(HTPresentations, match);
	HTList_addObject(HTPresentations, match);
    }
}

/*
 * Setup 'get_accept' flag to denote presentations that are not redundant,
 * and will be listed in "Accept:" header.
 */
PUBLIC void HTFilterPresentations NOARGS
{
    int i, j;
    int n = HTList_count(HTPresentations);
    HTPresentation *p, *q;
    BOOL matched;
    char *s, *t, *x, *y;

    for (i = 0; i < n; i++) {
	p = (HTPresentation *)HTList_objectAt(HTPresentations, i);
	s = HTAtom_name(p->rep);

	if (p->rep_out == WWW_PRESENT) {
	    if (p->rep != WWW_SOURCE
	     && strcasecomp(s, "www/mime")
	     && strcasecomp(s, "www/compressed")
	     && p->quality <= 1.0 && p->quality >= 0.0) {
		for (j = 0, matched = FALSE; j < i; j++) {
		    q = (HTPresentation *)HTList_objectAt(HTPresentations, j);
		    t = HTAtom_name(q->rep);

		    if (!strcasecomp(s, t)) {
			matched = TRUE;
			break;
		    }
		    if ((x = strchr(s, '/')) != 0
		     && (y = strchr(t, '/')) != 0) {
			int len1 = x++ - s;
			int len2 = y++ - t;
			int lens = (len1 > len2) ? len1 : len2;

			if ((*t == '*' || !strncasecomp(s, t, lens))
			 && (*y == '*' || !strcasecomp(x, y))) {
			    matched = TRUE;
			    break;
			}
		    }
		}
		if (!matched)
		    p->get_accept = TRUE;
	    }
	}
    }
}

/*		Find the cost of a filter stack
**		-------------------------------
**
**	Must return the cost of the same stack which StreamStack would set up.
**
** On entry,
**	length	The size of the data to be converted
*/
PUBLIC float HTStackValue ARGS4(
	HTFormat,		rep_in,
	HTFormat,		rep_out,
	float,			initial_value,
	long int,		length)
{
    HTAtom * wildcard = WWW_WILDCARD_REP_OUT;

    CTRACE((tfp, "HTFormat: Evaluating stream stack for %s worth %.3f to %s\n",
		HTAtom_name(rep_in), initial_value, HTAtom_name(rep_out)));

    if (rep_out == WWW_SOURCE || rep_out == rep_in)
	return 0.0;

    /* don't do anymore do it in the Lynx code at startup LJM */
    /* if (!HTPresentations) HTFormatInit(); */ /* set up the list */

    {
	int n = HTList_count(HTPresentations);
	int i;
	HTPresentation * pres;
	for (i = 0; i < n; i++) {
	    pres = (HTPresentation *)HTList_objectAt(HTPresentations, i);
	    if (pres->rep == rep_in &&
		(pres->rep_out == rep_out || pres->rep_out == wildcard)) {
		float value = initial_value * pres->quality;
		if (HTMaxSecs != 0.0)
		    value = value - (length*pres->secs_per_byte + pres->secs)
					 /HTMaxSecs;
		return value;
	    }
	}
    }

    return (float) -1e30;	/* Really bad */

}

/*	Display the page while transfer in progress
**	-------------------------------------------
**
**   Repaint the page only when necessary.
**   This is a traverse call for HText_pageDisplay() - it works!.
**
*/
PUBLIC void HTDisplayPartial NOARGS
{
#ifdef DISP_PARTIAL
    if (display_partial) {
	/*
	**  HText_getNumOfLines() = "current" number of complete lines received
	**  NumOfLines_partial = number of lines at the moment of last repaint.
	**  (we update NumOfLines_partial only when we repaint the display.)
	**
	**  display_partial could only be enabled in HText_new()
	**  so a new HTMainText object available - all HText_ functions use it,
	**  lines counter HText_getNumOfLines() in particular.
	**
	**  Otherwise HTMainText holds info from the previous document
	**  and we may repaint it instead of the new one:
	**  prev doc scrolled to the first line (=Newline_partial)
	**  is not good looking :-)	  23 Aug 1998 Leonid Pauzner
	**
	**  So repaint the page only when necessary:
	*/
	int Newline_partial = LYGetNewline();

	if (((Newline_partial + display_lines) - 1 > NumOfLines_partial)
		/* current page not complete... */
	&& (partial_threshold > 0 ?
		((Newline_partial + partial_threshold) -1 <= HText_getNumOfLines()) :
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
#endif  /* DISP_PARTIAL */
}

/* Put this as early as possible, OK just after HTDisplayPartial() */
PUBLIC void HTFinishDisplayPartial NOARGS
{
#ifdef DISP_PARTIAL
		    /*
		     *  End of incremental rendering stage here.
		     */
		    display_partial = FALSE;
#endif /* DISP_PARTIAL */
}

/*	Push data from a socket down a stream
**	-------------------------------------
**
**   This routine is responsible for creating and PRESENTING any
**   graphic (or other) objects described by the file.
**
**   The file number given is assumed to be a TELNET stream, i.e., containing
**   CRLF at the end of lines which need to be stripped to LF for unix
**   when the format is textual.
**
**  State of socket and target stream on entry:
**			socket (file_number) assumed open,
**			target (sink) assumed valid.
**
**  Return values:
**	HT_INTERRUPTED  Interruption or error after some data received.
**	-2		Unexpected disconnect before any data received.
**	-1		Interruption or error before any data received, or
**			(UNIX) other read error before any data received, or
**			download cancelled.
**	HT_LOADED	Normal close of socket (end of file indication
**			received), or
**			unexpected disconnect after some data received, or
**			other read error after some data received, or
**			(not UNIX) other read error before any data received.
**
**  State of socket and target stream on return depends on return value:
**	HT_INTERRUPTED	socket still open, target aborted.
**	-2		socket still open, target stream still valid.
**	-1		socket still open, target aborted.
**	otherwise	socket closed,	target stream still valid.
*/
PUBLIC int HTCopy ARGS4(
	HTParentAnchor *,	anchor,
	int,			file_number,
	void*,			handle GCC_UNUSED,
	HTStream*,		sink)
{
    HTStreamClass targetClass;
    BOOL suppress_readprogress = NO;
    int bytes;
    int rv = 0;

    /*	Push the data down the stream
    */
    targetClass = *(sink->isa); /* Copy pointers to procedures */

    /*	Push binary from socket down sink
    **
    **	This operation could be put into a main event loop
    */
    HTReadProgress(bytes = 0, 0);
    for (;;) {
	int status;

	if (LYCancelDownload) {
	    LYCancelDownload = FALSE;
	    (*targetClass._abort)(sink, NULL);
	    rv = -1;
	    goto finished;
	}

	if (HTCheckForInterrupt()) {
	    _HTProgress (TRANSFER_INTERRUPTED);
	    (*targetClass._abort)(sink, NULL);
	    if (bytes)
		rv = HT_INTERRUPTED;
	    else
		rv = -1;
	    goto finished;
	}

#ifdef USE_SSL
	if (handle)
	    status = SSL_read((SSL *)handle, input_buffer, INPUT_BUFFER_SIZE);
	else
	    status = NETREAD(file_number, input_buffer, INPUT_BUFFER_SIZE);
#else
	status = NETREAD(file_number, input_buffer, INPUT_BUFFER_SIZE);
#endif /* USE_SSL */

	if (status <= 0) {
	    if (status == 0) {
		break;
	    } else if (status == HT_INTERRUPTED) {
		_HTProgress (TRANSFER_INTERRUPTED);
		(*targetClass._abort)(sink, NULL);
		if (bytes)
		    rv = HT_INTERRUPTED;
		else
		    rv = -1;
		goto finished;
	    } else if (SOCKET_ERRNO == ENOTCONN ||
#ifdef _WINDOWS	/* 1997/11/10 (Mon) 16:57:18 */
		       SOCKET_ERRNO == ETIMEDOUT ||
#endif
		       SOCKET_ERRNO == ECONNRESET ||
		       SOCKET_ERRNO == EPIPE) {
		/*
		 *  Arrrrgh, HTTP 0/1 compatibility problem, maybe.
		 */
		if (bytes <= 0) {
		    /*
		     *	Don't have any data, so let the calling
		     *	function decide what to do about it. - FM
		     */
		    rv = -2;
		    goto finished;
		} else {
#ifdef UNIX
		   /*
		    *  Treat what we've received already as the complete
		    *  transmission, but not without giving the user
		    *  an alert.  I don't know about all the different
		    *  TCP stacks for VMS etc., so this is currently
		    *  only for UNIX. - kw
		    */
		   HTInetStatus("NETREAD");
		   HTAlert("Unexpected server disconnect.");
		   CTRACE((tfp,
	    "HTCopy: Unexpected server disconnect. Treating as completed.\n"));
		   status = 0;
		   break;
#else  /* !UNIX */
		   /*
		    *  Treat what we've gotten already
		    *  as the complete transmission. - FM
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
		 *  Treat what we've received already as the complete
		 *  transmission, but not without giving the user
		 *  an alert.  I don't know about all the different
		 *  TCP stacks for VMS etc., so this is currently
		 *  only for UNIX. - kw
		 */
		HTInetStatus("NETREAD");
		HTAlert("Unexpected read error.");
		if (bytes) {
		    (void)NETCLOSE(file_number);
		    rv = HT_LOADED;
		} else {
		    (*targetClass._abort)(sink, NULL);
		    rv = -1;
		}
		goto finished;
#endif
	    }
	    break;
	}

	/*
	 *  Suppress ReadProgress messages when collecting a redirection
	 *  message, at least initially (unless/until anchor->content_type
	 *  gets changed, probably by the MIME message parser).  That way
	 *  messages put up by the HTTP module or elsewhere can linger in
	 *  the statusline for a while. - kw
	 */
	suppress_readprogress = (anchor && anchor->content_type &&
				 !strcmp(anchor->content_type,
					 "message/x-http-redirection"));
#ifdef NOT_ASCII
	{
	    char * p;
	    for (p = input_buffer; p < input_buffer+status; p++) {
		*p = FROMASCII(*p);
	    }
	}
#endif /* NOT_ASCII */

	(*targetClass.put_block)(sink, input_buffer, status);
	bytes += status;
	if (!suppress_readprogress)
	    HTReadProgress(bytes, anchor ? anchor->content_length : 0);
	HTDisplayPartial();

    } /* next bufferload */

    _HTProgress(TRANSFER_COMPLETE);
    (void)NETCLOSE(file_number);
    rv = HT_LOADED;

finished:
    HTFinishDisplayPartial();
    return(rv);
}

/*	Push data from a file pointer down a stream
**	-------------------------------------
**
**   This routine is responsible for creating and PRESENTING any
**   graphic (or other) objects described by the file.
**
**
**  State of file and target stream on entry:
**			FILE* (fp) assumed open,
**			target (sink) assumed valid.
**
**  Return values:
**	HT_INTERRUPTED  Interruption after some data read.
**	HT_PARTIAL_CONTENT	Error after some data read.
**	-1		Error before any data read.
**	HT_LOADED	Normal end of file indication on reading.
**
**  State of file and target stream on return:
**	always		fp still open, target stream still valid.
*/
PUBLIC int HTFileCopy ARGS2(
	FILE *,			fp,
	HTStream*,		sink)
{
    HTStreamClass targetClass;
    int status, bytes;
    int rv = HT_OK;

    /*	Push the data down the stream
    */
    targetClass = *(sink->isa); /* Copy pointers to procedures */

    /*	Push binary from socket down sink
    */
    HTReadProgress(bytes = 0, 0);
    for (;;) {
	status = fread(input_buffer, 1, INPUT_BUFFER_SIZE, fp);
	if (status == 0) { /* EOF or error */
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

	(*targetClass.put_block)(sink, input_buffer, status);
	bytes += status;
	HTReadProgress(bytes, 0);
	/*  Suppress last screen update in partial mode - a regular update
	 *  under control of mainloop() should follow anyway. - kw
	 */
#ifdef DISP_PARTIAL
	if (display_partial && bytes != HTMainAnchor->content_length)
	    HTDisplayPartial();
#endif

	if (HTCheckForInterrupt()) {
	    _HTProgress (TRANSFER_INTERRUPTED);
	    if (bytes) {
		rv = HT_INTERRUPTED;
	    } else {
		rv = -1;
	    }
	    break;
	}
    } /* next bufferload */

    HTFinishDisplayPartial();
    return rv;
}

#ifdef USE_SOURCE_CACHE
/*	Push data from an HTChunk down a stream
**	---------------------------------------
**
**   This routine is responsible for creating and PRESENTING any
**   graphic (or other) objects described by the file.
**
**  State of memory and target stream on entry:
**			HTChunk* (chunk) and target (sink) assumed valid.
**
**  Return values:
**	HT_LOADED	All data sent.
**	HT_INTERRUPTED  Interruption after some data read.
**
**  State of memory and target stream on return:
**	always		chunk unchanged, target stream still valid.
*/
PUBLIC int HTMemCopy ARGS2(
	HTChunk *,		chunk,
	HTStream *,		sink)
{
    HTStreamClass targetClass;
    int bytes = 0;
    CONST char *data = chunk->data;
    int rv = HT_OK;

    targetClass = *(sink->isa);
    HTReadProgress(0, 0);
    for (;;) {
	/* Push the data down the stream a piece at a time, in case we're
	** running a large document on a slow machine.
	*/
	int n = INPUT_BUFFER_SIZE;
	if (n > chunk->size - bytes)
	    n = chunk->size - bytes;
	if (n == 0)
	    break;
	(*targetClass.put_block)(sink, data, n);
	bytes += n;
	data += n;
	HTReadProgress(bytes, 0);
	HTDisplayPartial();

	if (HTCheckForInterrupt()) {
	    _HTProgress (TRANSFER_INTERRUPTED);
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
**	-------------------------------------
**
**   This routine is responsible for creating and PRESENTING any
**   graphic (or other) objects described by the file.
**
**
**  State of file and target stream on entry:
**		      gzFile (gzfp) assumed open (should have gzipped content),
**		      target (sink) assumed valid.
**
**  Return values:
**	HT_INTERRUPTED  Interruption after some data read.
**	HT_PARTIAL_CONTENT	Error after some data read.
**	-1		Error before any data read.
**	HT_LOADED	Normal end of file indication on reading.
**
**  State of file and target stream on return:
**	always		gzfp still open, target stream still valid.
*/
PRIVATE int HTGzFileCopy ARGS2(
	gzFile,			gzfp,
	HTStream*,		sink)
{
    HTStreamClass targetClass;
    int status, bytes;
    int gzerrnum;
    int rv = HT_OK;

    /*	Push the data down the stream
    */
    targetClass = *(sink->isa); /* Copy pointers to procedures */

    /*	read and inflate gzip'd file, and push binary down sink
    */
    HTReadProgress(bytes = 0, 0);
    for (;;) {
	status = gzread(gzfp, input_buffer, INPUT_BUFFER_SIZE);
	if (status <= 0) { /* EOF or error */
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

	(*targetClass.put_block)(sink, input_buffer, status);
	bytes += status;
	HTReadProgress(bytes, -1);
	HTDisplayPartial();

	if (HTCheckForInterrupt()) {
	    _HTProgress (TRANSFER_INTERRUPTED);
	    if (bytes) {
		rv = HT_INTERRUPTED;
	    } else {
		rv = -1;
	    }
	    break;
	}
    } /* next bufferload */

    HTFinishDisplayPartial();
    return rv;
}
#endif /* USE_ZLIB */

#ifdef USE_BZLIB
/*	Push data from a bzip file pointer down a stream
**	-------------------------------------
**
**   This routine is responsible for creating and PRESENTING any
**   graphic (or other) objects described by the file.
**
**
**  State of file and target stream on entry:
**		      BZFILE (bzfp) assumed open (should have bzipped content),
**		      target (sink) assumed valid.
**
**  Return values:
**	HT_INTERRUPTED  Interruption after some data read.
**	HT_PARTIAL_CONTENT	Error after some data read.
**	-1		Error before any data read.
**	HT_LOADED	Normal end of file indication on reading.
**
**  State of file and target stream on return:
**	always		bzfp still open, target stream still valid.
*/
PRIVATE int HTBzFileCopy ARGS2(
	BZFILE *,		bzfp,
	HTStream*,		sink)
{
    HTStreamClass targetClass;
    int status, bytes;
    int bzerrnum;
    int rv = HT_OK;

    /*	Push the data down the stream
    */
    targetClass = *(sink->isa); /* Copy pointers to procedures */

    /*	read and inflate bzip'd file, and push binary down sink
    */
    HTReadProgress(bytes = 0, 0);
    for (;;) {
	status = BZ2_bzread(bzfp, input_buffer, INPUT_BUFFER_SIZE);
	if (status <= 0) { /* EOF or error */
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

	(*targetClass.put_block)(sink, input_buffer, status);
	bytes += status;
	HTReadProgress(bytes, -1);
	HTDisplayPartial();

	if (HTCheckForInterrupt()) {
	    _HTProgress (TRANSFER_INTERRUPTED);
	    if (bytes) {
		rv = HT_INTERRUPTED;
	    } else {
		rv = -1;
	    }
	    break;
	}
    } /* next bufferload */

    HTFinishDisplayPartial();
    return rv;
}
#endif /* USE_BZLIB */

/*	Push data from a socket down a stream STRIPPING CR
**	--------------------------------------------------
**
**   This routine is responsible for creating and PRESENTING any
**   graphic (or other) objects described by the socket.
**
**   The file number given is assumed to be a TELNET stream ie containing
**   CRLF at the end of lines which need to be stripped to LF for unix
**   when the format is textual.
**
*/
PUBLIC void HTCopyNoCR ARGS3(
	HTParentAnchor *,	anchor GCC_UNUSED,
	int,			file_number,
	HTStream*,		sink)
{
    HTStreamClass targetClass;
    int character;

    /*	Push the data, ignoring CRLF, down the stream
    */
    targetClass = *(sink->isa); /* Copy pointers to procedures */

    /*	Push text from telnet socket down sink
    **
    **	@@@@@ To push strings could be faster? (especially is we
    **	cheat and don't ignore CR! :-}
    */
    HTInitInput(file_number);
    for (;;) {
	character = HTGetCharacter();
	if (character == EOF)
	    break;
	(*targetClass.put_character)(sink, UCH(character));
    }
}

/*	Parse a socket given format and file number
**
**   This routine is responsible for creating and PRESENTING any
**   graphic (or other) objects described by the file.
**
**   The file number given is assumed to be a TELNET stream ie containing
**   CRLF at the end of lines which need to be stripped to LF for unix
**   when the format is textual.
**
**  State of socket and target stream on entry:
**			socket (file_number) assumed open,
**			target (sink) usually NULL (will call stream stack).
**
**  Return values:
**	HT_INTERRUPTED  Interruption or error after some data received.
**	-501		Stream stack failed (cannot present or convert).
**	-2		Unexpected disconnect before any data received.
**	-1		Stream stack failed (cannot present or convert), or
**			Interruption or error before any data received, or
**			(UNIX) other read error before any data received, or
**			download cancelled.
**	HT_LOADED	Normal close of socket (end of file indication
**			received), or
**			unexpected disconnect after some data received, or
**			other read error after some data received, or
**			(not UNIX) other read error before any data received.
**
**  State of socket and target stream on return depends on return value:
**	HT_INTERRUPTED	socket still open, target aborted.
**	-501		socket still open, target stream NULL.
**	-2		socket still open, target freed.
**	-1		socket still open, target stream aborted or NULL.
**	otherwise	socket closed,	target stream freed.
*/
PUBLIC int HTParseSocket ARGS5(
	HTFormat,		rep_in,
	HTFormat,		format_out,
	HTParentAnchor *,	anchor,
	int,			file_number,
	HTStream*,		sink)
{
    HTStream * stream;
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
	rv = HTLoadError(sink, 501, buffer); /* returns -501 */
	FREE(buffer);
    } else {
	/*
	** Push the data, don't worry about CRLF we can strip them later.
	*/
	targetClass = *(stream->isa);	/* Copy pointers to procedures */
	rv = HTCopy(anchor, file_number, NULL, stream);
	if (rv != -1 && rv != HT_INTERRUPTED)
	    (*targetClass._free)(stream);
    }
    return rv;
    /* Originally:  full: HT_LOADED;  partial: HT_INTERRUPTED;  no bytes: -1 */
}

/*	Parse a file given format and file pointer
**
**   This routine is responsible for creating and PRESENTING any
**   graphic (or other) objects described by the file.
**
**   The file number given is assumed to be a TELNET stream ie containing
**   CRLF at the end of lines which need to be stripped to \n for unix
**   when the format is textual.
**
**  State of file and target stream on entry:
**			FILE* (fp) assumed open,
**			target (sink) usually NULL (will call stream stack).
**
**  Return values:
**	-501		Stream stack failed (cannot present or convert).
**	-1		Download cancelled.
**	HT_NO_DATA	Error before any data read.
**	HT_PARTIAL_CONTENT	Interruption or error after some data read.
**	HT_LOADED	Normal end of file indication on reading.
**
**  State of file and target stream on return:
**	always		fp still open; target freed, aborted, or NULL.
*/
PUBLIC int HTParseFile ARGS5(
	HTFormat,		rep_in,
	HTFormat,		format_out,
	HTParentAnchor *,	anchor,
	FILE *,			fp,
	HTStream*,		sink)
{
    HTStream * stream;
    HTStreamClass targetClass;
    int rv;

#ifdef SH_EX		/* 1998/01/04 (Sun) 16:04:09 */
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

    /*	Push the data down the stream
    **
    **	@@  Bug:  This decision ought to be made based on "encoding"
    **	rather than on content-type.  @@@  When we handle encoding.
    **	The current method smells anyway.
    */
    targetClass = *(stream->isa);	/* Copy pointers to procedures */
    rv = HTFileCopy(fp, stream);
    if (rv == -1 || rv == HT_INTERRUPTED) {
	(*targetClass._abort)(stream, NULL);
    } else {
	(*targetClass._free)(stream);
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
**
**   This routine is responsible for creating and PRESENTING any
**   graphic (or other) objects described by the file.
**
**  State of memory and target stream on entry:
**			HTChunk* (chunk) assumed valid,
**			target (sink) usually NULL (will call stream stack).
**
**  Return values:
**	-501		Stream stack failed (cannot present or convert).
**	HT_LOADED	All data sent.
**
**  State of memory and target stream on return:
**	always		chunk unchanged; target freed, aborted, or NULL.
*/
PUBLIC int HTParseMem ARGS5(
	HTFormat,		rep_in,
	HTFormat,		format_out,
	HTParentAnchor *,	anchor,
	HTChunk *,		chunk,
	HTStream *,		sink)
{
    HTStream * stream;
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
    (*targetClass._free)(stream);
    return HT_LOADED;
}
#endif

#ifdef USE_ZLIB
PRIVATE int HTCloseGzFile ARGS1(
	gzFile,			gzfp)
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
    return(gzres);
}

/*	HTParseGzFile
**
**  State of file and target stream on entry:
**			gzFile (gzfp) assumed open,
**			target (sink) usually NULL (will call stream stack).
**
**  Return values:
**	-501		Stream stack failed (cannot present or convert).
**	-1		Download cancelled.
**	HT_NO_DATA	Error before any data read.
**	HT_PARTIAL_CONTENT	Interruption or error after some data read.
**	HT_LOADED	Normal end of file indication on reading.
**
**  State of file and target stream on return:
**	always		gzfp closed; target freed, aborted, or NULL.
*/
PUBLIC int HTParseGzFile ARGS5(
	HTFormat,		rep_in,
	HTFormat,		format_out,
	HTParentAnchor *,	anchor,
	gzFile,			gzfp,
	HTStream*,		sink)
{
    HTStream * stream;
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

    /*	Push the data down the stream
    **
    **	@@  Bug:  This decision ought to be made based on "encoding"
    **	rather than on content-type.  @@@  When we handle encoding.
    **	The current method smells anyway.
    */
    targetClass = *(stream->isa);	/* Copy pointers to procedures */
    rv = HTGzFileCopy(gzfp, stream);
    if (rv == -1 || rv == HT_INTERRUPTED) {
	(*targetClass._abort)(stream, NULL);
    } else {
	(*targetClass._free)(stream);
    }

    HTCloseGzFile(gzfp);
    if (rv == -1)
	return HT_NO_DATA;
    else if (rv == HT_INTERRUPTED || (rv > 0 && rv != HT_LOADED))
	return HT_PARTIAL_CONTENT;
    else
	return HT_LOADED;
}
#endif /* USE_ZLIB */

#ifdef USE_BZLIB
PRIVATE void HTCloseBzFile ARGS1(
	BZFILE *,		bzfp)
{
    if (bzfp)
	BZ2_bzclose(bzfp);
}

/*	HTParseBzFile
**
**  State of file and target stream on entry:
**			bzFile (bzfp) assumed open,
**			target (sink) usually NULL (will call stream stack).
**
**  Return values:
**	-501		Stream stack failed (cannot present or convert).
**	-1		Download cancelled.
**	HT_NO_DATA	Error before any data read.
**	HT_PARTIAL_CONTENT	Interruption or error after some data read.
**	HT_LOADED	Normal end of file indication on reading.
**
**  State of file and target stream on return:
**	always		bzfp closed; target freed, aborted, or NULL.
*/
PUBLIC int HTParseBzFile ARGS5(
	HTFormat,		rep_in,
	HTFormat,		format_out,
	HTParentAnchor *,	anchor,
	BZFILE*,		bzfp,
	HTStream*,		sink)
{
    HTStream * stream;
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

    /*	Push the data down the stream
    **
    **	@@  Bug:  This decision ought to be made based on "encoding"
    **	rather than on content-type.  @@@  When we handle encoding.
    **	The current method smells anyway.
    */
    targetClass = *(stream->isa);	/* Copy pointers to procedures */
    rv = HTBzFileCopy(bzfp, stream);
    if (rv == -1 || rv == HT_INTERRUPTED) {
	(*targetClass._abort)(stream, NULL);
    } else {
	(*targetClass._free)(stream);
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
**	-----------------------------------------------------------
**
**	The input is assumed to be in ASCII, with lines delimited
**	by (13,10) pairs, These pairs are converted into (CR,LF)
**	pairs in the local representation.  The (CR,LF) sequence
**	when found is changed to a '\n' character, the internal
**	C representation of a new line.
*/

PRIVATE void NetToText_put_character ARGS2(HTStream *, me, char, net_char)
{
    char c = FROMASCII(net_char);
    if (me->had_cr) {
	if (c == LF) {
	    me->sink->isa->put_character(me->sink, '\n');	/* Newline */
	    me->had_cr = NO;
	    return;
	} else {
	    me->sink->isa->put_character(me->sink, CR); /* leftover */
	}
    }
    me->had_cr = (BOOL) (c == CR);
    if (!me->had_cr)
	me->sink->isa->put_character(me->sink, c);		/* normal */
}

PRIVATE void NetToText_put_string ARGS2(HTStream *, me, CONST char *, s)
{
    CONST char * p;

    for (p = s; *p; p++)
	NetToText_put_character(me, *p);
}

PRIVATE void NetToText_put_block ARGS3(HTStream *, me, CONST char*, s, int, l)
{
    CONST char * p;

    for (p = s; p < (s+l); p++)
	NetToText_put_character(me, *p);
}

PRIVATE void NetToText_free ARGS1(HTStream *, me)
{
    (me->sink->isa->_free)(me->sink);		/* Close rest of pipe */
    FREE(me);
}

PRIVATE void NetToText_abort ARGS2(HTStream *, me, HTError, e)
{
    me->sink->isa->_abort(me->sink,e);		/* Abort rest of pipe */
    FREE(me);
}

/*	The class structure
*/
PRIVATE HTStreamClass NetToTextClass = {
    "NetToText",
    NetToText_free,
    NetToText_abort,
    NetToText_put_character,
    NetToText_put_string,
    NetToText_put_block
};

/*	The creation method
*/
PUBLIC HTStream * HTNetToText ARGS1(HTStream *, sink)
{
    HTStream* me = typecalloc(HTStream);

    if (me == NULL)
	outofmem(__FILE__, "NetToText");
    me->isa = &NetToTextClass;

    me->had_cr = NO;
    me->sink = sink;
    return me;
}

PRIVATE HTStream	HTBaseStreamInstance;		      /* Made static */
/*
**	ERROR STREAM
**	------------
**	There is only one error stream shared by anyone who wants a
**	generic error returned from all stream methods.
*/
PRIVATE void HTErrorStream_put_character ARGS2(HTStream *, me GCC_UNUSED, char, c GCC_UNUSED)
{
    LYCancelDownload = TRUE;
}

PRIVATE void HTErrorStream_put_string ARGS2(HTStream *, me GCC_UNUSED, CONST char *, s)
{
    if (s && *s)
	LYCancelDownload = TRUE;
}

PRIVATE void HTErrorStream_write ARGS3(HTStream *, me GCC_UNUSED, CONST char *, s, int, l)
{
    if (l && s)
	LYCancelDownload = TRUE;
}

PRIVATE void HTErrorStream_free ARGS1(HTStream *, me GCC_UNUSED)
{
    return;
}

PRIVATE void HTErrorStream_abort ARGS2(HTStream *, me GCC_UNUSED, HTError, e GCC_UNUSED)
{
    return;
}

PRIVATE CONST HTStreamClass HTErrorStreamClass =
{
    "ErrorStream",
    HTErrorStream_free,
    HTErrorStream_abort,
    HTErrorStream_put_character,
    HTErrorStream_put_string,
    HTErrorStream_write
};

PUBLIC HTStream * HTErrorStream NOARGS
{
    CTRACE((tfp, "ErrorStream. Created\n"));
    HTBaseStreamInstance.isa = &HTErrorStreamClass;    /* The rest is random */
    return &HTBaseStreamInstance;
}
