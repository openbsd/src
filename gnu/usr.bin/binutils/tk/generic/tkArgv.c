/*
 * tkArgv.c --
 *
 *	This file contains a procedure that handles table-based
 *	argv-argc parsing.
 *
 * Copyright (c) 1990-1994 The Regents of the University of California.
 * Copyright (c) 1994 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkArgv.c 1.20 96/02/15 18:51:32
 */

#include "tkPort.h"
#include "tk.h"

/*
 * Default table of argument descriptors.  These are normally available
 * in every application.
 */

static Tk_ArgvInfo defaultTable[] = {
    {"-help",		TK_ARGV_HELP,		(char *) NULL,	(char *) NULL,
	"Print summary of command-line options and abort"},
    {"-version",	TK_ARGV_VERSION,	(char *) NULL,	(char *) NULL,
	"Print version number and abort"},
    {NULL,	TK_ARGV_END,	(char *) NULL,	(char *) NULL,
	(char *) NULL}
};

/*
 * Forward declarations for procedures defined in this file:
 */

static void	PrintUsage _ANSI_ARGS_((Tcl_Interp *interp,
		    Tk_ArgvInfo *argTable, int flags));

/*
 *----------------------------------------------------------------------
 *
 * Tk_ParseArgv --
 *
 *	Process an argv array according to a table of expected
 *	command-line options.  See the manual page for more details.
 *
 * Results:
 *	The return value is a standard Tcl return value.  If an
 *	error occurs then an error message is left in interp->result.
 *	Under normal conditions, both *argcPtr and *argv are modified
 *	to return the arguments that couldn't be processed here (they
 *	didn't match the option table, or followed an TK_ARGV_REST
 *	argument).
 *
 * Side effects:
 *	Variables may be modified, resources may be entered for tkwin,
 *	or procedures may be called.  It all depends on the arguments
 *	and their entries in argTable.  See the user documentation
 *	for details.
 *
 *----------------------------------------------------------------------
 */

int
Tk_ParseArgv(interp, tkwin, argcPtr, argv, argTable, flags)
    Tcl_Interp *interp;		/* Place to store error message. */
    Tk_Window tkwin;		/* Window to use for setting Tk options.
				 * NULL means ignore Tk option specs. */
    int *argcPtr;		/* Number of arguments in argv.  Modified
				 * to hold # args left in argv at end. */
    char **argv;		/* Array of arguments.  Modified to hold
				 * those that couldn't be processed here. */
    Tk_ArgvInfo *argTable;	/* Array of option descriptions */
    int flags;			/* Or'ed combination of various flag bits,
				 * such as TK_ARGV_NO_DEFAULTS. */
{
    register Tk_ArgvInfo *infoPtr;
				/* Pointer to the current entry in the
				 * table of argument descriptions. */
    Tk_ArgvInfo *matchPtr;	/* Descriptor that matches current argument. */
    char *curArg;		/* Current argument */
    register char c;		/* Second character of current arg (used for
				 * quick check for matching;  use 2nd char.
				 * because first char. will almost always
				 * be '-'). */
    int srcIndex;		/* Location from which to read next argument
				 * from argv. */
    int dstIndex;		/* Index into argv to which next unused
				 * argument should be copied (never greater
				 * than srcIndex). */
    int argc;			/* # arguments in argv still to process. */
    size_t length;		/* Number of characters in current argument. */
    int i;

    if (flags & TK_ARGV_DONT_SKIP_FIRST_ARG) {
	srcIndex = dstIndex = 0;
	argc = *argcPtr;
    } else {
	srcIndex = dstIndex = 1;
	argc = *argcPtr-1;
    }

    while (argc > 0) {
	curArg = argv[srcIndex];
	srcIndex++;
	argc--;
	length = strlen(curArg);
	if (length > 0) {
	    c = curArg[1];
	} else {
	    c = 0;
	}

	/*
	 * Loop throught the argument descriptors searching for one with
	 * the matching key string.  If found, leave a pointer to it in
	 * matchPtr.
	 */

	matchPtr = NULL;
	for (i = 0; i < 2; i++) {
	    if (i == 0) {
		infoPtr = argTable;
	    } else {
		infoPtr = defaultTable;
	    }
	    for (; (infoPtr != NULL) && (infoPtr->type != TK_ARGV_END);
		    infoPtr++) {
		 if (infoPtr->key == NULL) {
		     continue;
		 }
		 if ((infoPtr->key[1] != c)
			 || (strncmp(infoPtr->key, curArg, length) != 0)) {
		     continue;
		 }
		 if ((tkwin == NULL)
			 && ((infoPtr->type == TK_ARGV_CONST_OPTION)
			 || (infoPtr->type == TK_ARGV_OPTION_VALUE)
			 || (infoPtr->type == TK_ARGV_OPTION_NAME_VALUE))) {
		     continue;
		 }
		 if (infoPtr->key[length] == 0) {
		     matchPtr = infoPtr;
		     goto gotMatch;
		 }
		 if (flags & TK_ARGV_NO_ABBREV) {
		     continue;
		 }
		 if (matchPtr != NULL) {
		     Tcl_AppendResult(interp, "ambiguous option \"", curArg,
			     "\"", (char *) NULL);
		     return TCL_ERROR;
		 }
		 matchPtr = infoPtr;
	    }
	}
	if (matchPtr == NULL) {

	    /*
	     * Unrecognized argument.  Just copy it down, unless the caller
	     * prefers an error to be registered.
	     */

	    if (flags & TK_ARGV_NO_LEFTOVERS) {
		Tcl_AppendResult(interp, "unrecognized argument \"",
			curArg, "\"", (char *) NULL);
		return TCL_ERROR;
	    }
	    argv[dstIndex] = curArg;
	    dstIndex++;
	    continue;
	}

	/*
	 * Take the appropriate action based on the option type
	 */

	gotMatch:
	infoPtr = matchPtr;
	switch (infoPtr->type) {
	    case TK_ARGV_CONSTANT:
		*((int *) infoPtr->dst) = (int) infoPtr->src;
		break;
	    case TK_ARGV_INT:
		if (argc == 0) {
		    goto missingArg;
		} else {
		    char *endPtr;

		    *((int *) infoPtr->dst) =
			    strtol(argv[srcIndex], &endPtr, 0);
		    if ((endPtr == argv[srcIndex]) || (*endPtr != 0)) {
			Tcl_AppendResult(interp, "expected integer argument ",
				"for \"", infoPtr->key, "\" but got \"",
				argv[srcIndex], "\"", (char *) NULL);
			return TCL_ERROR;
		    }
		    srcIndex++;
		    argc--;
		}
		break;
	    case TK_ARGV_STRING:
		if (argc == 0) {
		    goto missingArg;
		} else {
		    *((char **)infoPtr->dst) = argv[srcIndex];
		    srcIndex++;
		    argc--;
		}
		break;
	    case TK_ARGV_UID:
		if (argc == 0) {
		    goto missingArg;
		} else {
		    *((Tk_Uid *)infoPtr->dst) = Tk_GetUid(argv[srcIndex]);
		    srcIndex++;
		    argc--;
		}
		break;
	    case TK_ARGV_REST:
		*((int *) infoPtr->dst) = dstIndex;
		goto argsDone;
	    case TK_ARGV_FLOAT:
		if (argc == 0) {
		    goto missingArg;
		} else {
		    char *endPtr;

		    *((double *) infoPtr->dst) =
			    strtod(argv[srcIndex], &endPtr);
		    if ((endPtr == argv[srcIndex]) || (*endPtr != 0)) {
			Tcl_AppendResult(interp, "expected floating-point ",
				"argument for \"", infoPtr->key,
				"\" but got \"", argv[srcIndex], "\"",
				(char *) NULL);
			return TCL_ERROR;
		    }
		    srcIndex++;
		    argc--;
		}
		break;
	    case TK_ARGV_FUNC: {
		int (*handlerProc)();

		handlerProc = (int (*)())infoPtr->src;
		
		if ((*handlerProc)(infoPtr->dst, infoPtr->key,
			argv[srcIndex])) {
		    srcIndex += 1;
		    argc -= 1;
		}
		break;
	    }
	    case TK_ARGV_GENFUNC: {
		int	    (*handlerProc)();

		handlerProc = (int (*)())infoPtr->src;

		argc = (*handlerProc)(infoPtr->dst, interp, infoPtr->key,
			argc, argv+srcIndex);
		if (argc < 0) {
		    return TCL_ERROR;
		}
		break;
	    }
	    case TK_ARGV_HELP:
		PrintUsage (interp, argTable, flags);
		return TCL_ERROR;
	    case TK_ARGV_CONST_OPTION:
		Tk_AddOption(tkwin, infoPtr->dst, infoPtr->src,
			TK_INTERACTIVE_PRIO);
		break;
	    case TK_ARGV_OPTION_VALUE:
		if (argc < 1) {
		    goto missingArg;
		}
		Tk_AddOption(tkwin, infoPtr->dst, argv[srcIndex],
			TK_INTERACTIVE_PRIO);
		srcIndex++;
		argc--;
		break;
	    case TK_ARGV_OPTION_NAME_VALUE:
		if (argc < 2) {
		    Tcl_AppendResult(interp, "\"", curArg,
			    "\" option requires two following arguments",
			    (char *) NULL);
		    return TCL_ERROR;
		}
		Tk_AddOption(tkwin, argv[srcIndex], argv[srcIndex+1],
			TK_INTERACTIVE_PRIO);
		srcIndex += 2;
		argc -= 2;
		break;
	    case TK_ARGV_VERSION:
	        Tcl_AppendResult(interp, "Tk version ", TK_VERSION,
			(char *) NULL);
		return TCL_ERROR;
	    default:
		sprintf(interp->result, "bad argument type %d in Tk_ArgvInfo",
			infoPtr->type);
		return TCL_ERROR;
	}
    }

    /*
     * If we broke out of the loop because of an OPT_REST argument,
     * copy the remaining arguments down.
     */

    argsDone:
    while (argc) {
	argv[dstIndex] = argv[srcIndex];
	srcIndex++;
	dstIndex++;
	argc--;
    }
    argv[dstIndex] = (char *) NULL;
    *argcPtr = dstIndex;
    return TCL_OK;

    missingArg:
    Tcl_AppendResult(interp, "\"", curArg,
	    "\" option requires an additional argument", (char *) NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * PrintUsage --
 *
 *	Generate a help string describing command-line options.
 *
 * Results:
 *	Interp->result will be modified to hold a help string
 *	describing all the options in argTable, plus all those
 *	in the default table unless TK_ARGV_NO_DEFAULTS is
 *	specified in flags.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
PrintUsage(interp, argTable, flags)
    Tcl_Interp *interp;		/* Place information in this interp's
				 * result area. */
    Tk_ArgvInfo *argTable;	/* Array of command-specific argument
				 * descriptions. */
    int flags;			/* If the TK_ARGV_NO_DEFAULTS bit is set
				 * in this word, then don't generate
				 * information for default options. */
{
    register Tk_ArgvInfo *infoPtr;
    int width, i, numSpaces;
#define NUM_SPACES 20
    static char spaces[] = "                    ";
    char tmp[30];

    /*
     * First, compute the width of the widest option key, so that we
     * can make everything line up.
     */

    width = 4;
    for (i = 0; i < 2; i++) {
	for (infoPtr = i ? defaultTable : argTable;
		infoPtr->type != TK_ARGV_END; infoPtr++) {
	    int length;
	    if (infoPtr->key == NULL) {
		continue;
	    }
	    length = strlen(infoPtr->key);
	    if (length > width) {
		width = length;
	    }
	}
    }

    Tcl_AppendResult(interp, "Command-specific options:", (char *) NULL);
    for (i = 0; ; i++) {
	for (infoPtr = i ? defaultTable : argTable;
		infoPtr->type != TK_ARGV_END; infoPtr++) {
	    if ((infoPtr->type == TK_ARGV_HELP) && (infoPtr->key == NULL)) {
		Tcl_AppendResult(interp, "\n", infoPtr->help, (char *) NULL);
		continue;
	    }
	    Tcl_AppendResult(interp, "\n ", infoPtr->key, ":", (char *) NULL);
	    numSpaces = width + 1 - strlen(infoPtr->key);
	    while (numSpaces > 0) {
		if (numSpaces >= NUM_SPACES) {
		    Tcl_AppendResult(interp, spaces, (char *) NULL);
		} else {
		    Tcl_AppendResult(interp, spaces+NUM_SPACES-numSpaces,
			    (char *) NULL);
		}
		numSpaces -= NUM_SPACES;
	    }
	    Tcl_AppendResult(interp, infoPtr->help, (char *) NULL);
	    switch (infoPtr->type) {
		case TK_ARGV_INT: {
		    sprintf(tmp, "%d", *((int *) infoPtr->dst));
		    Tcl_AppendResult(interp, "\n\t\tDefault value: ",
			    tmp, (char *) NULL);
		    break;
		}
		case TK_ARGV_FLOAT: {
		    sprintf(tmp, "%g", *((double *) infoPtr->dst));
		    Tcl_AppendResult(interp, "\n\t\tDefault value: ",
			    tmp, (char *) NULL);
		    break;
		}
		case TK_ARGV_STRING: {
		    char *string;

		    string = *((char **) infoPtr->dst);
		    if (string != NULL) {
			Tcl_AppendResult(interp, "\n\t\tDefault value: \"",
				string, "\"", (char *) NULL);
		    }
		    break;
		}
		default: {
		    break;
		}
	    }
	}

	if ((flags & TK_ARGV_NO_DEFAULTS) || (i > 0)) {
	    break;
	}
	Tcl_AppendResult(interp, "\nGeneric options for all commands:",
		(char *) NULL);
    }
}
