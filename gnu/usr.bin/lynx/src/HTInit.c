/*
 * $LynxId: HTInit.c,v 1.70 2009/01/01 22:58:06 tom Exp $
 *
 *		Configuration-specific Initialization		HTInit.c
 *		----------------------------------------
 */

/*	Define a basic set of suffixes and presentations
 *	------------------------------------------------
 */

#include <HTUtils.h>

/* Implements:
*/
#include <HTInit.h>

#include <HTML.h>
#include <HTPlain.h>
#include <HTMLGen.h>
#include <HTFile.h>
#include <HTFormat.h>
#include <HTMIME.h>
#include <HTWSRC.h>

#include <HTSaveToFile.h>	/* LJM */
#include <LYStrings.h>
#include <LYUtils.h>
#include <LYGlobalDefs.h>

#include <LYexit.h>
#include <LYLeaks.h>

#define CTrace(p) CTRACE2(TRACE_CFG, p)

static int HTLoadTypesConfigFile(char *fn, AcceptMedia media);
static int HTLoadExtensionsConfigFile(char *fn);

#define SET_SUFFIX1(suffix, description, type) \
	HTSetSuffix(suffix, description, type, 1.0)

#define SET_SUFFIX5(suffix, mimetype, type, description) \
       HTSetSuffix5(suffix, mimetype, type, description, 1.0)

#define SET_PRESENT(mimetype, command, quality, delay) \
  HTSetPresentation(mimetype, command, 0, quality, delay, 0.0, 0, media)

#define SET_EXTERNL(rep_in, rep_out, command, quality) \
    HTSetConversion(rep_in, rep_out, command, quality, 3.0, 0.0, 0, mediaEXT)

#define SET_INTERNL(rep_in, rep_out, command, quality) \
    HTSetConversion(rep_in, rep_out, command, quality, 0.0, 0.0, 0, mediaINT)

void HTFormatInit(void)
{
    AcceptMedia media = mediaEXT;

    CTrace((tfp, "HTFormatInit\n"));
#ifdef NeXT
    SET_PRESENT("application/postscript", "open %s", 1.0, 2.0);
    SET_PRESENT("image/x-tiff", "open %s", 2.0, 2.0);
    SET_PRESENT("image/tiff", "open %s", 1.0, 2.0);
    SET_PRESENT("audio/basic", "open %s", 1.0, 2.0);
    SET_PRESENT("*", "open %s", 1.0, 0.0);
#else
    if (LYgetXDisplay() != 0) {	/* Must have X11 */
	SET_PRESENT("application/postscript", "ghostview %s&", 1.0, 3.0);
	if (non_empty(XLoadImageCommand)) {
	    /* *INDENT-OFF* */
	    SET_PRESENT("image/gif",	   XLoadImageCommand, 1.0, 3.0);
	    SET_PRESENT("image/x-xbm",	   XLoadImageCommand, 1.0, 3.0);
	    SET_PRESENT("image/x-xbitmap", XLoadImageCommand, 1.0, 3.0);
	    SET_PRESENT("image/x-png",	   XLoadImageCommand, 2.0, 3.0);
	    SET_PRESENT("image/png",	   XLoadImageCommand, 1.0, 3.0);
	    SET_PRESENT("image/x-rgb",	   XLoadImageCommand, 1.0, 3.0);
	    SET_PRESENT("image/x-tiff",	   XLoadImageCommand, 2.0, 3.0);
	    SET_PRESENT("image/tiff",	   XLoadImageCommand, 1.0, 3.0);
	    SET_PRESENT("image/jpeg",	   XLoadImageCommand, 1.0, 3.0);
	    /* *INDENT-ON* */

	}
	SET_PRESENT("video/mpeg", "mpeg_play %s &", 1.0, 3.0);

    }
#endif

#ifdef EXEC_SCRIPTS
    /* set quality to 999.0 for protected exec applications */
#ifndef VMS
    SET_PRESENT("application/x-csh", "csh %s", 999.0, 3.0);
    SET_PRESENT("application/x-sh", "sh %s", 999.0, 3.0);
    SET_PRESENT("application/x-ksh", "ksh %s", 999.0, 3.0);
#else
    SET_PRESENT("application/x-VMS_script", "@%s", 999.0, 3.0);
#endif /* not VMS */
#endif /* EXEC_SCRIPTS */

    /*
     * Add our header handlers.
     */
    media = mediaINT;
    SET_INTERNL("message/x-http-redirection", "*", HTMIMERedirect, 2.0);
    SET_INTERNL("message/x-http-redirection", "www/present", HTMIMERedirect, 2.0);
    SET_INTERNL("message/x-http-redirection", "www/debug", HTMIMERedirect, 1.0);
    SET_INTERNL("www/mime", "www/present", HTMIMEConvert, 1.0);
    SET_INTERNL("www/mime", "www/download", HTMIMEConvert, 1.0);
    SET_INTERNL("www/mime", "www/source", HTMIMEConvert, 1.0);
    SET_INTERNL("www/mime", "www/dump", HTMIMEConvert, 1.0);

    /*
     * Add our compressed file handlers.
     */
    SET_INTERNL("www/compressed", "www/download", HTCompressed, 1.0);
    SET_INTERNL("www/compressed", "www/present", HTCompressed, 1.0);
    SET_INTERNL("www/compressed", "www/source", HTCompressed, 1.0);
    SET_INTERNL("www/compressed", "www/dump", HTCompressed, 1.0);

    /*
     * Added the following to support some content types beginning to surface.
     */
    SET_INTERNL("application/html", "text/x-c", HTMLToC, 0.5);
    SET_INTERNL("application/html", "text/plain", HTMLToPlain, 0.5);
    SET_INTERNL("text/css", "text/plain", HTMLToPlain, 0.5);
    SET_INTERNL("application/html", "www/present", HTMLPresent, 2.0);
    SET_INTERNL("application/xhtml+xml", "www/present", HTMLPresent, 2.0);
    SET_INTERNL("application/xml", "www/present", HTMLPresent, 2.0);
    SET_INTERNL("application/html", "www/source", HTPlainPresent, 1.0);
    SET_INTERNL("application/x-wais-source", "www/source", HTPlainPresent, 1.0);
    SET_INTERNL("application/x-wais-source", "www/present", HTWSRCConvert, 2.0);
    SET_INTERNL("application/x-wais-source", "www/download", HTWSRCConvert, 1.0);
    SET_INTERNL("application/x-wais-source", "www/dump", HTWSRCConvert, 1.0);

    /*
     * Save all unknown mime types to disk.
     */
    SET_EXTERNL("www/source", "www/present", HTSaveToFile, 1.0);
    SET_EXTERNL("www/source", "www/source", HTSaveToFile, 1.0);
    SET_EXTERNL("www/source", "www/download", HTSaveToFile, 1.0);
    SET_EXTERNL("www/source", "*", HTSaveToFile, 1.0);

    /*
     * Output all www/dump presentations to stdout.
     */
    SET_EXTERNL("www/source", "www/dump", HTDumpToStdout, 1.0);

    /*
     * Now add our basic conversions.
     */
    SET_INTERNL("text/x-sgml", "www/source", HTPlainPresent, 1.0);
    SET_INTERNL("text/x-sgml", "www/present", HTMLPresent, 2.0);
    SET_INTERNL("text/sgml", "www/source", HTPlainPresent, 1.0);
    SET_INTERNL("text/sgml", "www/present", HTMLPresent, 1.0);
    SET_INTERNL("text/css", "www/present", HTPlainPresent, 1.0);
    SET_INTERNL("text/plain", "www/present", HTPlainPresent, 1.0);
    SET_INTERNL("text/plain", "www/source", HTPlainPresent, 1.0);
    SET_INTERNL("text/html", "www/source", HTPlainPresent, 1.0);
    SET_INTERNL("text/html", "text/x-c", HTMLToC, 0.5);
    SET_INTERNL("text/html", "text/plain", HTMLToPlain, 0.5);
    SET_INTERNL("text/html", "www/present", HTMLPresent, 1.0);
    SET_INTERNL("text/xml", "www/present", HTMLPresent, 2.0);

    if (LYisAbsPath(global_type_map)) {
	/* These should override the default types as necessary.  */
	HTLoadTypesConfigFile(global_type_map, mediaSYS);
    }

    /*
     * Load the local maps.
     */
    if (IsOurFile(LYAbsOrHomePath(&personal_type_map))
	&& LYCanReadFile(personal_type_map)) {
	/* These should override everything else. */
	HTLoadTypesConfigFile(personal_type_map, mediaUSR);
    }

    /*
     * Put text/html and text/plain at beginning of list.  - kw
     */
    HTReorderPresentation(WWW_PLAINTEXT, WWW_PRESENT);
    HTReorderPresentation(WWW_HTML, WWW_PRESENT);

    /*
     * Analyze the list, and set 'get_accept' for those whose representations
     * are not redundant.
     */
    HTFilterPresentations();
}

void HTPreparsedFormatInit(void)
{
    CTrace((tfp, "HTPreparsedFormatInit\n"));
    if (LYPreparsedSource) {
	SET_INTERNL("text/html", "www/source", HTMLParsedPresent, 1.0);
	SET_INTERNL("text/html", "www/dump", HTMLParsedPresent, 1.0);
    }
}

/* Some of the following is taken from: */

/*
Copyright (c) 1991 Bell Communications Research, Inc. (Bellcore)

Permission to use, copy, modify, and distribute this material
for any purpose and without fee is hereby granted, provided
that the above copyright notice and this permission notice
appear in all copies, and that the name of Bellcore not be
used in advertising or publicity pertaining to this
material without the specific, prior written permission
of an authorized representative of Bellcore.  BELLCORE
MAKES NO REPRESENTATIONS ABOUT THE ACCURACY OR SUITABILITY
OF THIS MATERIAL FOR ANY PURPOSE.  IT IS PROVIDED "AS IS",
WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES.
*/
/******************************************************
    Metamail -- A tool to help diverse mail readers
                cope with diverse multimedia mail formats.

    Author:  Nathaniel S. Borenstein, Bellcore

 ******************************************************* */

struct MailcapEntry {
    char *contenttype;
    char *command;
    char *testcommand;
    int needsterminal;
    int copiousoutput;
    int needtofree;
    char *label;
    char *printcommand;
    char *nametemplate;
    float quality;
    long int maxbytes;
};

static int ExitWithError(const char *txt);
static int PassesTest(struct MailcapEntry *mc);

static char *GetCommand(char *s, char **t)
{
    char *s2;
    int quoted = 0;

    s = LYSkipBlanks(s);
    /* marca -- added + 1 for error case -- oct 24, 1993. */
    s2 = typeMallocn(char, strlen(s) * 2 + 1);	/* absolute max, if all % signs */

    if (!s2)
	ExitWithError(MEMORY_EXHAUSTED_ABORT);

    *t = s2;
    while (non_empty(s)) {
	if (quoted) {
	    if (*s == '%')
		*s2++ = '%';	/* Quote through next level, ugh! */

	    *s2++ = *s++;
	    quoted = 0;
	} else {
	    if (*s == ';') {
		*s2 = '\0';
		return (++s);
	    }
	    if (*s == '\\') {
		quoted = 1;
		++s;
	    } else {
		*s2++ = *s++;
	    }
	}
    }
    *s2 = '\0';
    return (NULL);
}

/* no leading or trailing space, all lower case */
static char *Cleanse(char *s)
{
    LYTrimLeading(s);
    LYTrimTrailing(s);
    LYLowerCase(s);
    return (s);
}

/* remove unnecessary (unquoted) blanks in a shell command */
static void TrimCommand(char *command)
{
    LYTrimTrailing(command);
#ifdef UNIX
    {
	char *s = command;
	char *d = command;
	int ch;
	int c0 = ' ';
	BOOL escape = FALSE;
	BOOL dquote = FALSE;
	BOOL squote = FALSE;

	while ((ch = *s++) != '\0') {
	    if (escape) {
		escape = FALSE;
	    } else if (squote) {
		if (ch == '\'')
		    squote = FALSE;
	    } else if (dquote) {
		if (ch == '"')
		    dquote = FALSE;
	    } else {
		switch (ch) {
		case '"':
		    dquote = TRUE;
		    break;
		case '\'':
		    squote = TRUE;
		    break;
		case '\\':
		    if (dquote)
			escape = TRUE;
		    break;
		}
	    }
	    if (!escape && !dquote && !squote) {
		if (ch == '\t')
		    ch = ' ';
		if (ch == ' ') {
		    if (c0 == ' ')
			continue;
		}
	    }
	    *d++ = (char) ch;
	    c0 = ch;
	}
	*d = '\0';
    }
#endif
}

static int ProcessMailcapEntry(FILE *fp, struct MailcapEntry *mc, AcceptMedia media)
{
    size_t rawentryalloc = 2000, len, need;
    char *rawentry, *s, *t;
    char *LineBuf = NULL;

    rawentry = (char *) malloc(rawentryalloc);
    if (!rawentry)
	ExitWithError(MEMORY_EXHAUSTED_ABORT);
    *rawentry = '\0';
    while (LYSafeGets(&LineBuf, fp) != 0) {
	LYTrimNewline(LineBuf);
	if (LineBuf[0] == '#' || LineBuf[0] == '\0')
	    continue;
	len = strlen(LineBuf);
	need = len + strlen(rawentry) + 1;
	if (need > rawentryalloc) {
	    rawentryalloc += (2000 + need);
	    rawentry = typeRealloc(char, rawentry, rawentryalloc);

	    if (!rawentry)
		ExitWithError(MEMORY_EXHAUSTED_ABORT);
	}
	if (len > 0 && LineBuf[len - 1] == '\\') {
	    LineBuf[len - 1] = '\0';
	    strcat(rawentry, LineBuf);
	} else {
	    strcat(rawentry, LineBuf);
	    break;
	}
    }
    FREE(LineBuf);

    t = s = LYSkipBlanks(rawentry);
    if (!*s) {
	/* totally blank entry -- quietly ignore */
	FREE(rawentry);
	return (0);
    }
    s = strchr(rawentry, ';');
    if (s == NULL) {
	CTrace((tfp,
		"ProcessMailcapEntry: Ignoring invalid mailcap entry: %s\n",
		rawentry));
	FREE(rawentry);
	return (0);
    }
    *s++ = '\0';
    if (!strncasecomp(t, "text/html", 9) ||
	!strncasecomp(t, "text/plain", 10)) {
	--s;
	*s = ';';
	CTrace((tfp, "ProcessMailcapEntry: Ignoring mailcap entry: %s\n",
		rawentry));
	FREE(rawentry);
	return (0);
    }
    LYRemoveBlanks(rawentry);
    LYLowerCase(rawentry);

    mc->needsterminal = 0;
    mc->copiousoutput = 0;
    mc->needtofree = 1;
    mc->testcommand = NULL;
    mc->label = NULL;
    mc->printcommand = NULL;
    mc->contenttype = NULL;
    StrAllocCopy(mc->contenttype, rawentry);
    mc->quality = (float) 1.0;
    mc->maxbytes = 0;
    t = GetCommand(s, &mc->command);
    if (!t) {
	goto assign_presentation;
    }
    s = LYSkipBlanks(t);
    while (s) {
	char *arg, *eq, *mallocd_string;

	t = GetCommand(s, &mallocd_string);
	arg = mallocd_string;
	eq = strchr(arg, '=');
	if (eq) {
	    *eq++ = '\0';
	    eq = LYSkipBlanks(eq);
	}
	if (non_empty(arg)) {
	    arg = Cleanse(arg);
	    if (!strcmp(arg, "needsterminal")) {
		mc->needsterminal = 1;
	    } else if (!strcmp(arg, "copiousoutput")) {
		mc->copiousoutput = 1;
	    } else if (eq && !strcmp(arg, "test")) {
		mc->testcommand = NULL;
		StrAllocCopy(mc->testcommand, eq);
		TrimCommand(mc->testcommand);
		CTrace((tfp, "ProcessMailcapEntry: Found testcommand:%s\n",
			mc->testcommand));
	    } else if (eq && !strcmp(arg, "description")) {
		mc->label = eq;	/* ignored */
	    } else if (eq && !strcmp(arg, "label")) {
		mc->label = eq;	/* ignored: bogus old name for description */
	    } else if (eq && !strcmp(arg, "print")) {
		mc->printcommand = eq;	/* ignored */
	    } else if (eq && !strcmp(arg, "textualnewlines")) {
		/* no support for now.  What does this do anyways? */
		/* ExceptionalNewline(mc->contenttype, atoi(eq)); */
	    } else if (eq && !strcmp(arg, "q")) {
		mc->quality = (float) atof(eq);
		if (mc->quality > 0.000 && mc->quality < 0.001)
		    mc->quality = (float) 0.001;
	    } else if (eq && !strcmp(arg, "mxb")) {
		mc->maxbytes = atol(eq);
		if (mc->maxbytes < 0)
		    mc->maxbytes = 0;
	    } else if (strcmp(arg, "notes")) {	/* IGNORE notes field */
		if (*arg)
		    CTrace((tfp,
			    "ProcessMailcapEntry: Ignoring mailcap flag '%s'.\n",
			    arg));
	    }

	}
	FREE(mallocd_string);
	s = t;
    }

  assign_presentation:
    FREE(rawentry);

    if (PassesTest(mc)) {
	CTrace((tfp, "ProcessMailcapEntry Setting up conversion %s : %s\n",
		mc->contenttype, mc->command));
	HTSetPresentation(mc->contenttype,
			  mc->command,
			  mc->testcommand,
			  mc->quality,
			  3.0, 0.0, mc->maxbytes, media);
    }
    FREE(mc->command);
    FREE(mc->testcommand);
    FREE(mc->contenttype);

    return (1);
}

#define L_CURL '{'
#define R_CURL '}'

static const char *LYSkipQuoted(const char *s)
{
    int escaped = 0;

    ++s;			/* skip first quote */
    while (*s != 0) {
	if (escaped) {
	    escaped = 0;
	} else if (*s == '\\') {
	    escaped = 1;
	} else if (*s == '"') {
	    ++s;
	    break;
	}
	++s;
    }
    return s;
}

/*
 * Note: the tspecials[] here are those defined for Content-Type header, so
 * this function is not really general-purpose.
 */
static const char *LYSkipToken(const char *s)
{
    static const char tspecials[] = "\"()<>@,;:\\/[]?.=";

    while (*s != '\0' && !WHITE(*s) && strchr(tspecials, *s) == 0) {
	++s;
    }
    return s;
}

static const char *LYSkipValue(const char *s)
{
    if (*s == '"')
	s = LYSkipQuoted(s);
    else
	s = LYSkipToken(s);
    return s;
}

/*
 * Copy the value from the source, dequoting if needed.
 */
static char *LYCopyValue(const char *s)
{
    const char *t;
    char *result = 0;
    int j, k;

    if (*s == '"') {
	t = LYSkipQuoted(s);
	StrAllocCopy(result, s + 1);
	result[t - s - 2] = '\0';
	for (j = k = 0;; ++j, ++k) {
	    if (result[j] == '\\') {
		++j;
	    }
	    if ((result[k] = result[j]) == '\0')
		break;
	}
    } else {
	t = LYSkipToken(s);
	StrAllocCopy(result, s);
	result[t - s] = '\0';
    }
    return result;
}

/*
 * The "Content-Type:" field, contains zero or more parameters after a ';'.
 * Return the value of the named parameter, or null.
 */
static char *LYGetContentType(const char *name,
			      const char *params)
{
    char *result = 0;

    if (params != 0) {
	if (name != 0) {
	    size_t length = strlen(name);
	    const char *test = strchr(params, ';');	/* skip type/subtype */
	    const char *next;

	    while (test != 0) {
		BOOL found = FALSE;

		++test;		/* skip the ';' */
		test = LYSkipCBlanks(test);
		next = LYSkipToken(test);
		if ((next - test) == (int) length
		    && !strncmp(test, name, length)) {
		    found = TRUE;
		}
		test = LYSkipCBlanks(next);
		if (*test == '=') {
		    ++test;
		    test = LYSkipCBlanks(test);
		    if (found) {
			result = LYCopyValue(test);
			break;
		    } else {
			test = LYSkipValue(test);
		    }
		    test = LYSkipCBlanks(test);
		}
		if (*test != ';') {
		    break;	/* we're lost */
		}
	    }
	} else {		/* return the content-type */
	    StrAllocCopy(result, params);
	    *LYSkipNonBlanks(result) = '\0';
	}
    }
    return result;
}

/*
 * Check if the command uses a "%s" substitution.  We need to know this, to
 * decide when to create temporary files, etc.
 */
BOOL LYMailcapUsesPctS(const char *controlstring)
{
    BOOL result = FALSE;
    const char *from;
    const char *next;
    int prefixed = 0;
    int escaped = 0;

    for (from = controlstring; *from != '\0'; from++) {
	if (escaped) {
	    escaped = 0;
	} else if (*from == '\\') {
	    escaped = 1;
	} else if (prefixed) {
	    prefixed = 0;
	    switch (*from) {
	    case '%':		/* not defined */
	    case 'n':
	    case 'F':
	    case 't':
		break;
	    case 's':
		result = TRUE;
		break;
	    case L_CURL:
		next = strchr(from, R_CURL);
		if (next != 0) {
		    from = next;
		    break;
		}
		/* FALLTHRU */
	    default:
		break;
	    }
	} else if (*from == '%') {
	    prefixed = 1;
	}
    }
    return result;
}

/*
 * Build the command string for testing or executing a mailcap entry.
 * If a substitution from the Content-Type header is requested but no
 * parameters are available, return -1, otherwise 0.
 *
 * This does not support multipart %n or %F (does this apply to lynx?)
 */
static int BuildCommand(HTChunk *cmd,
			const char *controlstring,
			const char *TmpFileName,
			const char *params)
{
    int result = 0;
    size_t TmpFileLen = strlen(TmpFileName);
    const char *from;
    const char *next;
    char *name, *value;
    int prefixed = 0;
    int escaped = 0;

    for (from = controlstring; *from != '\0'; from++) {
	if (escaped) {
	    escaped = 0;
	    HTChunkPutc(cmd, *from);
	} else if (*from == '\\') {
	    escaped = 1;
	} else if (prefixed) {
	    prefixed = 0;
	    switch (*from) {
	    case '%':		/* not defined */
		HTChunkPutc(cmd, *from);
		break;
	    case 'n':
		/* FALLTHRU */
	    case 'F':
		CTrace((tfp, "BuildCommand: Bad mailcap \"test\" clause: %s\n",
			controlstring));
		break;
	    case 't':
		if ((value = LYGetContentType(NULL, params)) != 0) {
		    HTChunkPuts(cmd, value);
		    FREE(value);
		}
		break;
	    case 's':
		if (TmpFileLen && TmpFileName) {
		    HTChunkPuts(cmd, TmpFileName);
		}
		break;
	    case L_CURL:
		next = strchr(from, R_CURL);
		if (next != 0) {
		    if (params != 0) {
			++from;
			name = 0;
			HTSprintf0(&name, "%.*s", (int) (next - from), from);
			if ((value = LYGetContentType(name, params)) != 0) {
			    HTChunkPuts(cmd, value);
			    FREE(value);
			} else {
			    if (!strcmp(name, "charset")) {
				HTChunkPuts(cmd, "ISO-8859-1");
			    } else {
				CTrace((tfp, "BuildCommand no value for %s\n", name));
			    }
			}
			FREE(name);
		    } else {
			result = -1;
		    }
		    from = next;
		    break;
		}
		/* FALLTHRU */
	    default:
		CTrace((tfp,
			"BuildCommand: Ignoring unrecognized format code in mailcap file '%%%c'.\n",
			*from));
		break;
	    }
	} else if (*from == '%') {
	    prefixed = 1;
	} else {
	    HTChunkPutc(cmd, *from);
	}
    }
    HTChunkTerminate(cmd);
    return result;
}

/*
 * Build the mailcap test-command and execute it.  This is only invoked when
 * we cannot tell just by looking at the command if it would succeed.
 *
 * Returns 0 for success, -1 for error and 1 for deferred.
 */
int LYTestMailcapCommand(const char *testcommand,
			 const char *params)
{
    int result;
    char TmpFileName[LY_MAXPATH];
    HTChunk *expanded = 0;

    if (LYMailcapUsesPctS(testcommand)) {
	if (LYOpenTemp(TmpFileName, HTML_SUFFIX, "w") == 0)
	    ExitWithError(CANNOT_OPEN_TEMP);
	LYCloseTemp(TmpFileName);
    } else {
	/* We normally don't need a temp file name - kw */
	TmpFileName[0] = '\0';
    }
    expanded = HTChunkCreate(1024);
    if ((result = BuildCommand(expanded, testcommand, TmpFileName, params)) != 0) {
	result = 1;
	CTrace((tfp, "PassesTest: Deferring test command: %s\n", expanded->data));
    } else {
	CTrace((tfp, "PassesTest: Executing test command: %s\n", expanded->data));
	if ((result = LYSystem(expanded->data)) != 0) {
	    result = -1;
	    CTrace((tfp, "PassesTest: Test failed!\n"));
	} else {
	    CTrace((tfp, "PassesTest: Test passed!\n"));
	}
    }

    HTChunkFree(expanded);
    LYRemoveTemp(TmpFileName);

    return result;
}

char *LYMakeMailcapCommand(const char *command,
			   const char *params,
			   const char *filename)
{
    HTChunk *expanded = 0;
    char *result = 0;

    expanded = HTChunkCreate(1024);
    BuildCommand(expanded, command, filename, params);
    StrAllocCopy(result, expanded->data);
    HTChunkFree(expanded);
    return result;
}

#define RTR_forget      0
#define RTR_lookup      1
#define RTR_add         2

static int RememberTestResult(int mode, char *cmd, int result)
{
    struct cmdlist_s {
	char *cmd;
	int result;
	struct cmdlist_s *next;
    };
    static struct cmdlist_s *cmdlist = NULL;
    struct cmdlist_s *cur;

    switch (mode) {
    case RTR_forget:
	while (cmdlist) {
	    cur = cmdlist->next;
	    FREE(cmdlist->cmd);
	    FREE(cmdlist);
	    cmdlist = cur;
	}
	break;
    case RTR_lookup:
	for (cur = cmdlist; cur; cur = cur->next)
	    if (!strcmp(cmd, cur->cmd))
		return cur->result;
	return -1;
    case RTR_add:
	cur = typecalloc(struct cmdlist_s);

	if (cur == NULL)
	    outofmem(__FILE__, "RememberTestResult");
	cur->next = cmdlist;
	StrAllocCopy(cur->cmd, cmd);
	cur->result = result;
	cmdlist = cur;
	break;
    }
    return 0;
}

/* FIXME: this sometimes used caseless comparison, e.g., strcasecomp */
#define SameCommand(tst,ref) !strcmp(tst,ref)

static int PassesTest(struct MailcapEntry *mc)
{
    int result;

    /*
     *  Make sure we have a command
     */
    if (!mc->testcommand)
	return (1);

    /*
     *  Save overhead of system() calls by faking these. - FM
     */
    if (SameCommand(mc->testcommand, "test \"$DISPLAY\"") ||
	SameCommand(mc->testcommand, "test \"$DISPLAY\" != \"\"") ||
	SameCommand(mc->testcommand, "test -n \"$DISPLAY\"")) {
	FREE(mc->testcommand);
	CTrace((tfp, "PassesTest: Testing for XWINDOWS environment.\n"));
	if (LYgetXDisplay() != NULL) {
	    CTrace((tfp, "PassesTest: Test passed!\n"));
	    return (0 == 0);
	} else {
	    CTrace((tfp, "PassesTest: Test failed!\n"));
	    return (-1 == 0);
	}
    }
    if (SameCommand(mc->testcommand, "test -z \"$DISPLAY\"")) {
	FREE(mc->testcommand);
	CTrace((tfp, "PassesTest: Testing for NON_XWINDOWS environment.\n"));
	if (LYgetXDisplay() == NULL) {
	    CTrace((tfp, "PassesTest: Test passed!\n"));
	    return (0 == 0);
	} else {
	    CTrace((tfp, "PassesTest: Test failed!\n"));
	    return (-1 == 0);
	}
    }

    /*
     *  Why do anything but return success for this one! - FM
     */
    if (SameCommand(mc->testcommand, "test -n \"$LYNX_VERSION\"")) {
	FREE(mc->testcommand);
	CTrace((tfp, "PassesTest: Testing for LYNX environment.\n"));
	CTrace((tfp, "PassesTest: Test passed!\n"));
	return (0 == 0);
    } else
	/*
	 *  ... or failure for this one! - FM
	 */
    if (SameCommand(mc->testcommand, "test -z \"$LYNX_VERSION\"")) {
	FREE(mc->testcommand);
	CTrace((tfp, "PassesTest: Testing for non-LYNX environment.\n"));
	CTrace((tfp, "PassesTest: Test failed!\n"));
	return (-1 == 0);
    }

    result = RememberTestResult(RTR_lookup, mc->testcommand, 0);
    if (result == -1) {
	result = LYTestMailcapCommand(mc->testcommand, NULL);
	RememberTestResult(RTR_add, mc->testcommand, result ? 1 : 0);
    }

    /*
     *  Free the test command as well since
     *  we wont be needing it anymore.
     */
    if (result != 1)
	FREE(mc->testcommand);

    if (result < 0) {
	CTrace((tfp, "PassesTest: Test failed!\n"));
    } else if (result == 0) {
	CTrace((tfp, "PassesTest: Test passed!\n"));
    }

    return (result >= 0);
}

static int ProcessMailcapFile(char *file, AcceptMedia media)
{
    struct MailcapEntry mc;
    FILE *fp;

    CTrace((tfp, "ProcessMailcapFile: Loading file '%s'.\n",
	    file));
    if ((fp = fopen(file, TXT_R)) == NULL) {
	CTrace((tfp, "ProcessMailcapFile: Could not open '%s'.\n",
		file));
	return (-1 == 0);
    }

    while (fp && !feof(fp)) {
	ProcessMailcapEntry(fp, &mc, media);
    }
    LYCloseInput(fp);
    RememberTestResult(RTR_forget, NULL, 0);
    return (0 == 0);
}

static int ExitWithError(const char *txt)
{
    if (txt)
	fprintf(tfp, "Lynx: %s\n", txt);
    exit_immediately(EXIT_FAILURE);
    return (-1);
}

/* Reverse the entries from each mailcap after it has been read, so that
 * earlier entries have precedence.  Set to 0 to get traditional lynx
 * behavior, which means that the last match wins. - kw */
static int reverse_mailcap = 1;

static int HTLoadTypesConfigFile(char *fn, AcceptMedia media)
{
    int result = 0;
    HTList *saved = HTPresentations;

    if (reverse_mailcap) {	/* temporarily hide existing list */
	HTPresentations = NULL;
    }

    result = ProcessMailcapFile(fn, media);

    if (reverse_mailcap) {
	if (result && HTPresentations) {
	    HTList_reverse(HTPresentations);
	    HTList_appendList(HTPresentations, saved);
	    FREE(saved);
	} else {
	    HTPresentations = saved;
	}
    }
    return result;
}

/* ------------------------------------------------------------------------ */
/* ------------------------------------------------------------------------ */
/* ------------------------------------------------------------------------ */

/*	Define a basic set of suffixes
 *	------------------------------
 *
 *	The LAST suffix for a type is that used for temporary files
 *	of that type.
 *	The quality is an apriori bias as to whether the file should be
 *	used.  Not that different suffixes can be used to represent files
 *	which are of the same format but are originals or regenerated,
 *	with different values.
 */
/*
 * Additional notes:  the encoding parameter may be taken into account when
 * looking for a match; for that purpose "7bit", "8bit", and "binary" are
 * equivalent.
 *
 * Use of mixed case and of pseudo MIME types with embedded spaces should be
 * avoided.  It was once necessary for getting the fancy strings into type
 * labels in FTP directory listings, but that can now be done with the
 * description field (using HTSetSuffix5).  AFAIK the only effect of such
 * "fancy" (and mostly invalid) types that cannot be reproduced by using a
 * description fields is some statusline messages in SaveToFile (HTFWriter.c). 
 * And showing the user an invalid MIME type as the 'Content-type:' is not such
 * a hot idea anyway, IMO.  Still, if you want it, it is still possible (even
 * in lynx.cfg now), but use of it in the defaults below has been reduced.
 *
 * Case variations rely on peculiar behavior of HTAtom.c for matching.  They
 * lead to surprising behavior, Lynx retains the case of a string in the form
 * first encountered after starting up.  So while later suffix rules generally
 * override or modify earlier ones, the case used for a MIME time is determined
 * by the first suffix rule (or other occurrence).  Matching in HTAtom_for is
 * effectively case insensitive, except for the first character of the string
 * which is treated as case-sensitive by the hash function there; best not to
 * rely on that, rather convert MIME types to lowercase on input as is already
 * done in most places (And HTAtom could become consistently case-sensitive, as
 * in newer W3C libwww).
 *  - kw 1999-10-12
 */
void HTFileInit(void)
{
#ifdef BUILTIN_SUFFIX_MAPS
    if (LYUseBuiltinSuffixes) {
	CTrace((tfp, "HTFileInit: Loading default (HTInit) extension maps.\n"));

	/* default suffix interpretation */
	SET_SUFFIX1("*", "text/plain", "8bit");
	SET_SUFFIX1("*.*", "text/plain", "8bit");

#ifdef EXEC_SCRIPTS
	/*
	 * define these extensions for exec scripts.
	 */
#ifndef VMS
	/* for csh exec links */
	HTSetSuffix(".csh", "application/x-csh", "8bit", 0.8);
	HTSetSuffix(".sh", "application/x-sh", "8bit", 0.8);
	HTSetSuffix(".ksh", "application/x-ksh", "8bit", 0.8);
#else
	HTSetSuffix(".com", "application/x-VMS_script", "8bit", 0.8);
#endif /* !VMS */
#endif /* EXEC_SCRIPTS */

	/*
	 * Some of the old incarnation of the mappings is preserved and can be had
	 * by defining TRADITIONAL_SUFFIXES.  This is for some cases where I felt
	 * the old rules might be preferred by someone, for some reason.  It's not
	 * done consistently.  A lot more of this stuff could probably be changed
	 * too or omitted, now that nearly the equivalent functionality is
	 * available in lynx.cfg.  - kw 1999-10-12
	 */
	/* *INDENT-OFF* */
	SET_SUFFIX1(".saveme",	"application/x-Binary",		"binary");
	SET_SUFFIX1(".dump",	"application/x-Binary",		"binary");
	SET_SUFFIX1(".bin",	"application/x-Binary",		"binary");

	SET_SUFFIX1(".arc",	"application/x-Compressed",	"binary");

	SET_SUFFIX1(".alpha-exe", "application/x-Executable",	"binary");
	SET_SUFFIX1(".alpha_exe", "application/x-Executable",	"binary");
	SET_SUFFIX1(".AXP-exe", "application/x-Executable",	"binary");
	SET_SUFFIX1(".AXP_exe", "application/x-Executable",	"binary");
	SET_SUFFIX1(".VAX-exe", "application/x-Executable",	"binary");
	SET_SUFFIX1(".VAX_exe", "application/x-Executable",	"binary");
	SET_SUFFIX5(".exe",	"application/octet-stream",	"binary", "Executable");

#ifdef TRADITIONAL_SUFFIXES
	SET_SUFFIX1(".exe.Z",	"application/x-Comp. Executable", "binary");
	SET_SUFFIX1(".Z",	"application/UNIX Compressed",	"binary");
	SET_SUFFIX1(".tar_Z",	"application/UNIX Compr. Tar",	"binary");
	SET_SUFFIX1(".tar.Z",	"application/UNIX Compr. Tar",	"binary");
#else
	SET_SUFFIX5(".Z",	"application/x-compress",	"binary", "UNIX Compressed");
	SET_SUFFIX5(".Z",	NULL,				"compress", "UNIX Compressed");
	SET_SUFFIX5(".exe.Z",	"application/octet-stream",	"compress", "Executable");
	SET_SUFFIX5(".tar_Z",	"application/x-tar",		"compress", "UNIX Compr. Tar");
	SET_SUFFIX5(".tar.Z",	"application/x-tar",		"compress", "UNIX Compr. Tar");
#endif

#ifdef TRADITIONAL_SUFFIXES
	SET_SUFFIX1("-gz",	"application/GNU Compressed",	"binary");
	SET_SUFFIX1("_gz",	"application/GNU Compressed",	"binary");
	SET_SUFFIX1(".gz",	"application/GNU Compressed",	"binary");

	SET_SUFFIX5(".tar.gz",	"application/x-tar",		"binary", "GNU Compr. Tar");
	SET_SUFFIX5(".tgz",	"application/x-tar",		"gzip", "GNU Compr. Tar");
#else
	SET_SUFFIX5("-gz",	"application/x-gzip",		"binary", "GNU Compressed");
	SET_SUFFIX5("_gz",	"application/x-gzip",		"binary", "GNU Compressed");
	SET_SUFFIX5(".gz",	"application/x-gzip",		"binary", "GNU Compressed");
	SET_SUFFIX5("-gz",	NULL,				"gzip", "GNU Compressed");
	SET_SUFFIX5("_gz",	NULL,				"gzip", "GNU Compressed");
	SET_SUFFIX5(".gz",	NULL,				"gzip", "GNU Compressed");

	SET_SUFFIX5(".tar.gz",	"application/x-tar",		"gzip", "GNU Compr. Tar");
	SET_SUFFIX5(".tgz",	"application/x-tar",		"gzip", "GNU Compr. Tar");
#endif

#ifdef TRADITIONAL_SUFFIXES
	SET_SUFFIX1(".src",	"application/x-WAIS-source",	"8bit");
	SET_SUFFIX1(".wsrc",	"application/x-WAIS-source",	"8bit");
#else
	SET_SUFFIX5(".wsrc",	"application/x-wais-source",	"8bit", "WAIS-source");
#endif

	SET_SUFFIX5(".zip",	"application/zip",		"binary", "Zip File");

	SET_SUFFIX1(".zz",	"application/x-deflate",	"binary");
	SET_SUFFIX1(".zz",	"application/deflate",		"binary");

	SET_SUFFIX1(".bz2",	"application/x-bzip2",		"binary");
	SET_SUFFIX1(".bz2",	"application/bzip2",		"binary");

#ifdef TRADITIONAL_SUFFIXES
	SET_SUFFIX1(".uu",	"application/x-UUencoded",	"8bit");

	SET_SUFFIX1(".hqx",	"application/x-Binhex",		"8bit");

	SET_SUFFIX1(".o",	"application/x-Prog. Object",	"binary");
	SET_SUFFIX1(".a",	"application/x-Prog. Library",	"binary");
#else
	SET_SUFFIX5(".uu",	"application/x-uuencoded",	"7bit", "UUencoded");

	SET_SUFFIX5(".hqx",	"application/mac-binhex40",	"8bit", "Mac BinHex");

	HTSetSuffix5(".o",	"application/octet-stream",	"binary", "Prog. Object", 0.5);
	HTSetSuffix5(".a",	"application/octet-stream",	"binary", "Prog. Library", 0.5);
	HTSetSuffix5(".so",	"application/octet-stream",	"binary", "Shared Lib", 0.5);
#endif

	SET_SUFFIX5(".oda",	"application/oda",		"binary", "ODA");

	SET_SUFFIX5(".pdf",	"application/pdf",		"binary", "PDF");

	SET_SUFFIX5(".eps",	"application/postscript",	"8bit", "Postscript");
	SET_SUFFIX5(".ai",	"application/postscript",	"8bit", "Postscript");
	SET_SUFFIX5(".ps",	"application/postscript",	"8bit", "Postscript");

	SET_SUFFIX5(".rtf",	"application/rtf",		"8bit", "RTF");

	SET_SUFFIX5(".dvi",	"application/x-dvi",		"8bit", "DVI");

	SET_SUFFIX5(".hdf",	"application/x-hdf",		"8bit", "HDF");

	SET_SUFFIX1(".cdf",	"application/x-netcdf",		"8bit");
	SET_SUFFIX1(".nc",	"application/x-netcdf",		"8bit");

#ifdef TRADITIONAL_SUFFIXES
	SET_SUFFIX1(".latex",	"application/x-Latex",		"8bit");
	SET_SUFFIX1(".tex",	"application/x-Tex",		"8bit");
	SET_SUFFIX1(".texinfo", "application/x-Texinfo",	"8bit");
	SET_SUFFIX1(".texi",	"application/x-Texinfo",	"8bit");
#else
	SET_SUFFIX5(".latex",	"application/x-latex",		"8bit", "LaTeX");
	SET_SUFFIX5(".tex",	"text/x-tex",			"8bit", "TeX");
	SET_SUFFIX5(".texinfo", "application/x-texinfo",	"8bit", "Texinfo");
	SET_SUFFIX5(".texi",	"application/x-texinfo",	"8bit", "Texinfo");
#endif

#ifdef TRADITIONAL_SUFFIXES
	SET_SUFFIX1(".t",	"application/x-Troff",		"8bit");
	SET_SUFFIX1(".tr",	"application/x-Troff",		"8bit");
	SET_SUFFIX1(".roff",	"application/x-Troff",		"8bit");

	SET_SUFFIX1(".man",	"application/x-Troff-man",	"8bit");
	SET_SUFFIX1(".me",	"application/x-Troff-me",	"8bit");
	SET_SUFFIX1(".ms",	"application/x-Troff-ms",	"8bit");
#else
	SET_SUFFIX5(".t",	"application/x-troff",		"8bit", "Troff");
	SET_SUFFIX5(".tr",	"application/x-troff",		"8bit", "Troff");
	SET_SUFFIX5(".roff",	"application/x-troff",		"8bit", "Troff");

	SET_SUFFIX5(".man",	"application/x-troff-man",	"8bit", "Man Page");
	SET_SUFFIX5(".me",	"application/x-troff-me",	"8bit", "Troff me");
	SET_SUFFIX5(".ms",	"application/x-troff-ms",	"8bit", "Troff ms");
#endif

	SET_SUFFIX1(".zoo",	"application/x-Zoo File",	"binary");

#if defined(TRADITIONAL_SUFFIXES) || defined(VMS)
	SET_SUFFIX1(".bak",	"application/x-VMS BAK File",	"binary");
	SET_SUFFIX1(".bkp",	"application/x-VMS BAK File",	"binary");
	SET_SUFFIX1(".bck",	"application/x-VMS BAK File",	"binary");

	SET_SUFFIX5(".bkp_gz",	"application/octet-stream",	"gzip", "GNU BAK File");
	SET_SUFFIX5(".bkp-gz",	"application/octet-stream",	"gzip", "GNU BAK File");
	SET_SUFFIX5(".bck_gz",	"application/octet-stream",	"gzip", "GNU BAK File");
	SET_SUFFIX5(".bck-gz",	"application/octet-stream",	"gzip", "GNU BAK File");

	SET_SUFFIX5(".bkp-Z",	"application/octet-stream",	"compress", "Comp. BAK File");
	SET_SUFFIX5(".bkp_Z",	"application/octet-stream",	"compress", "Comp. BAK File");
	SET_SUFFIX5(".bck-Z",	"application/octet-stream",	"compress", "Comp. BAK File");
	SET_SUFFIX5(".bck_Z",	"application/octet-stream",	"compress", "Comp. BAK File");
#else
	HTSetSuffix5(".bak",	NULL,				"binary", "Backup", 0.5);
	SET_SUFFIX5(".bkp",	"application/octet-stream",	"binary", "VMS BAK File");
	SET_SUFFIX5(".bck",	"application/octet-stream",	"binary", "VMS BAK File");
#endif

#if defined(TRADITIONAL_SUFFIXES) || defined(VMS)
	SET_SUFFIX1(".hlb",	"application/x-VMS Help Libr.", "binary");
	SET_SUFFIX1(".olb",	"application/x-VMS Obj. Libr.", "binary");
	SET_SUFFIX1(".tlb",	"application/x-VMS Text Libr.", "binary");
	SET_SUFFIX1(".obj",	"application/x-VMS Prog. Obj.", "binary");
	SET_SUFFIX1(".decw$book", "application/x-DEC BookReader", "binary");
	SET_SUFFIX1(".mem",	"application/x-RUNOFF-MANUAL", "8bit");
#else
	SET_SUFFIX5(".hlb",	"application/octet-stream",	"binary", "VMS Help Libr.");
	SET_SUFFIX5(".olb",	"application/octet-stream",	"binary", "VMS Obj. Libr.");
	SET_SUFFIX5(".tlb",	"application/octet-stream",	"binary", "VMS Text Libr.");
	SET_SUFFIX5(".obj",	"application/octet-stream",	"binary", "Prog. Object");
	SET_SUFFIX5(".decw$book", "application/octet-stream",	"binary", "DEC BookReader");
	SET_SUFFIX5(".mem",	"text/x-runoff-manual",		"8bit", "RUNOFF-MANUAL");
#endif

	SET_SUFFIX1(".vsd",	"application/visio",		"binary");

	SET_SUFFIX5(".lha",	"application/x-lha",		"binary", "lha File");
	SET_SUFFIX5(".lzh",	"application/x-lzh",		"binary", "lzh File");
	SET_SUFFIX5(".sea",	"application/x-sea",		"binary", "sea File");
#ifdef TRADITIONAL_SUFFIXES
	SET_SUFFIX5(".sit",	"application/x-sit",		"binary", "sit File");
#else
	SET_SUFFIX5(".sit",	"application/x-stuffit",	"binary", "StuffIt");
#endif
	SET_SUFFIX5(".dms",	"application/x-dms",		"binary", "dms File");
	SET_SUFFIX5(".iff",	"application/x-iff",		"binary", "iff File");

	SET_SUFFIX1(".bcpio",	"application/x-bcpio",		"binary");
	SET_SUFFIX1(".cpio",	"application/x-cpio",		"binary");

#ifdef TRADITIONAL_SUFFIXES
	SET_SUFFIX1(".gtar",	"application/x-gtar",		"binary");
#endif

	SET_SUFFIX1(".shar",	"application/x-shar",		"8bit");
	SET_SUFFIX1(".share",	"application/x-share",		"8bit");

#ifdef TRADITIONAL_SUFFIXES
	SET_SUFFIX1(".sh",	"application/x-sh",		"8bit"); /* xtra */
#endif

	SET_SUFFIX1(".sv4cpio", "application/x-sv4cpio",	"binary");
	SET_SUFFIX1(".sv4crc",	"application/x-sv4crc",		"binary");

	SET_SUFFIX5(".tar",	"application/x-tar",		"binary", "Tar File");
	SET_SUFFIX1(".ustar",	"application/x-ustar",		"binary");

	SET_SUFFIX1(".snd",	"audio/basic",			"binary");
	SET_SUFFIX1(".au",	"audio/basic",			"binary");

	SET_SUFFIX1(".aifc",	"audio/x-aiff",			"binary");
	SET_SUFFIX1(".aif",	"audio/x-aiff",			"binary");
	SET_SUFFIX1(".aiff",	"audio/x-aiff",			"binary");
	SET_SUFFIX1(".wav",	"audio/x-wav",			"binary");
	SET_SUFFIX1(".midi",	"audio/midi",			"binary");
	SET_SUFFIX1(".mod",	"audio/mod",			"binary");

	SET_SUFFIX1(".gif",	"image/gif",			"binary");
	SET_SUFFIX1(".ief",	"image/ief",			"binary");
	SET_SUFFIX1(".jfif",	"image/jpeg",			"binary"); /* xtra */
	SET_SUFFIX1(".jfif-tbnl", "image/jpeg",			"binary"); /* xtra */
	SET_SUFFIX1(".jpe",	"image/jpeg",			"binary");
	SET_SUFFIX1(".jpg",	"image/jpeg",			"binary");
	SET_SUFFIX1(".jpeg",	"image/jpeg",			"binary");
	SET_SUFFIX1(".tif",	"image/tiff",			"binary");
	SET_SUFFIX1(".tiff",	"image/tiff",			"binary");
	SET_SUFFIX1(".ham",	"image/ham",			"binary");
	SET_SUFFIX1(".ras",	"image/x-cmu-rast",		"binary");
	SET_SUFFIX1(".pnm",	"image/x-portable-anymap",	"binary");
	SET_SUFFIX1(".pbm",	"image/x-portable-bitmap",	"binary");
	SET_SUFFIX1(".pgm",	"image/x-portable-graymap",	"binary");
	SET_SUFFIX1(".ppm",	"image/x-portable-pixmap",	"binary");
	SET_SUFFIX1(".png",	"image/png",			"binary");
	SET_SUFFIX1(".rgb",	"image/x-rgb",			"binary");
	SET_SUFFIX1(".xbm",	"image/x-xbitmap",		"binary");
	SET_SUFFIX1(".xpm",	"image/x-xpixmap",		"binary");
	SET_SUFFIX1(".xwd",	"image/x-xwindowdump",		"binary");

	SET_SUFFIX1(".rtx",	"text/richtext",		"8bit");
	SET_SUFFIX1(".tsv",	"text/tab-separated-values",	"8bit");
	SET_SUFFIX1(".etx",	"text/x-setext",		"8bit");

	SET_SUFFIX1(".mpg",	"video/mpeg",			"binary");
	SET_SUFFIX1(".mpe",	"video/mpeg",			"binary");
	SET_SUFFIX1(".mpeg",	"video/mpeg",			"binary");
	SET_SUFFIX1(".mov",	"video/quicktime",		"binary");
	SET_SUFFIX1(".qt",	"video/quicktime",		"binary");
	SET_SUFFIX1(".avi",	"video/x-msvideo",		"binary");
	SET_SUFFIX1(".movie",	"video/x-sgi-movie",		"binary");
	SET_SUFFIX1(".mv",	"video/x-sgi-movie",		"binary");

	SET_SUFFIX1(".mime",	"message/rfc822",		"8bit");

	SET_SUFFIX1(".c",	"text/plain",			"8bit");
	SET_SUFFIX1(".cc",	"text/plain",			"8bit");
	SET_SUFFIX1(".c++",	"text/plain",			"8bit");
	SET_SUFFIX1(".css",	"text/plain",			"8bit");
	SET_SUFFIX1(".h",	"text/plain",			"8bit");
	SET_SUFFIX1(".pl",	"text/plain",			"8bit");
	SET_SUFFIX1(".text",	"text/plain",			"8bit");
	SET_SUFFIX1(".txt",	"text/plain",			"8bit");

	SET_SUFFIX1(".php",	"text/html",			"8bit");
	SET_SUFFIX1(".php3",	"text/html",			"8bit");
	SET_SUFFIX1(".html3",	"text/html",			"8bit");
	SET_SUFFIX1(".ht3",	"text/html",			"8bit");
	SET_SUFFIX1(".phtml",	"text/html",			"8bit");
	SET_SUFFIX1(".shtml",	"text/html",			"8bit");
	SET_SUFFIX1(".sht",	"text/html",			"8bit");
	SET_SUFFIX1(".htmlx",	"text/html",			"8bit");
	SET_SUFFIX1(".htm",	"text/html",			"8bit");
	SET_SUFFIX1(".html",	"text/html",			"8bit");
	/* *INDENT-ON* */

    } else {			/* LYSuffixRules */
	/*
	 * Note that even .html -> text/html, .htm -> text/html are omitted if
	 * default maps are compiled in but then skipped because of a
	 * configuration file directive.  Whoever changes the config file in
	 * this way can easily also add the SUFFIX rules there.  - kw
	 */
	CTrace((tfp,
		"HTFileInit: Skipping all default (HTInit) extension maps!\n"));
    }				/* LYSuffixRules */

#else /* BUILTIN_SUFFIX_MAPS */

    CTrace((tfp,
	    "HTFileInit: Default (HTInit) extension maps not compiled in.\n"));
    /*
     * The following two are still used if BUILTIN_SUFFIX_MAPS was undefined. 
     * Without one of them, lynx would always need to have a mapping specified
     * in a lynx.cfg or mime.types file to be usable for local HTML files at
     * all.  That includes many of the generated user interface pages.  - kw
     */
    SET_SUFFIX1(".htm", "text/html", "8bit");
    SET_SUFFIX1(".html", "text/html", "8bit");
#endif /* BUILTIN_SUFFIX_MAPS */

    if (LYisAbsPath(global_extension_map)) {
	/* These should override the default extensions as necessary. */
	HTLoadExtensionsConfigFile(global_extension_map);
    }

    /*
     * Load the local maps.
     */
    if (IsOurFile(LYAbsOrHomePath(&personal_extension_map))
	&& LYCanReadFile(personal_extension_map)) {
	/* These should override everything else. */
	HTLoadExtensionsConfigFile(personal_extension_map);
    }
}

/* -------------------- Extension config file reading --------------------- */

/*
 *  The following is lifted from NCSA httpd 1.0a1, by Rob McCool;
 *  NCSA httpd is in the public domain, as is this code.
 *
 *  Modified Oct 97 - KW
 */

#define MAX_STRING_LEN 256

static int HTGetLine(char *s, int n, FILE *f)
{
    register int i = 0, r;

    if (!f)
	return (1);

    while (1) {
	r = fgetc(f);
	s[i] = (char) r;

	if (s[i] == CR) {
	    r = fgetc(f);
	    if (r == LF)
		s[i] = (char) r;
	    else if (r != EOF)
		ungetc(r, f);
	}

	if ((r == EOF) || (s[i] == LF) || (s[i] == CR) || (i == (n - 1))) {
	    s[i] = '\0';
	    return (feof(f) ? 1 : 0);
	}
	++i;
    }
}

static void HTGetWord(char *word, char *line, char stop, char stop2)
{
    int x = 0, y;

    for (x = 0; line[x] && line[x] != stop && line[x] != stop2; x++) {
	word[x] = line[x];
    }

    word[x] = '\0';
    if (line[x])
	++x;
    y = 0;

    while ((line[y++] = line[x++])) ;

    return;
}

static int HTLoadExtensionsConfigFile(char *fn)
{
    char line[MAX_STRING_LEN];
    char word[MAX_STRING_LEN];
    char *ct;
    FILE *f;
    int count = 0;

    CTrace((tfp, "HTLoadExtensionsConfigFile: Loading file '%s'.\n", fn));

    if ((f = fopen(fn, TXT_R)) == NULL) {
	CTrace((tfp, "HTLoadExtensionsConfigFile: Could not open '%s'.\n", fn));
	return count;
    }

    while (!(HTGetLine(line, sizeof(line), f))) {
	HTGetWord(word, line, ' ', '\t');
	if (line[0] == '\0' || word[0] == '#')
	    continue;
	ct = NULL;
	StrAllocCopy(ct, word);
	LYLowerCase(ct);

	while (line[0]) {
	    HTGetWord(word, line, ' ', '\t');
	    if (word[0] && (word[0] != ' ')) {
		char *ext = NULL;

		HTSprintf0(&ext, ".%s", word);
		LYLowerCase(ext);

		CTrace((tfp, "setting suffix '%s' to '%s'.\n", ext, ct));

		if (strstr(ct, "tex") != NULL ||
		    strstr(ct, "postscript") != NULL ||
		    strstr(ct, "sh") != NULL ||
		    strstr(ct, "troff") != NULL ||
		    strstr(ct, "rtf") != NULL)
		    SET_SUFFIX1(ext, ct, "8bit");
		else
		    SET_SUFFIX1(ext, ct, "binary");
		count++;

		FREE(ext);
	    }
	}
	FREE(ct);
    }
    LYCloseInput(f);

    return count;
}
