/*		Configuration-specific Initialization		HTInit.c
**		----------------------------------------
*/

/*	Define a basic set of suffixes and presentations
**	------------------------------------------------
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

#include <HTSaveToFile.h>  /* LJM */
#include <LYStrings.h>
#include <LYUtils.h>
#include <LYGlobalDefs.h>

#include <LYexit.h>
#include <LYLeaks.h>

PRIVATE int HTLoadTypesConfigFile PARAMS((char *fn));
PRIVATE int HTLoadExtensionsConfigFile PARAMS((char *fn));

PUBLIC void HTFormatInit NOARGS
{
#ifdef NeXT
  HTSetPresentation("application/postscript",   "open %s", 1.0, 2.0, 0.0, 0);
  HTSetPresentation("image/x-tiff",             "open %s", 2.0, 2.0, 0.0, 0);
  HTSetPresentation("image/tiff",               "open %s", 1.0, 2.0, 0.0, 0);
  HTSetPresentation("audio/basic",              "open %s", 1.0, 2.0, 0.0, 0);
  HTSetPresentation("*",                        "open %s", 1.0, 0.0, 0.0, 0);
#else
 if (LYgetXDisplay() != 0) {	/* Must have X11 */
  HTSetPresentation("application/postscript", "ghostview %s&",
							    1.0, 3.0, 0.0, 0);
  if (XLoadImageCommand && *XLoadImageCommand) {
      HTSetPresentation("image/gif",	XLoadImageCommand,  1.0, 3.0, 0.0, 0);
      HTSetPresentation("image/x-xbm",	XLoadImageCommand,  1.0, 3.0, 0.0, 0);
      HTSetPresentation("image/x-xbitmap",XLoadImageCommand,1.0, 3.0, 0.0, 0);
      HTSetPresentation("image/x-png",	XLoadImageCommand,  2.0, 3.0, 0.0, 0);
      HTSetPresentation("image/png",	XLoadImageCommand,  1.0, 3.0, 0.0, 0);
      HTSetPresentation("image/x-rgb",	XLoadImageCommand,  1.0, 3.0, 0.0, 0);
      HTSetPresentation("image/x-tiff", XLoadImageCommand,  2.0, 3.0, 0.0, 0);
      HTSetPresentation("image/tiff",	XLoadImageCommand,  1.0, 3.0, 0.0, 0);
      HTSetPresentation("image/jpeg",	XLoadImageCommand,  1.0, 3.0, 0.0, 0);
  }
  HTSetPresentation("video/mpeg",       "mpeg_play %s &",   1.0, 3.0, 0.0, 0);

 }
#endif

#ifdef EXEC_SCRIPTS
 /* set quality to 999.0 for protected exec applications */
#ifndef VMS
 HTSetPresentation("application/x-csh", "csh %s", 999.0, 3.0, 0.0, 0);
 HTSetPresentation("application/x-sh",  "sh %s",  999.0, 3.0, 0.0, 0);
 HTSetPresentation("application/x-ksh", "ksh %s", 999.0, 3.0, 0.0, 0);
#else
 HTSetPresentation("application/x-VMS_script",	"@%s", 999.0, 3.0, 0.0, 0);
#endif /* not VMS */
#endif /* EXEC_SCRIPTS */

/*
 *  Add our header handlers.
 */
 HTSetConversion("message/x-http-redirection", "*",
					     HTMIMERedirect, 2.0, 0.0, 0.0, 0);
 HTSetConversion("message/x-http-redirection", "www/present",
					     HTMIMERedirect, 2.0, 0.0, 0.0, 0);
 HTSetConversion("message/x-http-redirection", "www/debug",
					     HTMIMERedirect, 1.0, 0.0, 0.0, 0);
 HTSetConversion("www/mime",  "www/present",  HTMIMEConvert, 1.0, 0.0, 0.0, 0);
 HTSetConversion("www/mime",  "www/download", HTMIMEConvert, 1.0, 0.0, 0.0, 0);
 HTSetConversion("www/mime",  "www/source",   HTMIMEConvert, 1.0, 0.0, 0.0, 0);
 HTSetConversion("www/mime",  "www/dump",     HTMIMEConvert, 1.0, 0.0, 0.0, 0);

/*
 *  Add our compressed file handlers.
 */
 HTSetConversion("www/compressed", "www/download",
					      HTCompressed,   1.0, 0.0, 0.0, 0);
 HTSetConversion("www/compressed", "www/present",
					      HTCompressed,   1.0, 0.0, 0.0, 0);
 HTSetConversion("www/compressed", "www/source",
					      HTCompressed,   1.0, 0.0, 0.0, 0);
 HTSetConversion("www/compressed", "www/dump",
					      HTCompressed,   1.0, 0.0, 0.0, 0);

 /*
  * Added the following to support some content types beginning to surface.
  */
 HTSetConversion("application/html", "text/x-c",
					HTMLToC,	0.5, 0.0, 0.0, 0);
 HTSetConversion("application/html", "text/plain",
					HTMLToPlain,	0.5, 0.0, 0.0, 0);
 HTSetConversion("application/html", "www/present",
					HTMLPresent,	2.0, 0.0, 0.0, 0);
 HTSetConversion("application/html", "www/source",
					HTPlainPresent,	1.0, 0.0, 0.0, 0);
 HTSetConversion("application/x-wais-source", "www/source",
					HTPlainPresent,	1.0, 0.0, 0.0, 0);
 HTSetConversion("application/x-wais-source", "www/present",
				        HTWSRCConvert,	2.0, 0.0, 0.0, 0);
 HTSetConversion("application/x-wais-source", "www/download",
					HTWSRCConvert,	1.0, 0.0, 0.0, 0);
 HTSetConversion("application/x-wais-source", "www/dump",
					HTWSRCConvert,	1.0, 0.0, 0.0, 0);

 /*
  *  Save all unknown mime types to disk.
  */
 HTSetConversion("www/source",  "www/present",
					HTSaveToFile,	1.0, 3.0, 0.0, 0);
 HTSetConversion("www/source",  "www/source",
					HTSaveToFile,	1.0, 3.0, 0.0, 0);
 HTSetConversion("www/source",  "www/download",
					HTSaveToFile,	1.0, 3.0, 0.0, 0);
 HTSetConversion("www/source",  "*",	HTSaveToFile,	1.0, 3.0, 0.0, 0);

 /*
  *  Output all www/dump presentations to stdout.
  */
 HTSetConversion("www/source",  "www/dump",
					HTDumpToStdout,	1.0, 3.0, 0.0, 0);

/*
 *  Now add our basic conversions.
 */
 HTSetConversion("text/x-sgml",
			      "www/source",  HTPlainPresent, 1.0, 0.0, 0.0, 0);
 HTSetConversion("text/x-sgml",
			      "www/present", HTMLPresent,    2.0, 0.0, 0.0, 0);
 HTSetConversion("text/sgml", "www/source",  HTPlainPresent, 1.0, 0.0, 0.0, 0);
 HTSetConversion("text/sgml", "www/present", HTMLPresent,    1.0, 0.0, 0.0, 0);
 HTSetConversion("text/plain","www/present", HTPlainPresent, 1.0, 0.0, 0.0, 0);
 HTSetConversion("text/plain","www/source",  HTPlainPresent, 1.0, 0.0, 0.0, 0);
 HTSetConversion("text/html", "www/source",  HTPlainPresent, 1.0, 0.0, 0.0, 0);
 HTSetConversion("text/html", "text/x-c",    HTMLToC,	     0.5, 0.0, 0.0, 0);
 HTSetConversion("text/html", "text/plain",  HTMLToPlain,    0.5, 0.0, 0.0, 0);
 HTSetConversion("text/html", "www/present", HTMLPresent,    1.0, 0.0, 0.0, 0);

 /*
  *  These should override the default types as necessary.
  */
 HTLoadTypesConfigFile(global_type_map);

 /*
  *  Load the local maps.
  */
 if (LYCanReadFile(personal_type_map)) {
     /* These should override everything else. */
     HTLoadTypesConfigFile(personal_type_map);
 } else {
     char buffer[LY_MAXPATH];
     LYAddPathToHome(buffer, sizeof(buffer), personal_type_map);
     HTLoadTypesConfigFile(buffer);
 }

 /*
  *  Put text/html and text/plain at beginning of list. - kw
  */
 HTReorderPresentation(WWW_PLAINTEXT, WWW_PRESENT);
 HTReorderPresentation(WWW_HTML, WWW_PRESENT);

 /*
  * Analyze the list, and set 'get_accept' for those whose representations
  * are not redundant.
  */
 HTFilterPresentations();
}

PUBLIC void HTPreparsedFormatInit NOARGS
{
 if (LYPreparsedSource) {
     HTSetConversion("text/html", "www/source", HTMLParsedPresent, 1.0, 0.0, 0.0, 0);
     HTSetConversion("text/html", "www/dump",	HTMLParsedPresent, 1.0, 0.0, 0.0, 0);
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
    float quality;
    long int maxbytes;
};


PRIVATE int ExitWithError PARAMS((char *txt));
PRIVATE int PassesTest PARAMS((struct MailcapEntry *mc));

#define LINE_BUF_SIZE		2048

PRIVATE char *GetCommand ARGS2(
	char *,		s,
	char **,	t)
{
    char *s2;
    int quoted = 0;

    s = LYSkipBlanks(s);
    /* marca -- added + 1 for error case -- oct 24, 1993. */
    s2 = malloc(strlen(s)*2 + 1); /* absolute max, if all % signs */
    if (!s2)
	ExitWithError(MEMORY_EXHAUSTED_ABORT);

    *t = s2;
    while (s && *s) {
	if (quoted) {
	    if (*s == '%') *s2++ = '%'; /* Quote through next level, ugh! */

	    *s2++ = *s++;
	    quoted = 0;
	} else {
	    if (*s == ';') {
		*s2 = '\0';
		return(++s);
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
    return(NULL);
}

/* no leading or trailing space, all lower case */
PRIVATE char *Cleanse ARGS1(
	char *,		s)
{
    LYTrimLeading(s);
    LYTrimTrailing(s);
    LYLowerCase(s);
    return(s);
}

PRIVATE int ProcessMailcapEntry ARGS2(
	FILE *,			fp,
	struct MailcapEntry *,	mc)
{
    size_t rawentryalloc = 2000, len, need;
    char *rawentry, *s, *t;
    char *LineBuf = NULL;

    rawentry = (char *)malloc(rawentryalloc);
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
	    rawentry = realloc(rawentry, rawentryalloc);
	    if (!rawentry)
	        ExitWithError(MEMORY_EXHAUSTED_ABORT);
	}
	if (len > 0 && LineBuf[len-1] == '\\') {
	    LineBuf[len-1] = '\0';
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
	return(0);
    }
    s = strchr(rawentry, ';');
    if (s == NULL) {
	CTRACE((tfp, "ProcessMailcapEntry: Ignoring invalid mailcap entry: %s\n",
		    rawentry));
	FREE(rawentry);
	return(0);
    }
    *s++ = '\0';
    if (!strncasecomp(t, "text/html", 9) ||
	!strncasecomp(t, "text/plain", 10)) {
	--s;
	*s = ';';
	CTRACE((tfp, "ProcessMailcapEntry: Ignoring mailcap entry: %s\n",
		    rawentry));
	FREE(rawentry);
	return(0);
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
	if (arg && *arg) {
	    arg = Cleanse(arg);
	    if (!strcmp(arg, "needsterminal")) {
		mc->needsterminal = 1;
	    } else if (!strcmp(arg, "copiousoutput")) {
		mc->copiousoutput = 1;
	    } else if (eq && !strcmp(arg, "test")) {
		mc->testcommand = NULL;
		StrAllocCopy(mc->testcommand, eq);
		CTRACE((tfp, "ProcessMailcapEntry: Found testcommand:%s\n",
			    mc->testcommand));
	    } else if (eq && !strcmp(arg, "description")) {
		mc->label = eq;
	    } else if (eq && !strcmp(arg, "label")) {
		mc->label = eq; /* bogus old name for description */
	    } else if (eq && !strcmp(arg, "print")) {
		mc->printcommand = eq;
	    } else if (eq && !strcmp(arg, "textualnewlines")) {
		/* no support for now.  What does this do anyways? */
		/* ExceptionalNewline(mc->contenttype, atoi(eq)); */
	    } else if (eq && !strcmp(arg, "q")) {
	        mc->quality = (float)atof(eq);
		if (mc->quality > 0.000 && mc->quality < 0.001)
		    mc->quality = (float) 0.001;
	    } else if (eq && !strcmp(arg, "mxb")) {
	        mc->maxbytes = atol(eq);
		if (mc->maxbytes < 0)
		    mc->maxbytes = 0;
	    } else if (strcmp(arg, "notes")) { /* IGNORE notes field */
		if (*arg)
		    CTRACE((tfp, "ProcessMailcapEntry: Ignoring mailcap flag '%s'.\n",
			        arg));
	    }

	}
	FREE(mallocd_string);
	s = t;
    }

assign_presentation:
    FREE(rawentry);

    if (PassesTest(mc)) {
	CTRACE((tfp, "ProcessMailcapEntry Setting up conversion %s : %s\n",
		    mc->contenttype, mc->command));
	HTSetPresentation(mc->contenttype, mc->command,
			  mc->quality, 3.0, 0.0, mc->maxbytes);
    }
    FREE(mc->command);
    FREE(mc->contenttype);

    return(1);
}

PRIVATE void BuildCommand ARGS5(
	char **,	pBuf,
	size_t,		Bufsize,
	char *,		controlstring,
	char *,		TmpFileName,
	size_t,		TmpFileLen)
{
    char *from, *to;
    int prefixed = 0;

    for (from = controlstring, to = *pBuf; *from != '\0'; from++) {
	if (prefixed) {
	    prefixed = 0;
	    switch(*from) {
		case '%':
		    *to++ = '%';
		    break;
		case 'n':
		    /* FALLTHRU */
		case 'F':
		    CTRACE((tfp, "BuildCommand: Bad mailcap \"test\" clause: %s\n",
				controlstring));
		    /* FALLTHRU */
		case 's':
		    if (TmpFileLen && TmpFileName) {
			if ((to - *pBuf) + TmpFileLen + 1 > Bufsize) {
			    *to = '\0';
			    CTRACE((tfp, "BuildCommand: Too long mailcap \"test\" clause,\n"));
			    CTRACE((tfp, "              ignoring: %s%s...\n",
					*pBuf, TmpFileName));
			    **pBuf = '\0';
			    return;
			}
			strcpy(to, TmpFileName);
			to += strlen(TmpFileName);
		    }
		    break;
		default:
		    CTRACE((tfp,
  "BuildCommand: Ignoring unrecognized format code in mailcap file '%%%c'.\n",
			*from));
		    break;
	    }
	} else if (*from == '%') {
	    prefixed = 1;
	} else {
	    *to++ = *from;
	}
	if (to >= *pBuf + Bufsize) {
	    (*pBuf)[Bufsize - 1] = '\0';
	    CTRACE((tfp, "BuildCommand: Too long mailcap \"test\" clause,\n"));
	    CTRACE((tfp, "              ignoring: %s...\n",
			*pBuf));
	    **pBuf = '\0';
	    return;
	}
    }
    *to = '\0';
}

#define RTR_forget      0
#define RTR_lookup      1
#define RTR_add         2

PRIVATE int RememberTestResult ARGS3(
	int,		mode,
	char *,		cmd,
	int,		result)
{
    struct cmdlist_s {
	char *cmd;
	int result;
	struct cmdlist_s *next;
    };
    static struct cmdlist_s *cmdlist = NULL;
    struct cmdlist_s *cur;

    switch(mode) {
	case RTR_forget:
	    while(cmdlist) {
		cur = cmdlist->next;
		FREE(cmdlist->cmd);
		FREE(cmdlist);
		cmdlist = cur;
	    }
	    break;
	case RTR_lookup:
	    for(cur = cmdlist; cur; cur = cur->next)
		if(!strcmp(cmd, cur->cmd))
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

PRIVATE int PassesTest ARGS1(
	struct MailcapEntry *,	mc)
{
    int result;
    char *cmd, TmpFileName[LY_MAXPATH];

    /*
     *  Make sure we have a command
     */
    if (!mc->testcommand)
	return(1);

    /*
     *  Save overhead of system() calls by faking these. - FM
     */
    if (0 == strcmp(mc->testcommand, "test \"$DISPLAY\"") ||
	0 == strcmp(mc->testcommand, "test \"$DISPLAY\" != \"\"") ||
	0 == strcasecomp(mc->testcommand, "test -n \"$DISPLAY\"")) {
	FREE(mc->testcommand);
	CTRACE((tfp, "PassesTest: Testing for XWINDOWS environment.\n"));
	if (LYgetXDisplay() != NULL) {
	    CTRACE((tfp, "PassesTest: Test passed!\n"));
	    return(0 == 0);
	} else {
	    CTRACE((tfp, "PassesTest: Test failed!\n"));
	    return(-1 == 0);
	}
    }
    if (0 == strcasecomp(mc->testcommand, "test -z \"$DISPLAY\"")) {
	FREE(mc->testcommand);
	CTRACE((tfp, "PassesTest: Testing for NON_XWINDOWS environment.\n"));
	if (LYgetXDisplay() == NULL) {
	    CTRACE((tfp,"PassesTest: Test passed!\n"));
	    return(0 == 0);
	} else {
	    CTRACE((tfp,"PassesTest: Test failed!\n"));
	    return(-1 == 0);
	}
    }

    /*
     *  Why do anything but return success for this one! - FM
     */
    if (0 == strcasecomp(mc->testcommand, "test -n \"$LYNX_VERSION\"")){
	FREE(mc->testcommand);
	CTRACE((tfp, "PassesTest: Testing for LYNX environment.\n"));
	CTRACE((tfp, "PassesTest: Test passed!\n"));
	return(0 == 0);
    } else
    /*
     *  ... or failure for this one! - FM
     */
    if (0 == strcasecomp(mc->testcommand, "test -z \"$LYNX_VERSION\"")) {
	FREE(mc->testcommand);
	CTRACE((tfp, "PassesTest: Testing for non-LYNX environment.\n"));
	CTRACE((tfp, "PassesTest: Test failed!\n"));
	return(-1 == 0);
    }

    result = RememberTestResult(RTR_lookup, mc->testcommand, 0);
    if(result == -1) {
	/*
	 *  Build the command and execute it.
	 */
	if (strchr(mc->testcommand, '%')) {
	    if (LYOpenTemp(TmpFileName, HTML_SUFFIX, "w") == 0)
		ExitWithError(CANNOT_OPEN_TEMP);
	    LYCloseTemp(TmpFileName);
	} else {
	    /* We normally don't need a temp file name - kw */
	    TmpFileName[0] = '\0';
	}
	cmd = (char *)malloc(1024);
	if (!cmd)
	    ExitWithError(MEMORY_EXHAUSTED_ABORT);
	BuildCommand(&cmd, 1024,
		     mc->testcommand,
		     TmpFileName,
		     strlen(TmpFileName));
	CTRACE((tfp, "PassesTest: Executing test command: %s\n", cmd));
	result = LYSystem(cmd);
	FREE(cmd);
	LYRemoveTemp(TmpFileName);
	RememberTestResult(RTR_add, mc->testcommand, result ? 1 : 0);
    }

    /*
     *  Free the test command as well since
     *  we wont be needing it anymore.
     */
    FREE(mc->testcommand);

    if (result) {
	CTRACE((tfp,"PassesTest: Test failed!\n"));
    } else {
	CTRACE((tfp,"PassesTest: Test passed!\n"));
    }

    return(result == 0);
}

PRIVATE int ProcessMailcapFile ARGS1(
	char *,		file)
{
    struct MailcapEntry mc;
    FILE *fp;

    CTRACE((tfp, "ProcessMailcapFile: Loading file '%s'.\n",
		file));
    if ((fp = fopen(file, TXT_R)) == NULL) {
	CTRACE((tfp, "ProcessMailcapFile: Could not open '%s'.\n",
		    file));
	return(-1 == 0);
    }

    while (fp && !feof(fp)) {
	ProcessMailcapEntry(fp, &mc);
    }
    LYCloseInput(fp);
    RememberTestResult(RTR_forget, NULL, 0);
    return(0 == 0);
}

PRIVATE int ExitWithError ARGS1(
	char *,		txt)
{
    if (txt)
	fprintf(tfp, "Lynx: %s\n", txt);
    exit_immediately(EXIT_FAILURE);
    return(-1);
}

/* Reverse the entries from each mailcap after it has been read, so that
 * earlier entries have precedence.  Set to 0 to get traditional lynx
 * behavior, which means that the last match wins. - kw */
static int reverse_mailcap = 1;

PRIVATE int HTLoadTypesConfigFile ARGS1(
	char *,		fn)
{
    int result = 0;
    HTList * saved = HTPresentations;

    if (reverse_mailcap) {		/* temporarily hide existing list */
	HTPresentations = NULL;
    }

    result = ProcessMailcapFile(fn);

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
**	------------------------------
**
**	The LAST suffix for a type is that used for temporary files
**	of that type.
**	The quality is an apriori bias as to whether the file should be
**	used.  Not that different suffixes can be used to represent files
**	which are of the same format but are originals or regenerated,
**	with different values.
*/
/*
 *  Additional notes: the encoding parameter may be taken into account when
 *  looking for a match; for that purpose "7bit", "8bit", and "binary" are
 *  equivalent.
 *  Use of mixed case and of pseudo MIME types with embedded spaces should
 *  be avoided.  It was once necessary for getting the fancy strings into
 *  type labels in FTP directory listings, but that can now be done with
 *  the description field (using HTSetSuffix5).  AFAIK the only effect of
 *  such "fancy" (and mostly invalid) types that cannot be reproduced by
 *  using a description fields is some statusline messages in SaveToFile
 *  (HTFWriter.c).  And showing the user an invalid MIME type as the
 *  'Content-type:' is not such a hot idea anyway, IMO.  Still, if you
 *  want it, it is still possible (even in lynx.cfg now), but use of it
 *  in the defaults below has been reduced.
 *  Case variations rely on peculiar behavior of HTAtom.c for matching.
 *  They lead to surprising behavior, Lynx retains the case of a string
 *  in the form first encountered after starting up.  So while later suffix
 *  rules generally override or modify earlier ones, the case used for a
 *  MIME time is determined by the first suffix rule (or other occurrence).
 *  Matching in HTAtom_for is effectively case insensitive, except for the
 *  first character of the string which is treated as case-sensitive by the
 *  hash function there; best not to rely on that, rather convert MIME types
 *  to lowercase on input as is already done in most places (And HTAtom could
 *  become consistently case-sensitive, as in newer W3C libwww).
 *  - kw 1999-10-12
 */
PUBLIC void HTFileInit NOARGS
{
#ifdef BUILTIN_SUFFIX_MAPS
    if (LYUseBuiltinSuffixes)
    {
    CTRACE((tfp, "HTFileInit: Loading default (HTInit) extension maps.\n"));

    /* default suffix interpretation */
    HTSetSuffix("*",		"text/plain", "8bit", 1.0);
    HTSetSuffix("*.*",		"text/plain", "8bit", 1.0);


#ifdef EXEC_SCRIPTS
    /*
     *  define these extensions for exec scripts.
     */
#ifndef VMS
    /* for csh exec links */
    HTSetSuffix(".csh",		"application/x-csh", "8bit", 0.8);
    HTSetSuffix(".sh",		"application/x-sh", "8bit", 0.8);
    HTSetSuffix(".ksh",		"application/x-ksh", "8bit", 0.8);
#else
    HTSetSuffix(".com",		"application/x-VMS_script", "8bit", 0.8);
#endif /* !VMS */
#endif /* EXEC_SCRIPTS */

    /*
     *  Some of the old incarnation of the mappings is preserved
     *  and can be had by defining TRADITIONAL_SUFFIXES.  This
     *  is for some cases where I felt the old rules might be preferred
     *  by someone, for some reason.  It's not done consistently.
     *  A lot more of this stuff could probably be changed too or
     *  omitted, now that nearly the equivalent functionality is
     *  available in lynx.cfg. - kw 1999-10-12
     */
    HTSetSuffix(".saveme",	"application/x-Binary", "binary", 1.0);
    HTSetSuffix(".dump",	"application/x-Binary", "binary", 1.0);
    HTSetSuffix(".bin",		"application/x-Binary", "binary", 1.0);

    HTSetSuffix(".arc",		"application/x-Compressed", "binary", 1.0);

    HTSetSuffix(".alpha-exe",	"application/x-Executable", "binary", 1.0);
    HTSetSuffix(".alpha_exe",	"application/x-Executable", "binary", 1.0);
    HTSetSuffix(".AXP-exe",	"application/x-Executable", "binary", 1.0);
    HTSetSuffix(".AXP_exe",	"application/x-Executable", "binary", 1.0);
    HTSetSuffix(".VAX-exe",	"application/x-Executable", "binary", 1.0);
    HTSetSuffix(".VAX_exe",	"application/x-Executable", "binary", 1.0);
    HTSetSuffix5(".exe",	"application/octet-stream", "binary", "Executable", 1.0);

#ifdef TRADITIONAL_SUFFIXES
    HTSetSuffix(".exe.Z",	"application/x-Comp. Executable",
							     "binary", 1.0);
    HTSetSuffix(".Z",	        "application/UNIX Compressed", "binary", 1.0);
    HTSetSuffix(".tar_Z",	"application/UNIX Compr. Tar", "binary", 1.0);
    HTSetSuffix(".tar.Z",	"application/UNIX Compr. Tar", "binary", 1.0);
#else
    HTSetSuffix5(".Z",	        "application/x-compress", "binary", "UNIX Compressed", 1.0);
    HTSetSuffix5(".Z",	        NULL, "compress",      "UNIX Compressed", 1.0);
    HTSetSuffix5(".exe.Z",	"application/octet-stream", "compress",
						       "Executable", 1.0);
    HTSetSuffix5(".tar_Z",	"application/x-tar", "compress",
						       "UNIX Compr. Tar", 1.0);
    HTSetSuffix5(".tar.Z",	"application/x-tar", "compress",
						       "UNIX Compr. Tar", 1.0);
#endif

#ifdef TRADITIONAL_SUFFIXES
    HTSetSuffix("-gz",		"application/GNU Compressed", "binary", 1.0);
    HTSetSuffix("_gz",		"application/GNU Compressed", "binary", 1.0);
    HTSetSuffix(".gz",		"application/GNU Compressed", "binary", 1.0);

    HTSetSuffix5(".tar.gz",	"application/x-tar", "binary", "GNU Compr. Tar", 1.0);
    HTSetSuffix5(".tgz",	"application/x-tar", "gzip", "GNU Compr. Tar", 1.0);
#else
    HTSetSuffix5("-gz",		"application/x-gzip", "binary", "GNU Compressed", 1.0);
    HTSetSuffix5("_gz",		"application/x-gzip", "binary", "GNU Compressed", 1.0);
    HTSetSuffix5(".gz",		"application/x-gzip", "binary", "GNU Compressed", 1.0);
    HTSetSuffix5("-gz",		NULL, "gzip", "GNU Compressed", 1.0);
    HTSetSuffix5("_gz",		NULL, "gzip", "GNU Compressed", 1.0);
    HTSetSuffix5(".gz",		NULL, "gzip", "GNU Compressed", 1.0);

    HTSetSuffix5(".tar.gz",	"application/x-tar", "gzip", "GNU Compr. Tar", 1.0);
    HTSetSuffix5(".tgz",	"application/x-tar", "gzip", "GNU Compr. Tar", 1.0);
#endif

#ifdef TRADITIONAL_SUFFIXES
    HTSetSuffix(".src",		"application/x-WAIS-source", "8bit", 1.0);
    HTSetSuffix(".wsrc",	"application/x-WAIS-source", "8bit", 1.0);
#else
    HTSetSuffix5(".wsrc",	"application/x-wais-source", "8bit", "WAIS-source", 1.0);
#endif

    HTSetSuffix5(".zip",	"application/zip", "binary", "Zip File", 1.0);

    HTSetSuffix(".bz2",		"application/x-bzip2", "binary", 1.0);

    HTSetSuffix(".bz2",		"application/bzip2", "binary", 1.0);

#ifdef TRADITIONAL_SUFFIXES
    HTSetSuffix(".uu",		"application/x-UUencoded", "8bit", 1.0);

    HTSetSuffix(".hqx",		"application/x-Binhex", "8bit", 1.0);

    HTSetSuffix(".o",		"application/x-Prog. Object", "binary", 1.0);
    HTSetSuffix(".a",		"application/x-Prog. Library", "binary", 1.0);
#else
    HTSetSuffix5(".uu",		"application/x-uuencoded", "7bit", "UUencoded", 1.0);

    HTSetSuffix5(".hqx",	"application/mac-binhex40", "8bit", "Mac BinHex", 1.0);

    HTSetSuffix5(".o",		"application/octet-stream", "binary", "Prog. Object", 0.5);
    HTSetSuffix5(".a",		"application/octet-stream", "binary", "Prog. Library", 0.5);
    HTSetSuffix5(".so",		"application/octet-stream", "binary", "Shared Lib", 0.5);
#endif

    HTSetSuffix5(".oda",	"application/oda", "binary", "ODA", 1.0);

    HTSetSuffix5(".pdf",	"application/pdf", "binary", "PDF", 1.0);

    HTSetSuffix5(".eps",	"application/postscript", "8bit", "Postscript", 1.0);
    HTSetSuffix5(".ai",		"application/postscript", "8bit", "Postscript", 1.0);
    HTSetSuffix5(".ps",		"application/postscript", "8bit", "Postscript", 1.0);

    HTSetSuffix5(".rtf",	"application/rtf", "8bit", "RTF", 1.0);

    HTSetSuffix5(".dvi",	"application/x-dvi", "8bit", "DVI", 1.0);

    HTSetSuffix5(".hdf",	"application/x-hdf", "8bit", "HDF", 1.0);

    HTSetSuffix(".cdf",		"application/x-netcdf", "8bit", 1.0);
    HTSetSuffix(".nc",		"application/x-netcdf", "8bit", 1.0);

#ifdef TRADITIONAL_SUFFIXES
    HTSetSuffix(".latex",	"application/x-Latex", "8bit", 1.0);
    HTSetSuffix(".tex",		"application/x-Tex", "8bit", 1.0);
    HTSetSuffix(".texinfo",	"application/x-Texinfo", "8bit", 1.0);
    HTSetSuffix(".texi",	"application/x-Texinfo", "8bit", 1.0);
#else
    HTSetSuffix5(".latex",	"application/x-latex", "8bit", "LaTeX", 1.0);
    HTSetSuffix5(".tex",	"text/x-tex", "8bit", "TeX", 1.0);
    HTSetSuffix5(".texinfo",	"application/x-texinfo", "8bit", "Texinfo", 1.0);
    HTSetSuffix5(".texi",	"application/x-texinfo", "8bit", "Texinfo", 1.0);
#endif

#ifdef TRADITIONAL_SUFFIXES
    HTSetSuffix(".t",		"application/x-Troff", "8bit", 1.0);
    HTSetSuffix(".tr",		"application/x-Troff", "8bit", 1.0);
    HTSetSuffix(".roff",	"application/x-Troff", "8bit", 1.0);

    HTSetSuffix(".man",		"application/x-Troff-man", "8bit", 1.0);
    HTSetSuffix(".me",		"application/x-Troff-me", "8bit", 1.0);
    HTSetSuffix(".ms",		"application/x-Troff-ms", "8bit", 1.0);
#else
    HTSetSuffix5(".t",		"application/x-troff", "8bit", "Troff", 1.0);
    HTSetSuffix5(".tr",		"application/x-troff", "8bit", "Troff", 1.0);
    HTSetSuffix5(".roff",	"application/x-troff", "8bit", "Troff", 1.0);

    HTSetSuffix5(".man",	"application/x-troff-man", "8bit", "Man Page", 1.0);
    HTSetSuffix5(".me",		"application/x-troff-me", "8bit", "Troff me", 1.0);
    HTSetSuffix5(".ms",		"application/x-troff-ms", "8bit", "Troff ms", 1.0);
#endif

    HTSetSuffix(".zoo",		"application/x-Zoo File", "binary", 1.0);

#if defined(TRADITIONAL_SUFFIXES) || defined(VMS)
    HTSetSuffix(".bak",		"application/x-VMS BAK File", "binary", 1.0);
    HTSetSuffix(".bkp",		"application/x-VMS BAK File", "binary", 1.0);
    HTSetSuffix(".bck",		"application/x-VMS BAK File", "binary", 1.0);

    HTSetSuffix5(".bkp_gz",	"application/octet-stream", "gzip", "GNU BAK File", 1.0);
    HTSetSuffix5(".bkp-gz",	"application/octet-stream", "gzip", "GNU BAK File", 1.0);
    HTSetSuffix5(".bck_gz",	"application/octet-stream", "gzip", "GNU BAK File", 1.0);
    HTSetSuffix5(".bck-gz",	"application/octet-stream", "gzip", "GNU BAK File", 1.0);

    HTSetSuffix5(".bkp-Z",	"application/octet-stream", "compress", "Comp. BAK File", 1.0);
    HTSetSuffix5(".bkp_Z",	"application/octet-stream", "compress", "Comp. BAK File", 1.0);
    HTSetSuffix5(".bck-Z",	"application/octet-stream", "compress", "Comp. BAK File", 1.0);
    HTSetSuffix5(".bck_Z",	"application/octet-stream", "compress", "Comp. BAK File", 1.0);
#else
    HTSetSuffix5(".bak",	NULL, "binary", "Backup", 0.5);
    HTSetSuffix5(".bkp",	"application/octet-stream", "binary", "VMS BAK File", 1.0);
    HTSetSuffix5(".bck",	"application/octet-stream", "binary", "VMS BAK File", 1.0);
#endif

#if defined(TRADITIONAL_SUFFIXES) || defined(VMS)
    HTSetSuffix(".hlb",		"application/x-VMS Help Libr.", "binary", 1.0);
    HTSetSuffix(".olb",		"application/x-VMS Obj. Libr.", "binary", 1.0);
    HTSetSuffix(".tlb",		"application/x-VMS Text Libr.", "binary", 1.0);
    HTSetSuffix(".obj",		"application/x-VMS Prog. Obj.", "binary", 1.0);
    HTSetSuffix(".decw$book",	"application/x-DEC BookReader", "binary", 1.0);
    HTSetSuffix(".mem",		"application/x-RUNOFF-MANUAL", "8bit", 1.0);
#else
    HTSetSuffix5(".hlb",	"application/octet-stream", "binary", "VMS Help Libr.", 1.0);
    HTSetSuffix5(".olb",	"application/octet-stream", "binary", "VMS Obj. Libr.", 1.0);
    HTSetSuffix5(".tlb",	"application/octet-stream", "binary", "VMS Text Libr.", 1.0);
    HTSetSuffix5(".obj",	"application/octet-stream", "binary", "Prog. Object", 1.0);
    HTSetSuffix5(".decw$book",	"application/octet-stream", "binary", "DEC BookReader", 1.0);
    HTSetSuffix5(".mem",	"text/x-runoff-manual", "8bit", "RUNOFF-MANUAL", 1.0);
#endif

    HTSetSuffix(".vsd",		"application/visio", "binary", 1.0);

    HTSetSuffix5(".lha",	"application/x-lha", "binary", "lha File", 1.0);
    HTSetSuffix5(".lzh",	"application/x-lzh", "binary", "lzh File", 1.0);
    HTSetSuffix5(".sea",	"application/x-sea", "binary", "sea File", 1.0);
#ifdef TRADITIONAL_SUFFIXES
    HTSetSuffix5(".sit",	"application/x-sit", "binary", "sit File", 1.0);
#else
    HTSetSuffix5(".sit",	"application/x-stuffit", "binary", "StuffIt", 1.0);
#endif
    HTSetSuffix5(".dms",	"application/x-dms", "binary", "dms File", 1.0);
    HTSetSuffix5(".iff",	"application/x-iff", "binary", "iff File", 1.0);

    HTSetSuffix(".bcpio",	"application/x-bcpio", "binary", 1.0);
    HTSetSuffix(".cpio",	"application/x-cpio", "binary", 1.0);

#ifdef TRADITIONAL_SUFFIXES
    HTSetSuffix(".gtar",	"application/x-gtar", "binary", 1.0);
#endif

    HTSetSuffix(".shar",	"application/x-shar", "8bit", 1.0);
    HTSetSuffix(".share",	"application/x-share", "8bit", 1.0);

#ifdef TRADITIONAL_SUFFIXES
    HTSetSuffix(".sh",		"application/x-sh", "8bit", 1.0); /* xtra */
#endif

    HTSetSuffix(".sv4cpio",	"application/x-sv4cpio", "binary", 1.0);
    HTSetSuffix(".sv4crc",	"application/x-sv4crc", "binary", 1.0);

    HTSetSuffix5(".tar",	"application/x-tar", "binary", "Tar File", 1.0);
    HTSetSuffix(".ustar",	"application/x-ustar", "binary", 1.0);

    HTSetSuffix(".snd",		"audio/basic", "binary", 1.0);
    HTSetSuffix(".au",		"audio/basic", "binary", 1.0);

    HTSetSuffix(".aifc",	"audio/x-aiff", "binary", 1.0);
    HTSetSuffix(".aif",		"audio/x-aiff", "binary", 1.0);
    HTSetSuffix(".aiff",	"audio/x-aiff", "binary", 1.0);
    HTSetSuffix(".wav",		"audio/x-wav", "binary", 1.0);
    HTSetSuffix(".midi",	"audio/midi", "binary", 1.0);
    HTSetSuffix(".mod",		"audio/mod", "binary", 1.0);

    HTSetSuffix(".gif",		"image/gif", "binary", 1.0);
    HTSetSuffix(".ief",		"image/ief", "binary", 1.0);
    HTSetSuffix(".jfif",	"image/jpeg", "binary", 1.0); /* xtra */
    HTSetSuffix(".jfif-tbnl",	"image/jpeg", "binary", 1.0); /* xtra */
    HTSetSuffix(".jpe",		"image/jpeg", "binary", 1.0);
    HTSetSuffix(".jpg",		"image/jpeg", "binary", 1.0);
    HTSetSuffix(".jpeg",	"image/jpeg", "binary", 1.0);
    HTSetSuffix(".tif",		"image/tiff", "binary", 1.0);
    HTSetSuffix(".tiff",	"image/tiff", "binary", 1.0);
    HTSetSuffix(".ham",		"image/ham", "binary", 1.0);
    HTSetSuffix(".ras",		"image/x-cmu-rast", "binary", 1.0);
    HTSetSuffix(".pnm",		"image/x-portable-anymap", "binary", 1.0);
    HTSetSuffix(".pbm",		"image/x-portable-bitmap", "binary", 1.0);
    HTSetSuffix(".pgm",		"image/x-portable-graymap", "binary", 1.0);
    HTSetSuffix(".ppm",		"image/x-portable-pixmap", "binary", 1.0);
    HTSetSuffix(".png",		"image/png", "binary", 1.0);
    HTSetSuffix(".rgb",		"image/x-rgb", "binary", 1.0);
    HTSetSuffix(".xbm",		"image/x-xbitmap", "binary", 1.0);
    HTSetSuffix(".xpm",		"image/x-xpixmap", "binary", 1.0);
    HTSetSuffix(".xwd",		"image/x-xwindowdump", "binary", 1.0);

    HTSetSuffix(".rtx",		"text/richtext", "8bit", 1.0);
    HTSetSuffix(".tsv",		"text/tab-separated-values", "8bit", 1.0);
    HTSetSuffix(".etx",		"text/x-setext", "8bit", 1.0);

    HTSetSuffix(".mpg",		"video/mpeg", "binary", 1.0);
    HTSetSuffix(".mpe",		"video/mpeg", "binary", 1.0);
    HTSetSuffix(".mpeg",	"video/mpeg", "binary", 1.0);
    HTSetSuffix(".mov",		"video/quicktime", "binary", 1.0);
    HTSetSuffix(".qt",		"video/quicktime", "binary", 1.0);
    HTSetSuffix(".avi",		"video/x-msvideo", "binary", 1.0);
    HTSetSuffix(".movie",	"video/x-sgi-movie", "binary", 1.0);
    HTSetSuffix(".mv",		"video/x-sgi-movie", "binary", 1.0);

    HTSetSuffix(".mime",	"message/rfc822", "8bit", 1.0);

    HTSetSuffix(".c",		"text/plain", "8bit", 1.0);
    HTSetSuffix(".cc",		"text/plain", "8bit", 1.0);
    HTSetSuffix(".c++",		"text/plain", "8bit", 1.0);
    HTSetSuffix(".h",		"text/plain", "8bit", 1.0);
    HTSetSuffix(".pl",		"text/plain", "8bit", 1.0);
    HTSetSuffix(".text",	"text/plain", "8bit", 1.0);
    HTSetSuffix(".txt",		"text/plain", "8bit", 1.0);

    HTSetSuffix(".php",		"text/html", "8bit", 1.0);
    HTSetSuffix(".php3",	"text/html", "8bit", 1.0);
    HTSetSuffix(".html3",	"text/html", "8bit", 1.0);
    HTSetSuffix(".ht3",		"text/html", "8bit", 1.0);
    HTSetSuffix(".phtml",	"text/html", "8bit", 1.0);
    HTSetSuffix(".shtml",	"text/html", "8bit", 1.0);
    HTSetSuffix(".sht",		"text/html", "8bit", 1.0);
    HTSetSuffix(".htmlx",	"text/html", "8bit", 1.0);
    HTSetSuffix(".htm",		"text/html", "8bit", 1.0);
    HTSetSuffix(".html",	"text/html", "8bit", 1.0);

    } else { /* LYSuffixRules */
    /*
     *  Note that even .html -> text/html, .htm -> text/html are omitted
     *  if default maps are compiled in but then skipped because of a
     *  configuration file directive.  Whoever changes the config file
     *  in this way can easily also add the SUFFIX rules there. - kw
     */
    CTRACE((tfp, "HTFileInit: Skipping all default (HTInit) extension maps!\n"));
    } /* LYSuffixRules */

#else /* BUILTIN_SUFFIX_MAPS */

    CTRACE((tfp, "HTFileInit: Default (HTInit) extension maps not compiled in.\n"));
    /*
     *  The followin two are still used if BUILTIN_SUFFIX_MAPS was
     *  undefined.  Without one of them, lynx would always need to
     *  have a mapping specified in a lynx.cfg or mime.types file
     *  to be usable for local HTML files at all.  That includes
     *  many of the generated user interface pages. - kw
     */
    HTSetSuffix(".htm",		"text/html", "8bit", 1.0);
    HTSetSuffix(".html",	"text/html", "8bit", 1.0);
#endif /* BUILTIN_SUFFIX_MAPS */


    /* These should override the default extensions as necessary. */
    HTLoadExtensionsConfigFile(global_extension_map);

    if (LYCanReadFile(personal_extension_map)) {
	/* These should override everything else. */
	HTLoadExtensionsConfigFile(personal_extension_map);
    } else {
	char buffer[LY_MAXPATH];
	LYAddPathToHome(buffer, sizeof(buffer), personal_extension_map);
	/* These should override everything else. */
	HTLoadExtensionsConfigFile(buffer);
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

PRIVATE int HTGetLine ARGS3(
	char *,		s,
	int,		n,
	FILE *,		f)
{
    register int i = 0, r;

    if (!f)
	return(1);

    while (1) {
	r = fgetc(f);
	s[i] = (char)r;

	if (s[i] == CR) {
	    r = fgetc(f);
	    if (r == LF)
		s[i] = (char) r;
	    else if (r != EOF)
		ungetc(r, f);
	}

	if ((r == EOF) || (s[i] == LF) || (s[i] == CR) || (i == (n-1))) {
	    s[i] = '\0';
	    return (feof(f) ? 1 : 0);
	}
	++i;
    }
}

PRIVATE void HTGetWord ARGS4(
	char *,		word,
	char *,		line,
	char ,		stop,
	char ,		stop2)
{
    int x = 0, y;

    for (x = 0; line[x] && line[x] != stop && line[x] != stop2; x++) {
	word[x] = line[x];
    }

    word[x] = '\0';
    if (line[x])
	++x;
    y=0;

    while ((line[y++] = line[x++]))
	;

    return;
}

PRIVATE int HTLoadExtensionsConfigFile ARGS1(
	char *,		fn)
{
    char line[MAX_STRING_LEN];
    char word[MAX_STRING_LEN];
    char *ct;
    FILE *f;
    int count = 0;

    CTRACE((tfp, "HTLoadExtensionsConfigFile: Loading file '%s'.\n", fn));

    if ((f = fopen(fn, TXT_R)) == NULL) {
	CTRACE((tfp, "HTLoadExtensionsConfigFile: Could not open '%s'.\n", fn));
	return count;
    }

    while (!(HTGetLine(line,sizeof(line),f))) {
	HTGetWord(word, line, ' ', '\t');
	if (line[0] == '\0' || word[0] == '#')
	    continue;
	ct = NULL;
	StrAllocCopy(ct, word);
	LYLowerCase(ct);

	while(line[0]) {
	    HTGetWord(word, line, ' ', '\t');
	    if (word[0] && (word[0] != ' ')) {
		char *ext = NULL;

		HTSprintf0(&ext, ".%s", word);
		LYLowerCase(ext);

		CTRACE((tfp, "SETTING SUFFIX '%s' to '%s'.\n", ext, ct));

	        if (strstr(ct, "tex") != NULL ||
	            strstr(ct, "postscript") != NULL ||
		    strstr(ct, "sh") != NULL ||
		    strstr(ct, "troff") != NULL ||
		    strstr(ct, "rtf") != NULL)
		    HTSetSuffix (ext, ct, "8bit", 1.0);
	        else
		    HTSetSuffix (ext, ct, "binary", 1.0);
		count++;

		FREE(ext);
	    }
	}
	FREE(ct);
    }
    LYCloseInput(f);

    return count;
}
