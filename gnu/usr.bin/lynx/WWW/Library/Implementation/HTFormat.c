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

#include "HTUtils.h"
#include "tcp.h"
#include "HTAccess.h"

/* Implements:
*/
#include "HTFormat.h"

PUBLIC float HTMaxSecs = 1e10;		/* No effective limit */
PUBLIC float HTMaxLength = 1e10;	/* No effective limit */
PUBLIC long int HTMaxBytes  = 0;	/* No effective limit */

#ifdef unix
#ifdef NeXT
#define PRESENT_POSTSCRIPT "open %s; /bin/rm -f %s\n"
#else
#define PRESENT_POSTSCRIPT "(ghostview %s ; /bin/rm -f %s)&\n"
			   /* Full pathname would be better! */
#endif /* NeXT */
#endif /* unix */

#include "HTML.h"
#include "HTMLDTD.h"
#include "HText.h"
#include "HTAlert.h"
#include "HTList.h"
#include "HTInit.h"
#include "HTTCP.h"
/*	Streams and structured streams which we use:
*/
#include "HTFWriter.h"
#include "HTPlain.h"
#include "SGML.h"
#include "HTML.h"
#include "HTMLGen.h"

#include "LYexit.h"
#include "LYUtils.h"
#include "LYGlobalDefs.h"
#include "LYLeaks.h"

#define FREE(x) if (x) {free(x); x = NULL;}

extern int HTCheckForInterrupt NOPARAMS;

PUBLIC	BOOL HTOutputSource = NO;	/* Flag: shortcut parser to stdout */
/* extern  BOOL interactive; LJM */

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
PRIVATE void HTFreePresentations NOPARAMS;

/*	Define a presentation system command for a content-type
**	-------------------------------------------------------
*/
PUBLIC void HTSetPresentation ARGS6(
	CONST char *,	representation,
	CONST char *,	command,
	float,		quality,
	float,		secs,
	float,		secs_per_byte,
	long int,	maxbytes)
{
    HTPresentation * pres = (HTPresentation *)malloc(sizeof(HTPresentation));
    if (pres == NULL)
	outofmem(__FILE__, "HTSetPresentation");

    pres->rep = HTAtom_for(representation);
    pres->rep_out = WWW_PRESENT;		/* Fixed for now ... :-) */
    pres->converter = HTSaveAndExecute; 	/* Fixed for now ...	 */
    pres->quality = quality;
    pres->secs = secs;
    pres->secs_per_byte = secs_per_byte;
    pres->maxbytes = maxbytes;
    pres->command = NULL;
    StrAllocCopy(pres->command, command);

    /*
     *	Memory leak fixed.
     *	05-28-94 Lynx 2-3-1 Garrett Arch Blythe
     */
    if (!HTPresentations)	{
	HTPresentations = HTList_new();
	atexit(HTFreePresentations);
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
    HTPresentation * pres = (HTPresentation *)malloc(sizeof(HTPresentation));
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
	atexit(HTFreePresentations);
    }

    HTList_addObject(HTPresentations, pres);
}

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
PUBLIC char HTGetCharacter NOARGS
{
    char ch;
    interrupted_in_htgetcharacter = 0;
    do {
	if (input_pointer >= input_limit) {
	    int status = NETREAD(input_file_number,
				 input_buffer, INPUT_BUFFER_SIZE);
	    if (status <= 0) {
		if (status == 0)
		    return (char)EOF;
		if (status == HT_INTERRUPTED) {
		    if (TRACE)
			fprintf(stderr,
				"HTFormat: Interrupted in HTGetCharacter\n");
		    interrupted_in_htgetcharacter = 1;
		    return (char)EOF;
		}
		if (TRACE)
		    fprintf(stderr, "HTFormat: File read error %d\n", status);
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

/*  Match maintype to any MIME type starting with maintype,
 *  for example:  image/gif should match image
 */
PRIVATE int half_match ARGS2(char *,trial_type, char *,target)
{
    char *cp=strchr(trial_type,'/');

    /* if no '/' or no '*' */
    if (!cp || *(cp+1) != '*')
	return 0;

    if (TRACE)
	fprintf(stderr,"HTFormat: comparing %s and %s for half match\n",
						      trial_type, target);

	/* main type matches */
    if (!strncmp(trial_type, target, (cp-trial_type)-1))
	return 1;

    return 0;
}

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
    HTAtom * wildcard = HTAtom_for("*");

    if (TRACE)
	fprintf(stderr,
		"HTFormat: Looking up presentation for %s to %s\n",
		HTAtom_name(rep_in), HTAtom_name(rep_out));

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
		    if (TRACE)
			fprintf(stderr,
				"FindPresentation: found exact match: %s\n",
				HTAtom_name(pres->rep));
		    return pres;

		} else if (!fill_in) {
		    continue;
		} else if (pres->rep_out == wildcard) {
		    if (!strong_wildcard_match)
			strong_wildcard_match = pres;
		    /* otherwise use the first one */
		    if (TRACE)
			fprintf(stderr,
			     "StreamStack: found strong wildcard match: %s\n",
				HTAtom_name(pres->rep));
		}

	    } else if (!fill_in) {
		continue;

	    } else if (half_match(HTAtom_name(pres->rep),
					      HTAtom_name(rep_in))) {
		if (pres->rep_out == rep_out) {
		    if (!strong_subtype_wildcard_match)
			strong_subtype_wildcard_match = pres;
		    /* otherwise use the first one */
		    if (TRACE)
			fprintf(stderr,
		     "StreamStack: found strong subtype wildcard match: %s\n",
				HTAtom_name(pres->rep));
		}
	    }

	    if (pres->rep == WWW_SOURCE) {
		if (pres->rep_out == rep_out) {
		    if (!weak_wildcard_match)
			weak_wildcard_match = pres;
		    /* otherwise use the first one */
		    if (TRACE)
			fprintf(stderr,
			    "StreamStack: found weak wildcard match: %s\n",
				HTAtom_name(pres->rep_out));
		}
		if (pres->rep_out == wildcard) {
		    if (!last_default_match)
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

    if (TRACE)
	fprintf(stderr,
		"HTFormat: Constructing stream stack for %s to %s\n",
		HTAtom_name(rep_in), HTAtom_name(rep_out));

    /* don't return on WWW_SOURCE some people might like
     * to make use of the source!!!!  LJM
     *//*
    if (rep_out == WWW_SOURCE || rep_out == rep_in)
	return sink;  LJM */

    if (rep_out == rep_in)
	return sink;

    if ((match = HTFindPresentation(rep_in, rep_out, &temp))) {
	if (match == &temp) {
	    if (TRACE)
		fprintf(stderr,
			"StreamStack: Using %s\n", HTAtom_name(temp.rep_out));
	} else {
	    if (TRACE)
		fprintf(stderr,
			"StreamStack: found exact match: %s\n",
			HTAtom_name(match->rep));
	}
	return (*match->converter)(match, anchor, sink);
    } else {
	return NULL;
    }
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
    HTAtom * wildcard = HTAtom_for("*");

    if (TRACE)
	fprintf(stderr,
		"HTFormat: Evaluating stream stack for %s worth %.3f to %s\n",
		HTAtom_name(rep_in), initial_value, HTAtom_name(rep_out));

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

    return -1e30;		/* Really bad */

}

/*	Push data from a socket down a stream
**	-------------------------------------
**
**   This routine is responsible for creating and PRESENTING any
**   graphic (or other) objects described by the file.
**
**   The file number given is assumed to be a TELNET stream ie containing
**   CRLF at the end of lines which need to be stripped to LF for unix
**   when the format is textual.
**
*/
PUBLIC int HTCopy ARGS4(
	HTParentAnchor *,	anchor,
	int,			file_number,
	void*,			handle GCC_UNUSED,
	HTStream*,		sink)
{
    HTStreamClass targetClass;
    char line[256];
    int bytes = 0;
    int rv = 0;

    /*	Push the data down the stream
    */
    targetClass = *(sink->isa); /* Copy pointers to procedures */

    /*	Push binary from socket down sink
    **
    **	This operation could be put into a main event loop
    */
    for (;;) {
	int status;

	if (LYCancelDownload) {
	    LYCancelDownload = FALSE;
	    (*targetClass._abort)(sink, NULL);
	    rv = -1;
	    goto finished;
	}

	if (HTCheckForInterrupt()) {
	    _HTProgress ("Data transfer interrupted.");
	    (*targetClass._abort)(sink, NULL);
	    if (bytes)
		rv = HT_INTERRUPTED;
	    else
		rv = -1;
	    goto finished;
	}

	status = NETREAD(file_number, input_buffer, INPUT_BUFFER_SIZE);

	if (status <= 0) {
	    if (status == 0) {
		break;
	    } else if (status == HT_INTERRUPTED) {
		_HTProgress ("Data transfer interrupted.");
		(*targetClass._abort)(sink, NULL);
		if (bytes)
		    rv = HT_INTERRUPTED;
		else
		    rv = -1;
		goto finished;
	    } else if (SOCKET_ERRNO == ENOTCONN ||
		       SOCKET_ERRNO == ECONNRESET ||
		       SOCKET_ERRNO == EPIPE) {
		/*
		 *  Arrrrgh, HTTP 0/1 compability problem, maybe.
		 */
		if (bytes <= 0) {
		    /*
		     *	Don't have any data, so let the calling
		     *	function decide what to do about it. - FM
		     */
		    rv = -2;
		    goto finished;
		} else {
		   /*
		    *  Treat what we've gotten already
		    *  as the complete transmission. - FM
		    */
		   if (TRACE)
		       fprintf(stderr,
	    "HTCopy: Unexpected server disconnect. Treating as completed.\n");
		   status = 0;
		   break;
		}
	    }
	    break;
	}

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
	if (anchor && anchor->content_length > 0)
	    sprintf(line, "Read %d of %d bytes of data.",
			  bytes, anchor->content_length);
	else
	    sprintf(line, "Read %d bytes of data.", bytes);
	HTProgress(line);

    } /* next bufferload */

    _HTProgress("Data transfer complete");
    (void)NETCLOSE(file_number);
    rv = HT_LOADED;

finished:
    return(rv);
}

/*	Push data from a file pointer down a stream
**	-------------------------------------
**
**   This routine is responsible for creating and PRESENTING any
**   graphic (or other) objects described by the file.
**
**
*/
PUBLIC int HTFileCopy ARGS2(
	FILE *, 		fp,
	HTStream*,		sink)
{
    HTStreamClass targetClass;
    char line[256];
    int status, bytes = 0, nreads = 0, nprogr = 0;
    int rv = HT_OK;

    /*	Push the data down the stream
    */
    targetClass = *(sink->isa); /* Copy pointers to procedures */

    /*	Push binary from socket down sink
    */
    for (;;) {
	status = fread(input_buffer, 1, INPUT_BUFFER_SIZE, fp);
	nreads++;
	if (status == 0) { /* EOF or error */
	    if (ferror(fp) == 0) {
		rv = HT_LOADED;
		break;
	    }
	    if (TRACE)
		fprintf(stderr,
			"HTFormat: Read error, read returns %d\n",
			ferror(fp));
	    if (bytes) {
		rv = HT_PARTIAL_CONTENT;
	    } else {
		rv = -1;
	    }
	    break;
	}
	(*targetClass.put_block)(sink, input_buffer, status);

	bytes += status;
	if (nreads >= 100) {
	    /*
	    **	Show progress messages for local files, and check for
	    **	user interruption.  Start doing so only after a certain
	    **	number of reads have been done, and don't update it on
	    **	every read (normally reading in a local file should be
	    **	speedy). - KW
	    */
	    if (nprogr == 0) {
		if (bytes < 1024000) {
		    sprintf(line, "Read %d bytes of data.", bytes);
		} else {
		    sprintf(line, "Read %d KB of data. %s",
				  bytes/1024,
		    "(Press 'z' if you want to abort loading.)");
		}
		HTProgress(line);
		if (HTCheckForInterrupt()) {
		    _HTProgress ("Data transfer interrupted.");
		    if (bytes) {
			rv = HT_INTERRUPTED;
		    } else {
			rv = -1;
		    }
		    break;
		}
		nprogr++;
	    } else if (nprogr == 25) {
		nprogr = 0;
	    } else {
		nprogr++;
	    }
	}
    } /* next bufferload */

    return rv;
}

#ifdef USE_ZLIB
/*	Push data from a gzip file pointer down a stream
**	-------------------------------------
**
**   This routine is responsible for creating and PRESENTING any
**   graphic (or other) objects described by the file.
**
**
*/
PRIVATE int HTGzFileCopy ARGS2(
	gzFile, 		gzfp,
	HTStream*,		sink)
{
    HTStreamClass targetClass;
    char line[256];
    int status, bytes = 0, nreads = 0, nprogr = 0;
    int gzerrnum;
    int rv = HT_OK;

    /*	Push the data down the stream
    */
    targetClass = *(sink->isa); /* Copy pointers to procedures */

    /*	read and inflate gzipped file, and push binary down sink
    */
    for (;;) {
	status = gzread(gzfp, input_buffer, INPUT_BUFFER_SIZE);
	nreads++;
	if (status <= 0) { /* EOF or error */
	    if (status == 0) {
		rv = HT_LOADED;
		break;
	    }
	    if (TRACE) {
		fprintf(stderr,
			"HTGzFileCopy: Read error, gzread returns %d\n",
			status);
		fprintf(stderr,
			"gzerror   : %s\n",
			gzerror(gzfp, &gzerrnum));
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
	if (nreads >= 100) {
	    /*
	    **	Show progress messages for local files, and check for
	    **	user interruption.  Start doing so only after a certain
	    **	number of reads have been done, and don't update it on
	    **	every read (normally reading in a local file should be
	    **	speedy). - KW
	    */
	    if (nprogr == 0) {
		if (bytes < 1024000) {
		    sprintf(line,
			    "Read %d uncompressed bytes of data.", bytes);
		} else {
		    sprintf(line, "Read %d uncompressed KB of data. %s",
				  bytes/1024,
		    "(Press 'z' to abort.)");
		}
		HTProgress(line);
		if (HTCheckForInterrupt()) {
		    _HTProgress ("Data transfer interrupted.");
		    if (bytes) {
			rv = HT_INTERRUPTED;
		    } else {
			rv = -1;
		    }
		    break;
		}
		nprogr++;
	    } else if (nprogr == 25) {
		nprogr = 0;
	    } else {
		nprogr++;
	    }
	}
    } /* next bufferload */

    return rv;
}
#endif /* USE_ZLIB */

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
    char character;

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
	if (character == (char)EOF)
	    break;
	(*targetClass.put_character)(sink, character);
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
	char buffer[1024];	/* @@@@@@@@ */
	if (LYCancelDownload) {
	    LYCancelDownload = FALSE;
	    return -1;
	}
	sprintf(buffer, "Sorry, can't convert from %s to %s.",
		HTAtom_name(rep_in), HTAtom_name(format_out));
	if (TRACE)
	    fprintf(stderr, "HTFormat: %s\n", buffer);
	return HTLoadError(sink, 501, buffer); /* returns -501 */
    }

    /*
    ** Push the data, don't worry about CRLF we can strip them later.
    */
    targetClass = *(stream->isa);	/* Copy pointers to procedures */
    rv = HTCopy(anchor, file_number, NULL, stream);
    if (rv != -1 && rv != HT_INTERRUPTED)
	(*targetClass._free)(stream);

    return rv; /* full: HT_LOADED;  partial: HT_INTERRUPTED;  no bytes: -1 */
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
*/
PUBLIC int HTParseFile ARGS5(
	HTFormat,		rep_in,
	HTFormat,		format_out,
	HTParentAnchor *,	anchor,
	FILE *, 		fp,
	HTStream*,		sink)
{
    HTStream * stream;
    HTStreamClass targetClass;
    int rv;

    stream = HTStreamStack(rep_in,
			format_out,
			sink , anchor);

    if (!stream) {
	char buffer[1024];	/* @@@@@@@@ */
	if (LYCancelDownload) {
	    LYCancelDownload = FALSE;
	    return -1;
	}
	sprintf(buffer, "Sorry, can't convert from %s to %s.",
		HTAtom_name(rep_in), HTAtom_name(format_out));
	if (TRACE)
	    fprintf(stderr, "HTFormat(in HTParseFile): %s\n", buffer);
	return HTLoadError(sink, 501, buffer);
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

#ifdef USE_ZLIB
PRIVATE int HTCloseGzFile ARGS1(
	gzFile, 		gzfp)
{
    int gzres;
    if (gzfp == NULL)
	return 0;
    gzres = gzclose(gzfp);
    if (TRACE) {
	if (gzres == Z_ERRNO) {
	    perror("gzclose   ");
	} else if (gzres != Z_OK) {
	    fprintf(stderr, "gzclose   : error number %d\n", gzres);
	}
    }
    return(gzres);
}

PUBLIC int HTParseGzFile ARGS5(
	HTFormat,		rep_in,
	HTFormat,		format_out,
	HTParentAnchor *,	anchor,
	gzFile, 		gzfp,
	HTStream*,		sink)
{
    HTStream * stream;
    HTStreamClass targetClass;
    int rv;

    stream = HTStreamStack(rep_in,
			format_out,
			sink , anchor);

    if (!stream) {
	char buffer[1024];	/* @@@@@@@@ */
	extern char LYCancelDownload;
	HTCloseGzFile(gzfp);
	if (LYCancelDownload) {
	    LYCancelDownload = FALSE;
	    return -1;
	}
	sprintf(buffer, "Sorry, can't convert from %s to %s.",
		HTAtom_name(rep_in), HTAtom_name(format_out));
	if (TRACE)
	    fprintf(stderr, "HTFormat(in HTParseGzFile): %s\n", buffer);
	return HTLoadError(sink, 501, buffer);
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
    me->had_cr = (c == CR);
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
    HTStream* me = (HTStream*)malloc(sizeof(*me));

    if (me == NULL)
	outofmem(__FILE__, "NetToText");
    me->isa = &NetToTextClass;

    me->had_cr = NO;
    me->sink = sink;
    return me;
}

