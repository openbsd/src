/*		Configuration-specific Initialialization	HTInit.c
**		----------------------------------------
*/

/*	Define a basic set of suffixes and presentations
**	------------------------------------------------
*/

#include "HTUtils.h"
#include "tcp.h"

/* Implements:
*/
#include "HTInit.h"

#include "HTML.h"
#include "HTPlain.h"
#include "HTMLGen.h"
#include "HTFile.h"
#include "HTFormat.h"
#include "HTMIME.h"
#include "HTWSRC.h"

#include "HTSaveToFile.h"  /* LJM */
#include "userdefs.h"
#include "LYUtils.h"
#include "LYGlobalDefs.h"
#include "LYSignal.h"
#include "LYSystem.h"

#include "LYexit.h"
#include "LYLeaks.h"

#define FREE(x) if (x) {free(x); x = NULL;}

#ifdef VMS
#define DISPLAY "DECW$DISPLAY"
#else
#define DISPLAY "DISPLAY"
#endif /* VMS */

PRIVATE int HTLoadTypesConfigFile PARAMS((char *fn));
PRIVATE int HTLoadExtensionsConfigFile PARAMS((char *fn));

PUBLIC void HTFormatInit NOARGS
{
 FILE *fp = NULL;
 char *cp = NULL;

#ifdef NeXT
  HTSetPresentation("application/postscript",   "open %s", 1.0, 2.0, 0.0, 0);
  HTSetPresentation("image/x-tiff",             "open %s", 2.0, 2.0, 0.0, 0);
  HTSetPresentation("image/tiff",               "open %s", 1.0, 2.0, 0.0, 0);
  HTSetPresentation("audio/basic",              "open %s", 1.0, 2.0, 0.0, 0);
  HTSetPresentation("*",                        "open %s", 1.0, 0.0, 0.0, 0);
#else
 if ((cp = getenv(DISPLAY)) != NULL && *cp != '\0') {	/* Must have X11 */
  HTSetPresentation("application/postscript", "ghostview %s&",
  							    1.0, 3.0, 0.0, 0);
  HTSetPresentation("image/gif",        XLoadImageCommand,  1.0, 3.0, 0.0, 0);
  HTSetPresentation("image/x-xbm",      XLoadImageCommand,  1.0, 3.0, 0.0, 0);
  HTSetPresentation("image/x-xbitmap",  XLoadImageCommand,  1.0, 3.0, 0.0, 0);
  HTSetPresentation("image/x-png",      XLoadImageCommand,  2.0, 3.0, 0.0, 0);
  HTSetPresentation("image/png",	XLoadImageCommand,  1.0, 3.0, 0.0, 0);
  HTSetPresentation("image/x-rgb",      XLoadImageCommand,  1.0, 3.0, 0.0, 0);
  HTSetPresentation("image/x-tiff",     XLoadImageCommand,  2.0, 3.0, 0.0, 0);
  HTSetPresentation("image/tiff",	XLoadImageCommand,  1.0, 3.0, 0.0, 0);
  HTSetPresentation("image/jpeg",       XLoadImageCommand,  1.0, 3.0, 0.0, 0);
  HTSetPresentation("video/mpeg",       "mpeg_play %s &",   1.0, 3.0, 0.0, 0);

 }
#endif

#ifdef EXEC_SCRIPTS
 /* set quality to 999.0 for protected exec applications */
#ifndef VMS
 HTSetPresentation("application/x-csh",	"csh %s", 999.0, 3.0, 0.0, 0);
 HTSetPresentation("application/x-sh",	"sh %s",  999.0, 3.0, 0.0, 0);
 HTSetPresentation("application/x-ksh",	"ksh %s", 999.0, 3.0, 0.0, 0);
#else
 HTSetPresentation("application/x-VMS_script",	"@%s", 999.0, 3.0, 0.0, 0);
#endif /* not VMS */
#endif /* EXEC_SCRIPTS */

/*
 *  Add our header handlers.
 */
 HTSetConversion("www/mime",  "www/present",  HTMIMEConvert, 1.0, 0.0, 0.0, 0);
 HTSetConversion("www/mime",  "www/download", HTMIMEConvert, 1.0, 0.0, 0.0, 0);
 HTSetConversion("www/mime",  "www/source",   HTMIMEConvert, 1.0, 0.0, 0.0, 0);
 HTSetConversion("www/mime",  "www/dump",     HTMIMEConvert, 1.0, 0.0, 0.0, 0);

/*
 *  Add our compressed file handlers.
 */
 HTSetConversion("www/compressed", "www/present",
 					      HTCompressed,   1.0, 0.0, 0.0, 0);
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
 HTSetConversion("text/html", "text/x-c",    HTMLToC, 	     0.5, 0.0, 0.0, 0);
 HTSetConversion("text/html", "text/plain",  HTMLToPlain,    0.5, 0.0, 0.0, 0);
 HTSetConversion("text/html", "www/present", HTMLPresent,    1.0, 0.0, 0.0, 0);

 /*
  *  These should override the default types as necessary.
  */
 HTLoadTypesConfigFile(global_type_map);

 /*
  *  Load the local maps.
  */
 if ((fp = fopen(personal_type_map,"r")) != NULL) {
     fclose(fp);
     /* These should override everything else. */
     HTLoadTypesConfigFile(personal_type_map);
 } else {
     char buffer[256];
#ifdef VMS
     sprintf(buffer, "sys$login:%s", personal_type_map);
#else
     sprintf(buffer, "%s/%s", (Home_Dir() ? Home_Dir() : ""),
			      personal_type_map);
#endif
     HTLoadTypesConfigFile(buffer);
 }

 /*
  *  Put text/html and text/plain at beginning of list. - kw
  */
 HTReorderPresentation(WWW_PLAINTEXT, WWW_PRESENT);
 HTReorderPresentation(WWW_HTML, WWW_PRESENT);
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
#define TMPFILE_NAME_SIZE	256

PRIVATE char *GetCommand ARGS2(
	char *,		s,
	char **,	t)
{
    char *s2;
    int quoted = 0;

    /* marca -- added + 1 for error case -- oct 24, 1993. */
    s2 = malloc(strlen(s)*2 + 1); /* absolute max, if all % signs */
    if (!s2)
	ExitWithError("Out of memory");

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
    char *tmp, *news;

    /* strip leading white space */
    while (*s && isspace((unsigned char) *s))
	++s;
    news = s;
    /* put in lower case */
    for (tmp=s; *tmp; ++tmp) {
	*tmp = TOLOWER ((unsigned char)*tmp);
    }
    /* strip trailing white space */
    while ((tmp > news) && *--tmp && isspace((unsigned char) *tmp))
	*tmp = '\0';
    return(news);
}

PRIVATE int ProcessMailcapEntry ARGS2(
	FILE *,			fp,
	struct MailcapEntry *,	mc)
{
    int i, j;
    size_t rawentryalloc = 2000, len;
    char *rawentry, *s, *t, *LineBuf;

    LineBuf = (char *)malloc(LINE_BUF_SIZE);
    if (!LineBuf)
	ExitWithError("Out of memory");
    rawentry = (char *)malloc(1 + rawentryalloc);
    if (!rawentry)
	ExitWithError("Out of memory");
    *rawentry = '\0';
    while (fgets(LineBuf, LINE_BUF_SIZE, fp)) {
	if (LineBuf[0] == '#')
	    continue;
	len = strlen(LineBuf);
	if (len == 0)
	    continue;
	if (LineBuf[len-1] == '\n')
	    LineBuf[--len] = '\0';
	if ((len + strlen(rawentry)) > rawentryalloc) {
	    rawentryalloc += 2000;
	    rawentry = realloc(rawentry, rawentryalloc+1);
	    if (!rawentry)
	        ExitWithError("Out of memory");
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

    for (s = rawentry; *s && isspace((unsigned char) *s); ++s)
	;
    if (!*s) {
	/* totally blank entry -- quietly ignore */
	FREE(rawentry);
	return(0);
    }
    s = strchr(rawentry, ';');
    if (s == NULL) {
	if (TRACE) {
		fprintf(stderr,
		 "ProcessMailcapEntry: Ignoring invalid mailcap entry: %s\n",
			rawentry);
	}
	FREE(rawentry);
	return(0);
    }
    *s++ = '\0';
    if (!strncasecomp(rawentry, "text/html", 9) ||
	!strncasecomp(rawentry, "text/plain", 10)) {
	--s;
	*s = ';';
	if (TRACE) {
		fprintf(stderr,
			"ProcessMailcapEntry: Ignoring mailcap entry: %s\n",
			rawentry);
	}
	FREE(rawentry);
	return(0);
    }
    for (i = 0, j = 0; rawentry[i]; i++) {
	if (rawentry[i] != ' ') {
	    rawentry[j++] = TOLOWER(rawentry[i]);
	}
    }
    rawentry[j] = '\0';
    mc->needsterminal = 0;
    mc->copiousoutput = 0;
    mc->needtofree = 1;
    mc->testcommand = NULL;
    mc->label = NULL;
    mc->printcommand = NULL;
    mc->contenttype = (char *)malloc(1 + strlen(rawentry));
    if (!mc->contenttype)
	ExitWithError("Out of memory");
    strcpy(mc->contenttype, rawentry);
    mc->quality = 1.0;
    mc->maxbytes = 0;
    t = GetCommand(s, &mc->command);
    if (!t) {
	goto assign_presentation;
    }
    s = t;
    while (s && *s && isspace((unsigned char) *s)) ++s;
    while (s) {
	char *arg, *eq, *mallocd_string;

	t = GetCommand(s, &mallocd_string);
	arg = mallocd_string;
	eq = strchr(arg, '=');
	if (eq) {
	    *eq++ = '\0';
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
		if (TRACE)
		    fprintf(stderr,
		    	    "ProcessMailcapEntry: Found testcommand:%s\n",
			    mc->testcommand);
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
	        mc->quality = atof(eq);
		if (mc->quality > 0.000 && mc->quality < 0.001)
		    mc->quality = 0.001;
	    } else if (eq && !strcmp(arg, "mxb")) {
	        mc->maxbytes = atol(eq);
		if (mc->maxbytes < 0)
		    mc->maxbytes = 0;
	    } else if (strcmp(arg, "notes")) { /* IGNORE notes field */
		if (*arg && TRACE)
		    fprintf(stderr,
			"ProcessMailcapEntry: Ignoring mailcap flag '%s'.\n",
			    arg);
	    }

	}
      FREE(mallocd_string);
      s = t;
    }

assign_presentation:
    FREE(rawentry);

    if (PassesTest(mc)) {
	if (TRACE)
	    fprintf(stderr,
	    	    "ProcessMailcapEntry Setting up conversion %s : %s\n",
		    mc->contenttype, mc->command);
	HTSetPresentation(mc->contenttype, mc->command,
			  mc->quality, 3.0, 0.0, mc->maxbytes);
    }
    FREE(mc->command);
    FREE(mc->contenttype);

    return(1);
}

PRIVATE void BuildCommand ARGS5(
	char **, 	pBuf,
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
		case 'F':
		    if (TRACE) {
		        fprintf(stderr,
			     "BuildCommand: Bad mailcap \"test\" clause: %s\n",
				controlstring);
		    }
		case 's':
		    if (TmpFileLen && TmpFileName) {
			if ((to - *pBuf) + TmpFileLen + 1 > Bufsize) {
			    *to = '\0';
			    if (TRACE) {
				fprintf(stderr,
			"BuildCommand: Too long mailcap \"test\" clause,\n");
				fprintf(stderr,
					"              ignoring: %s%s...\n",
					*pBuf, TmpFileName);
			    }
			    **pBuf = '\0';
			    return;
			}
			strcpy(to, TmpFileName);
			to += strlen(TmpFileName);
		    }
		    break;
		default:
		    if (TRACE) {
			fprintf(stderr,
  "BuildCommand: Ignoring unrecognized format code in mailcap file '%%%c'.\n",
			*from);
		    }
		    break;
	    }
	} else if (*from == '%') {
	    prefixed = 1;
	} else {
	    *to++ = *from;
	}
	if (to >= *pBuf + Bufsize) {
	    (*pBuf)[Bufsize - 1] = '\0';
	    if (TRACE) {
		fprintf(stderr,
			"BuildCommand: Too long mailcap \"test\" clause,\n");
		fprintf(stderr,
			"              ignoring: %s...\n",
			*pBuf);
	    }
	    **pBuf = '\0';
	    return;
	}
    }
    *to = '\0';
}

PRIVATE int PassesTest ARGS1(
	struct MailcapEntry *,	mc)
{
    int result;
    char *cmd, TmpFileName[TMPFILE_NAME_SIZE];
    char *cp = NULL;

    /*
     *  Make sure we have a command
     */
    if (!mc->testcommand)
	return(1);

    /*
     *  Save overhead of system() calls by faking these. - FM
     */
    if (0 == strcasecomp(mc->testcommand, "test -n \"$DISPLAY\"")) {
	FREE(mc->testcommand);
	if (TRACE)
	    fprintf(stderr,
		    "PassesTest: Testing for XWINDOWS environment.\n");
    	if ((cp = getenv(DISPLAY)) != NULL && *cp != '\0') {
	    if (TRACE)
	        fprintf(stderr,"PassesTest: Test passed!\n");
	    return(0 == 0);
	} else {
	    if (TRACE)
	        fprintf(stderr,"PassesTest: Test failed!\n");
	    return(-1 == 0);
	}
    }
    if (0 == strcasecomp(mc->testcommand, "test -z \"$DISPLAY\"")) {
	FREE(mc->testcommand);
	if (TRACE)
	    fprintf(stderr,
		    "PassesTest: Testing for NON_XWINDOWS environment.\n");
    	if (!((cp = getenv(DISPLAY)) != NULL && *cp != '\0')) {
	    if (TRACE)
	        fprintf(stderr,"PassesTest: Test passed!\n");
	    return(0 == 0);
	} else {
	    if (TRACE)
	        fprintf(stderr,"PassesTest: Test failed!\n");
	    return(-1 == 0);
	}
    }

    /*
     *  Why do anything but return success for this one! - FM
     */
    if (0 == strcasecomp(mc->testcommand, "test -n \"$LYNX_VERSION\"")){
	FREE(mc->testcommand);
	if (TRACE) {
	    fprintf(stderr,
		    "PassesTest: Testing for LYNX environment.\n");
	    fprintf(stderr,"PassesTest: Test passed!\n");
	}
	return(0 == 0);
    } else
    /*
     *  ... or failure for this one! - FM
     */
    if (0 == strcasecomp(mc->testcommand, "test -z \"$LYNX_VERSION\"")) {
	FREE(mc->testcommand);
	if (TRACE) {
	    fprintf(stderr,
		    "PassesTest: Testing for non-LYNX environment.\n");
	    fprintf(stderr,"PassesTest: Test failed!\n");
	}
	return(-1 == 0);
    }

    /*
     *  Build the command and execute it.
     */
    tempname(TmpFileName, NEW_FILE);
    cmd = (char *)malloc(1024);
    if (!cmd)
	ExitWithError("Out of memory");
    BuildCommand(&cmd, 1024,
		 mc->testcommand,
		 TmpFileName,
		 strlen(TmpFileName));
    if (TRACE)
	fprintf(stderr,"PassesTest: Executing test command: %s\n", cmd);
    result = system(cmd);
    FREE(cmd);

    /*
     *  Free the test command as well since
     *  we wont be needing it anymore.
     */
    FREE(mc->testcommand);

    if (TRACE && result)
	fprintf(stderr,"PassesTest: Test failed!\n");
    else if (TRACE)
	fprintf(stderr,"PassesTest: Test passed!\n");

    return(result == 0);
}

PRIVATE int ProcessMailcapFile ARGS1(
	char *,		file)
{
    struct MailcapEntry mc;
    FILE *fp;

    if (TRACE)
	fprintf(stderr,
		"ProcessMailcapFile: Loading file '%s'.\n",
		file);
    if ((fp = fopen(file, "r")) == NULL) {
	if (TRACE)
	    fprintf(stderr,
		"ProcessMailcapFile: Could not open '%s'.\n",
		    file);
	return(-1 == 0);
    }

    while (fp && !feof(fp)) {
	ProcessMailcapEntry(fp, &mc);
    }
    fclose(fp);
    return(0 == 0);
}

PRIVATE int ExitWithError ARGS1(
	char *,		txt)
{
    if (txt)
	fprintf(stderr, "metamail: %s\n", txt);
#ifndef NOSIGHUP
    (void) signal(SIGHUP, SIG_DFL);
#endif /* NOSIGHUP */
    (void) signal(SIGTERM, SIG_DFL);
#ifndef VMS
    (void) signal(SIGINT, SIG_DFL);
#endif /* !VMS */
#ifdef SIGTSTP
    if (no_suspend)
	(void) signal(SIGTSTP,SIG_DFL);
#endif /* SIGTSTP */
    exit(-1);
    return(-1);
}


PRIVATE int HTLoadTypesConfigFile ARGS1(
	char *,		fn)
{
  return ProcessMailcapFile(fn);
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

PUBLIC void HTFileInit NOARGS
{
    FILE *fp;

    if (TRACE)
	fprintf(stderr,
		"HTFileInit: Loading default (HTInit) extension maps.\n");

    /* default suffix interpretation */
    HTSetSuffix("*",		"text/plain", "7bit", 1.0);
    HTSetSuffix("*.*",		"text/plain", "7bit", 1.0);

#ifdef EXEC_SCRIPTS
    /*
     *  define these extentions for exec scripts.
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
    HTSetSuffix(".exe",		"application/x-Executable", "binary", 1.0);

    HTSetSuffix(".exe.Z",	"application/x-Comp. Executable",
    							     "binary", 1.0);

    HTSetSuffix(".Z",	        "application/UNIX Compressed", "binary", 1.0);

    HTSetSuffix(".tar_Z",	"application/UNIX Compr. Tar", "binary", 1.0);
    HTSetSuffix(".tar.Z",	"application/UNIX Compr. Tar", "binary", 1.0);

    HTSetSuffix("-gz",		"application/GNU Compressed", "binary", 1.0);
    HTSetSuffix("_gz",		"application/GNU Compressed", "binary", 1.0);
    HTSetSuffix(".gz",		"application/GNU Compressed", "binary", 1.0);

    HTSetSuffix5(".tar.gz",	"application/x-tar", "binary", "GNU Compr. Tar", 1.0);
    HTSetSuffix5(".tgz",	"application/x-tar", "gzip", "GNU Compr. Tar", 1.0);

    HTSetSuffix(".src",		"application/x-WAIS-source", "8bit", 1.0);
    HTSetSuffix(".wsrc",	"application/x-WAIS-source", "8bit", 1.0);

    HTSetSuffix(".zip",		"application/x-Zip File", "binary", 1.0);

    HTSetSuffix(".uu",		"application/x-UUencoded", "8bit", 1.0);

    HTSetSuffix(".hqx",		"application/x-Binhex", "8bit", 1.0);

    HTSetSuffix(".o",		"application/x-Prog. Object", "binary", 1.0);
    HTSetSuffix(".a",		"application/x-Prog. Library", "binary", 1.0);

    HTSetSuffix5(".oda",	"application/oda", "binary", "ODA", 1.0);

    HTSetSuffix5(".pdf",	"application/pdf", "binary", "PDF", 1.0);

    HTSetSuffix(".eps",		"application/Postscript", "8bit", 1.0);
    HTSetSuffix(".ai",		"application/Postscript", "8bit", 1.0);
    HTSetSuffix(".ps",		"application/Postscript", "8bit", 1.0);

    HTSetSuffix(".rtf",		"application/RTF", "8bit", 1.0);

    HTSetSuffix(".dvi",		"application/x-DVI", "8bit", 1.0);

    HTSetSuffix(".hdf",		"application/x-HDF", "8bit", 1.0);

    HTSetSuffix(".cdf",		"application/x-netcdf", "8bit", 1.0);
    HTSetSuffix(".nc",		"application/x-netcdf", "8bit", 1.0);

    HTSetSuffix(".latex",	"application/x-Latex", "8bit", 1.0);
    HTSetSuffix(".tex",  	"application/x-Tex", "8bit", 1.0);
    HTSetSuffix(".texinfo",	"application/x-Texinfo", "8bit", 1.0);
    HTSetSuffix(".texi",	"application/x-Texinfo", "8bit", 1.0);

    HTSetSuffix(".t",		"application/x-Troff", "8bit", 1.0);
    HTSetSuffix(".tr",		"application/x-Troff", "8bit", 1.0);
    HTSetSuffix(".roff",	"application/x-Troff", "8bit", 1.0);

    HTSetSuffix(".man",		"application/x-Troff-man", "8bit", 1.0);
    HTSetSuffix(".me",		"application/x-Troff-me", "8bit", 1.0);
    HTSetSuffix(".ms",		"application/x-Troff-ms", "8bit", 1.0);

    HTSetSuffix(".zoo",		"application/x-Zoo File", "binary", 1.0);

    HTSetSuffix(".bak",		"application/x-VMS BAK File", "binary", 1.0);
    HTSetSuffix(".bkp",		"application/x-VMS BAK File", "binary", 1.0);
    HTSetSuffix(".bck",		"application/x-VMS BAK File", "binary", 1.0);

    HTSetSuffix(".bkp_gz",	"application/x-GNU BAK File", "binary", 1.0);
    HTSetSuffix(".bkp-gz",	"application/x-GNU BAK File", "binary", 1.0);
    HTSetSuffix(".bck_gz",	"application/x-GNU BAK File", "binary", 1.0);
    HTSetSuffix(".bck-gz",	"application/x-GNU BAK File", "binary", 1.0);

    HTSetSuffix(".bkp-Z",	"application/x-Comp. BAK File", "binary", 1.0);
    HTSetSuffix(".bkp_Z",	"application/x-Comp. BAK File", "binary", 1.0);
    HTSetSuffix(".bck-Z",	"application/x-Comp. BAK File", "binary", 1.0);
    HTSetSuffix(".bck_Z",	"application/x-Comp. BAK File", "binary", 1.0);

    HTSetSuffix(".hlb",		"application/x-VMS Help Libr.", "binary", 1.0);
    HTSetSuffix(".olb",		"application/x-VMS Obj. Libr.", "binary", 1.0);
    HTSetSuffix(".tlb",		"application/x-VMS Text Libr.", "binary", 1.0);
    HTSetSuffix(".obj",		"application/x-VMS Prog. Obj.", "binary", 1.0);
    HTSetSuffix(".decw$book",	"application/x-DEC BookReader", "binary", 1.0);
    HTSetSuffix(".mem",		"application/x-RUNOFF-MANUAL", "8bit", 1.0);

    HTSetSuffix(".vsd",		"application/visio", "binary", 1.0);

    HTSetSuffix(".lha",		"application/x-lha File", "binary", 1.0);
    HTSetSuffix(".lzh",		"application/x-lzh File", "binary", 1.0);

    HTSetSuffix(".sea",		"application/x-sea File", "binary", 1.0);
    HTSetSuffix(".sit",		"application/x-sit File", "binary", 1.0);

    HTSetSuffix(".dms",		"application/x-dms File", "binary", 1.0);

    HTSetSuffix(".iff",		"application/x-iff File", "binary", 1.0);

    HTSetSuffix(".bcpio",	"application/x-bcpio", "binary", 1.0);
    HTSetSuffix(".cpio",	"application/x-cpio", "binary", 1.0);

    HTSetSuffix(".gtar",	"application/x-gtar", "binary", 1.0);

    HTSetSuffix(".shar",	"application/x-shar", "8bit", 1.0);
    HTSetSuffix(".share",	"application/x-share", "8bit", 1.0);

    HTSetSuffix(".sh",		"application/x-sh", "8bit", 1.0); /* xtra */

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

    HTSetSuffix(".html3",	"text/html", "8bit", 1.0);
    HTSetSuffix(".ht3",		"text/html", "8bit", 1.0);
    HTSetSuffix(".phtml",	"text/html", "8bit", 1.0);
    HTSetSuffix(".shtml",	"text/html", "8bit", 1.0);
    HTSetSuffix(".htmlx",	"text/html", "8bit", 1.0);
    HTSetSuffix(".htm",		"text/html", "8bit", 1.0);
    HTSetSuffix(".html",	"text/html", "8bit", 1.0);

    /* These should override the default extensions as necessary. */
    HTLoadExtensionsConfigFile(global_extension_map);

    if ((fp = fopen(personal_extension_map,"r")) != NULL) {
	fclose(fp);
	/* These should override everything else. */
	HTLoadExtensionsConfigFile(personal_extension_map);
    } else {
	char buffer[256];
#ifdef VMS
	sprintf(buffer, "sys$login:%s", personal_extension_map);
#else
	sprintf(buffer, "%s/%s", (Home_Dir() ? Home_Dir() : ""),
				  personal_extension_map);
#endif /* VMS */
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
		s[i] = r;
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
    char l[MAX_STRING_LEN],w[MAX_STRING_LEN],*ct;
    FILE *f;
    int x, count = 0;

    if (TRACE)
	fprintf(stderr,
		"HTLoadExtensionsConfigFile: Loading file '%s'.\n", fn);

    if ((f = fopen(fn,"r")) == NULL) {
	if (TRACE)
	    fprintf(stderr,
		    "HTLoadExtensionsConfigFile: Could not open '%s'.\n", fn);
	    return count;
    }

    while (!(HTGetLine(l,MAX_STRING_LEN,f))) {
	HTGetWord(w, l, ' ', '\t');
	if (l[0] == '\0' || w[0] == '#')
	    continue;
	ct = (char *)malloc(sizeof(char) * (strlen(w) + 1));
	if (!ct)
	    outofmem(__FILE__, "HTLoadExtensionsConfigFile");
	strcpy(ct,w);
	for (x = 0; ct[x]; x++)
	    ct[x] = TOLOWER(ct[x]);

	while(l[0]) {
	    HTGetWord(w, l, ' ', '\t');
	    if (w[0] && (w[0] != ' ')) {
		char *ext = (char *)malloc(sizeof(char) * (strlen(w)+1+1));
	        if (!ct)
	            outofmem(__FILE__, "HTLoadExtensionsConfigFile");

		for (x = 0; w[x]; x++)
		    ext[x+1] = TOLOWER(w[x]);
		ext[0] = '.';
		ext[strlen(w)+1] = '\0';

		if (TRACE) {
		    fprintf (stderr,
			     "SETTING SUFFIX '%s' to '%s'.\n", ext, ct);
		}

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
    fclose(f);

    return count;
}
