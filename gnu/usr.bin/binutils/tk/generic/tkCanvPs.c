/* 
 * tkCanvPs.c --
 *
 *	This module provides Postscript output support for canvases,
 *	including the "postscript" widget command plus a few utility
 *	procedures used for generating Postscript.
 *
 * Copyright (c) 1991-1994 The Regents of the University of California.
 * Copyright (c) 1994-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkCanvPs.c 1.47 96/04/11 20:23:36
 */

#include <stdio.h>
#include "tkInt.h"
#include "tkCanvas.h"
#include "tkPort.h"

/*
 * See tkCanvas.h for key data structures used to implement canvases.
 */

/*
 * One of the following structures is created to keep track of Postscript
 * output being generated.  It consists mostly of information provided on
 * the widget command line.
 */

typedef struct TkPostscriptInfo {
    int x, y, width, height;	/* Area to print, in canvas pixel
				 * coordinates. */
    int x2, y2;			/* x+width and y+height. */
    char *pageXString;		/* String value of "-pagex" option or NULL. */
    char *pageYString;		/* String value of "-pagey" option or NULL. */
    double pageX, pageY;	/* Postscript coordinates (in points)
				 * corresponding to pageXString and
				 * pageYString. Don't forget that y-values
				 * grow upwards for Postscript! */
    char *pageWidthString;	/* Printed width of output. */
    char *pageHeightString;	/* Printed height of output. */
    double scale;		/* Scale factor for conversion: each pixel
				 * maps into this many points. */
    Tk_Anchor pageAnchor;	/* How to anchor bbox on Postscript page. */
    int rotate;			/* Non-zero means output should be rotated
				 * on page (landscape mode). */
    char *fontVar;		/* If non-NULL, gives name of global variable
				 * containing font mapping information.
				 * Malloc'ed. */
    char *colorVar;		/* If non-NULL, give name of global variable
				 * containing color mapping information.
				 * Malloc'ed. */
    char *colorMode;		/* Mode for handling colors:  "monochrome",
				 * "gray", or "color".  Malloc'ed. */
    int colorLevel;		/* Numeric value corresponding to colorMode:
				 * 0 for mono, 1 for gray, 2 for color. */
    char *fileName;		/* Name of file in which to write Postscript;
				 * NULL means return Postscript info as
				 * result. Malloc'ed. */
    FILE *f;			/* Open file corresponding to fileName. */
    Tcl_HashTable fontTable;	/* Hash table containing names of all font
				 * families used in output.  The hash table
				 * values are not used. */
    int prepass;		/* Non-zero means that we're currently in
				 * the pre-pass that collects font information,
				 * so the Postscript generated isn't
				 * relevant. */
} TkPostscriptInfo;

/*
 * The table below provides a template that's used to process arguments
 * to the canvas "postscript" command and fill in TkPostscriptInfo
 * structures.
 */

static Tk_ConfigSpec configSpecs[] = {
    {TK_CONFIG_STRING, "-colormap", (char *) NULL, (char *) NULL,
	"", Tk_Offset(TkPostscriptInfo, colorVar), 0},
    {TK_CONFIG_STRING, "-colormode", (char *) NULL, (char *) NULL,
	"", Tk_Offset(TkPostscriptInfo, colorMode), 0},
    {TK_CONFIG_STRING, "-file", (char *) NULL, (char *) NULL,
	"", Tk_Offset(TkPostscriptInfo, fileName), 0},
    {TK_CONFIG_STRING, "-fontmap", (char *) NULL, (char *) NULL,
	"", Tk_Offset(TkPostscriptInfo, fontVar), 0},
    {TK_CONFIG_PIXELS, "-height", (char *) NULL, (char *) NULL,
	"", Tk_Offset(TkPostscriptInfo, height), 0},
    {TK_CONFIG_ANCHOR, "-pageanchor", (char *) NULL, (char *) NULL,
	"", Tk_Offset(TkPostscriptInfo, pageAnchor), 0},
    {TK_CONFIG_STRING, "-pageheight", (char *) NULL, (char *) NULL,
	"", Tk_Offset(TkPostscriptInfo, pageHeightString), 0},
    {TK_CONFIG_STRING, "-pagewidth", (char *) NULL, (char *) NULL,
	"", Tk_Offset(TkPostscriptInfo, pageWidthString), 0},
    {TK_CONFIG_STRING, "-pagex", (char *) NULL, (char *) NULL,
	"", Tk_Offset(TkPostscriptInfo, pageXString), 0},
    {TK_CONFIG_STRING, "-pagey", (char *) NULL, (char *) NULL,
	"", Tk_Offset(TkPostscriptInfo, pageYString), 0},
    {TK_CONFIG_BOOLEAN, "-rotate", (char *) NULL, (char *) NULL,
	"", Tk_Offset(TkPostscriptInfo, rotate), 0},
    {TK_CONFIG_PIXELS, "-width", (char *) NULL, (char *) NULL,
	"", Tk_Offset(TkPostscriptInfo, width), 0},
    {TK_CONFIG_PIXELS, "-x", (char *) NULL, (char *) NULL,
	"", Tk_Offset(TkPostscriptInfo, x), 0},
    {TK_CONFIG_PIXELS, "-y", (char *) NULL, (char *) NULL,
	"", Tk_Offset(TkPostscriptInfo, y), 0},
    {TK_CONFIG_END, (char *) NULL, (char *) NULL, (char *) NULL,
	(char *) NULL, 0, 0}
};

/*
 * Forward declarations for procedures defined later in this file:
 */

static int		GetPostscriptPoints _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string, double *doublePtr));

/*
 *--------------------------------------------------------------
 *
 * TkCanvPostscriptCmd --
 *
 *	This procedure is invoked to process the "postscript" options
 *	of the widget command for canvas widgets. See the user
 *	documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *--------------------------------------------------------------
 */

    /* ARGSUSED */
int
TkCanvPostscriptCmd(canvasPtr, interp, argc, argv)
    TkCanvas *canvasPtr;		/* Information about canvas widget. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings.  Caller has
					 * already parsed this command enough
					 * to know that argv[1] is
					 * "postscript". */
{
    TkPostscriptInfo psInfo, *oldInfoPtr;
    int result = TCL_ERROR;
    Tk_Item *itemPtr;
#define STRING_LENGTH 400
    char string[STRING_LENGTH+1], *p;
    time_t now;
#if !(defined(__WIN32__) || defined(MAC_TCL))
    struct passwd *pwPtr;
#endif /* __WIN32__ || MAC_TCL */
    FILE *f;
    size_t length;
    int deltaX = 0, deltaY = 0;		/* Offset of lower-left corner of
					 * area to be marked up, measured
					 * in canvas units from the positioning
					 * point on the page (reflects
					 * anchor position).  Initial values
					 * needed only to stop compiler
					 * warnings. */
    Tcl_HashSearch search;
    Tcl_HashEntry *hPtr;
    Tcl_DString buffer;
    char *libDir;

    /*
     *----------------------------------------------------------------
     * Initialize the data structure describing Postscript generation,
     * then process all the arguments to fill the data structure in.
     *----------------------------------------------------------------
     */

    oldInfoPtr = canvasPtr->psInfoPtr;
    canvasPtr->psInfoPtr = &psInfo;
    psInfo.x = canvasPtr->xOrigin;
    psInfo.y = canvasPtr->yOrigin;
    psInfo.width = -1;
    psInfo.height = -1;
    psInfo.pageXString = NULL;
    psInfo.pageYString = NULL;
    psInfo.pageX = 72*4.25;
    psInfo.pageY = 72*5.5;
    psInfo.pageWidthString = NULL;
    psInfo.pageHeightString = NULL;
    psInfo.scale = 1.0;
    psInfo.pageAnchor = TK_ANCHOR_CENTER;
    psInfo.rotate = 0;
    psInfo.fontVar = NULL;
    psInfo.colorVar = NULL;
    psInfo.colorMode = NULL;
    psInfo.colorLevel = 0;
    psInfo.fileName = NULL;
    psInfo.f = NULL;
    psInfo.prepass = 0;
    Tcl_InitHashTable(&psInfo.fontTable, TCL_STRING_KEYS);
    result = Tk_ConfigureWidget(canvasPtr->interp, canvasPtr->tkwin,
	    configSpecs, argc-2, argv+2, (char *) &psInfo,
	    TK_CONFIG_ARGV_ONLY);
    if (result != TCL_OK) {
	goto cleanup;
    }

    if (psInfo.width == -1) {
	psInfo.width = Tk_Width(canvasPtr->tkwin);
    }
    if (psInfo.height == -1) {
	psInfo.height = Tk_Height(canvasPtr->tkwin);
    }
    psInfo.x2 = psInfo.x + psInfo.width;
    psInfo.y2 = psInfo.y + psInfo.height;

    if (psInfo.pageXString != NULL) {
	if (GetPostscriptPoints(canvasPtr->interp, psInfo.pageXString,
		&psInfo.pageX) != TCL_OK) {
	    goto cleanup;
	}
    }
    if (psInfo.pageYString != NULL) {
	if (GetPostscriptPoints(canvasPtr->interp, psInfo.pageYString,
		&psInfo.pageY) != TCL_OK) {
	    goto cleanup;
	}
    }
    if (psInfo.pageWidthString != NULL) {
	if (GetPostscriptPoints(canvasPtr->interp, psInfo.pageWidthString,
		&psInfo.scale) != TCL_OK) {
	    goto cleanup;
	}
	psInfo.scale /= psInfo.width;
    } else if (psInfo.pageHeightString != NULL) {
	if (GetPostscriptPoints(canvasPtr->interp, psInfo.pageHeightString,
		&psInfo.scale) != TCL_OK) {
	    goto cleanup;
	}
	psInfo.scale /= psInfo.height;
    } else {
	psInfo.scale = (72.0/25.4)*WidthMMOfScreen(Tk_Screen(canvasPtr->tkwin));
	psInfo.scale /= WidthOfScreen(Tk_Screen(canvasPtr->tkwin));
    }
    switch (psInfo.pageAnchor) {
	case TK_ANCHOR_NW:
	case TK_ANCHOR_W:
	case TK_ANCHOR_SW:
	    deltaX = 0;
	    break;
	case TK_ANCHOR_N:
	case TK_ANCHOR_CENTER:
	case TK_ANCHOR_S:
	    deltaX = -psInfo.width/2;
	    break;
	case TK_ANCHOR_NE:
	case TK_ANCHOR_E:
	case TK_ANCHOR_SE:
	    deltaX = -psInfo.width;
	    break;
    }
    switch (psInfo.pageAnchor) {
	case TK_ANCHOR_NW:
	case TK_ANCHOR_N:
	case TK_ANCHOR_NE:
	    deltaY = - psInfo.height;
	    break;
	case TK_ANCHOR_W:
	case TK_ANCHOR_CENTER:
	case TK_ANCHOR_E:
	    deltaY = -psInfo.height/2;
	    break;
	case TK_ANCHOR_SW:
	case TK_ANCHOR_S:
	case TK_ANCHOR_SE:
	    deltaY = 0;
	    break;
    }

    if (psInfo.colorMode == NULL) {
	psInfo.colorLevel = 2;
    } else {
	length = strlen(psInfo.colorMode);
	if (strncmp(psInfo.colorMode, "monochrome", length) == 0) {
	    psInfo.colorLevel = 0;
	} else if (strncmp(psInfo.colorMode, "gray", length) == 0) {
	    psInfo.colorLevel = 1;
	} else if (strncmp(psInfo.colorMode, "color", length) == 0) {
	    psInfo.colorLevel = 2;
	} else {
	    Tcl_AppendResult(canvasPtr->interp, "bad color mode \"",
		    psInfo.colorMode, "\": must be monochrome, ",
		    "gray, or color", (char *) NULL);
	    goto cleanup;
	}
    }

    if (psInfo.fileName != NULL) {
	p = Tcl_TranslateFileName(canvasPtr->interp, psInfo.fileName, &buffer);
	if (p == NULL) {
	    goto cleanup;
	}
	psInfo.f = fopen(p, "w");
	Tcl_DStringFree(&buffer);
	if (psInfo.f == NULL) {
	    Tcl_AppendResult(canvasPtr->interp, "couldn't write file \"",
		    psInfo.fileName, "\": ",
		    Tcl_PosixError(canvasPtr->interp), (char *) NULL);
	    goto cleanup;
	}
    }

    /*
     *--------------------------------------------------------
     * Make a pre-pass over all of the items, generating Postscript
     * and then throwing it away.  The purpose of this pass is just
     * to collect information about all the fonts in use, so that
     * we can output font information in the proper form required
     * by the Document Structuring Conventions.
     *--------------------------------------------------------
     */

    psInfo.prepass = 1;
    for (itemPtr = canvasPtr->firstItemPtr; itemPtr != NULL;
	    itemPtr = itemPtr->nextPtr) {
	if ((itemPtr->x1 >= psInfo.x2) || (itemPtr->x2 < psInfo.x)
		|| (itemPtr->y1 >= psInfo.y2) || (itemPtr->y2 < psInfo.y)) {
	    continue;
	}
	if (itemPtr->typePtr->postscriptProc == NULL) {
	    continue;
	}
	result = (*itemPtr->typePtr->postscriptProc)(canvasPtr->interp,
		(Tk_Canvas) canvasPtr, itemPtr, 1);
	Tcl_ResetResult(canvasPtr->interp);
	if (result != TCL_OK) {
	    /*
	     * An error just occurred.  Just skip out of this loop.
	     * There's no need to report the error now;  it can be
	     * reported later (errors can happen later that don't
	     * happen now, so we still have to check for errors later
	     * anyway).
	     */
	    break;
	}
    }
    psInfo.prepass = 0;

    /*
     *--------------------------------------------------------
     * Generate the header and prolog for the Postscript.
     *--------------------------------------------------------
     */

    Tcl_AppendResult(canvasPtr->interp, "%!PS-Adobe-3.0 EPSF-3.0\n",
	    "%%Creator: Tk Canvas Widget\n", (char *) NULL);
#if !(defined(__WIN32__) || defined(MAC_TCL))
    pwPtr = getpwuid(getuid());
    Tcl_AppendResult(canvasPtr->interp, "%%For: ",
	    (pwPtr != NULL) ? pwPtr->pw_gecos : "Unknown", "\n",
	    (char *) NULL);
    endpwent();
#endif /* __WIN32__ || MAC_TCL */
    Tcl_AppendResult(canvasPtr->interp, "%%Title: Window ",
	    Tk_PathName(canvasPtr->tkwin), "\n", (char *) NULL);
    time(&now);
    Tcl_AppendResult(canvasPtr->interp, "%%CreationDate: ",
	    ctime(&now), (char *) NULL);
    if (!psInfo.rotate) {
	sprintf(string, "%d %d %d %d",
		(int) (psInfo.pageX + psInfo.scale*deltaX),
		(int) (psInfo.pageY + psInfo.scale*deltaY),
		(int) (psInfo.pageX + psInfo.scale*(deltaX + psInfo.width)
			+ 1.0),
		(int) (psInfo.pageY + psInfo.scale*(deltaY + psInfo.height)
			+ 1.0));
    } else {
	sprintf(string, "%d %d %d %d",
		(int) (psInfo.pageX - psInfo.scale*(deltaY + psInfo.height)),
		(int) (psInfo.pageY + psInfo.scale*deltaX),
		(int) (psInfo.pageX - psInfo.scale*deltaY + 1.0),
		(int) (psInfo.pageY + psInfo.scale*(deltaX + psInfo.width)
			+ 1.0));
    }
    Tcl_AppendResult(canvasPtr->interp, "%%BoundingBox: ", string,
	    "\n", (char *) NULL);
    Tcl_AppendResult(canvasPtr->interp, "%%Pages: 1\n", 
	    "%%DocumentData: Clean7Bit\n", (char *) NULL);
    Tcl_AppendResult(canvasPtr->interp, "%%Orientation: ",
	    psInfo.rotate ? "Landscape\n" : "Portrait\n", (char *) NULL);
    p = "%%DocumentNeededResources: font ";
    for (hPtr = Tcl_FirstHashEntry(&psInfo.fontTable, &search);
	    hPtr != NULL; hPtr = Tcl_NextHashEntry(&search)) {
	Tcl_AppendResult(canvasPtr->interp, p,
		Tcl_GetHashKey(&psInfo.fontTable, hPtr),
		"\n", (char *) NULL);
	p = "%%+ font ";
    }
    Tcl_AppendResult(canvasPtr->interp, "%%EndComments\n\n", (char *) NULL);

    /*
     * Read a standard prolog file from disk and insert it into
     * the Postscript.
     */

    libDir = Tcl_GetVar(canvasPtr->interp, "tk_library", TCL_GLOBAL_ONLY);
    if (libDir == NULL) {
	Tcl_ResetResult(canvasPtr->interp);
	Tcl_AppendResult(canvasPtr->interp, "couldn't find library directory: ",
		"tk_library variable doesn't exist", (char *) NULL);
	goto cleanup;
    }
    sprintf(string, "%.350s/prolog.ps", libDir);
    p = Tcl_TranslateFileName(canvasPtr->interp, string, &buffer);
    f = fopen(p, "r");
    Tcl_DStringFree(&buffer);
    if (f == NULL) {
	Tcl_ResetResult(canvasPtr->interp);
	Tcl_AppendResult(canvasPtr->interp, "couldn't open prolog file \"",
		string, "\": ", Tcl_PosixError(canvasPtr->interp),
		(char *) NULL);
	goto cleanup;
    }
    while (fgets(string, STRING_LENGTH, f) != NULL) {
	Tcl_AppendResult(canvasPtr->interp, string, (char *) NULL);
    }
    if (ferror(f)) {
	fclose(f);
	Tcl_ResetResult(canvasPtr->interp);
	Tcl_AppendResult(canvasPtr->interp, "error reading prolog file \"",
		string, "\": ",
		Tcl_PosixError(canvasPtr->interp), (char *) NULL);
	goto cleanup;
    }
    fclose(f);
    if (psInfo.f != NULL) {
	fputs(canvasPtr->interp->result, psInfo.f);
	Tcl_ResetResult(canvasPtr->interp);
    }

    /*
     *-----------------------------------------------------------
     * Document setup:  set the color level and include fonts.
     *-----------------------------------------------------------
     */

    sprintf(string, "/CL %d def\n", psInfo.colorLevel);
    Tcl_AppendResult(canvasPtr->interp, "%%BeginSetup\n", string,
	    (char *) NULL);
    for (hPtr = Tcl_FirstHashEntry(&psInfo.fontTable, &search);
	    hPtr != NULL; hPtr = Tcl_NextHashEntry(&search)) {
	Tcl_AppendResult(canvasPtr->interp, "%%IncludeResource: font ",
		Tcl_GetHashKey(&psInfo.fontTable, hPtr), "\n", (char *) NULL);
    }
    Tcl_AppendResult(canvasPtr->interp, "%%EndSetup\n\n", (char *) NULL);

    /*
     *-----------------------------------------------------------
     * Page setup:  move to page positioning point, rotate if
     * needed, set scale factor, offset for proper anchor position,
     * and set clip region.
     *-----------------------------------------------------------
     */

    Tcl_AppendResult(canvasPtr->interp, "%%Page: 1 1\n", "save\n",
	    (char *) NULL);
    sprintf(string, "%.1f %.1f translate\n", psInfo.pageX, psInfo.pageY);
    Tcl_AppendResult(canvasPtr->interp, string, (char *) NULL);
    if (psInfo.rotate) {
	Tcl_AppendResult(canvasPtr->interp, "90 rotate\n", (char *) NULL);
    }
    sprintf(string, "%.4g %.4g scale\n", psInfo.scale, psInfo.scale);
    Tcl_AppendResult(canvasPtr->interp, string, (char *) NULL);
    sprintf(string, "%d %d translate\n", deltaX - psInfo.x, deltaY);
    Tcl_AppendResult(canvasPtr->interp, string, (char *) NULL);
    sprintf(string, "%d %.15g moveto %d %.15g lineto %d %.15g lineto %d %.15g",
	    psInfo.x, Tk_CanvasPsY((Tk_Canvas) canvasPtr, (double) psInfo.y),
	    psInfo.x2, Tk_CanvasPsY((Tk_Canvas) canvasPtr, (double) psInfo.y),
	    psInfo.x2, Tk_CanvasPsY((Tk_Canvas) canvasPtr, (double) psInfo.y2),
	    psInfo.x, Tk_CanvasPsY((Tk_Canvas) canvasPtr, (double) psInfo.y2));
    Tcl_AppendResult(canvasPtr->interp, string,
	" lineto closepath clip newpath\n", (char *) NULL);
    if (psInfo.f != NULL) {
	fputs(canvasPtr->interp->result, psInfo.f);
	Tcl_ResetResult(canvasPtr->interp);
    }

    /*
     *---------------------------------------------------------------------
     * Iterate through all the items, having each relevant one draw itself.
     * Quit if any of the items returns an error.
     *---------------------------------------------------------------------
     */

    result = TCL_OK;
    for (itemPtr = canvasPtr->firstItemPtr; itemPtr != NULL;
	    itemPtr = itemPtr->nextPtr) {
	if ((itemPtr->x1 >= psInfo.x2) || (itemPtr->x2 < psInfo.x)
		|| (itemPtr->y1 >= psInfo.y2) || (itemPtr->y2 < psInfo.y)) {
	    continue;
	}
	if (itemPtr->typePtr->postscriptProc == NULL) {
	    continue;
	}
	Tcl_AppendResult(canvasPtr->interp, "gsave\n", (char *) NULL);
	result = (*itemPtr->typePtr->postscriptProc)(canvasPtr->interp,
		(Tk_Canvas) canvasPtr, itemPtr, 0);
	if (result != TCL_OK) {
	    char msg[100];

	    sprintf(msg, "\n    (generating Postscript for item %d)",
		    itemPtr->id);
	    Tcl_AddErrorInfo(canvasPtr->interp, msg);
	    goto cleanup;
	}
	Tcl_AppendResult(canvasPtr->interp, "grestore\n", (char *) NULL);
	if (psInfo.f != NULL) {
	    fputs(canvasPtr->interp->result, psInfo.f);
	    Tcl_ResetResult(canvasPtr->interp);
	}
    }

    /*
     *---------------------------------------------------------------------
     * Output page-end information, such as commands to print the page
     * and document trailer stuff.
     *---------------------------------------------------------------------
     */

    Tcl_AppendResult(canvasPtr->interp, "restore showpage\n\n",
	    "%%Trailer\nend\n%%EOF\n", (char *) NULL);
    if (psInfo.f != NULL) {
	fputs(canvasPtr->interp->result, psInfo.f);
	Tcl_ResetResult(canvasPtr->interp);
    }

    /*
     * Clean up psInfo to release malloc'ed stuff.
     */

    cleanup:
    if (psInfo.pageXString != NULL) {
	ckfree(psInfo.pageXString);
    }
    if (psInfo.pageYString != NULL) {
	ckfree(psInfo.pageYString);
    }
    if (psInfo.pageWidthString != NULL) {
	ckfree(psInfo.pageWidthString);
    }
    if (psInfo.pageHeightString != NULL) {
	ckfree(psInfo.pageHeightString);
    }
    if (psInfo.fontVar != NULL) {
	ckfree(psInfo.fontVar);
    }
    if (psInfo.colorVar != NULL) {
	ckfree(psInfo.colorVar);
    }
    if (psInfo.colorMode != NULL) {
	ckfree(psInfo.colorMode);
    }
    if (psInfo.fileName != NULL) {
	ckfree(psInfo.fileName);
    }
    if (psInfo.f != NULL) {
	fclose(psInfo.f);
    }
    Tcl_DeleteHashTable(&psInfo.fontTable);
    canvasPtr->psInfoPtr = oldInfoPtr;
    return result;
}

/*
 *--------------------------------------------------------------
 *
 * Tk_CanvasPsColor --
 *
 *	This procedure is called by individual canvas items when
 *	they want to set a color value for output.  Given information
 *	about an X color, this procedure will generate Postscript
 *	commands to set up an appropriate color in Postscript.
 *
 * Results:
 *	Returns a standard Tcl return value.  If an error occurs
 *	then an error message will be left in interp->result.
 *	If no error occurs, then additional Postscript will be
 *	appended to interp->result.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

int
Tk_CanvasPsColor(interp, canvas, colorPtr)
    Tcl_Interp *interp;			/* Interpreter for returning Postscript
					 * or error message. */
    Tk_Canvas canvas;			/* Information about canvas. */
    XColor *colorPtr;			/* Information about color. */
{
    TkCanvas *canvasPtr = (TkCanvas *) canvas;
    TkPostscriptInfo *psInfoPtr = canvasPtr->psInfoPtr;
    int tmp;
    double red, green, blue;
    char string[200];

    if (psInfoPtr->prepass) {
	return TCL_OK;
    }

    /*
     * If there is a color map defined, then look up the color's name
     * in the map and use the Postscript commands found there, if there
     * are any.
     */

    if (psInfoPtr->colorVar != NULL) {
	char *cmdString;

	cmdString = Tcl_GetVar2(interp, psInfoPtr->colorVar,
		Tk_NameOfColor(colorPtr), 0);
	if (cmdString != NULL) {
	    Tcl_AppendResult(interp, cmdString, "\n", (char *) NULL);
	    return TCL_OK;
	}
    }

    /*
     * No color map entry for this color.  Grab the color's intensities
     * and output Postscript commands for them.  Special note:  X uses
     * a range of 0-65535 for intensities, but most displays only use
     * a range of 0-255, which maps to (0, 256, 512, ... 65280) in the
     * X scale.  This means that there's no way to get perfect white,
     * since the highest intensity is only 65280 out of 65535.  To
     * work around this problem, rescale the X intensity to a 0-255
     * scale and use that as the basis for the Postscript colors.  This
     * scheme still won't work if the display only uses 4 bits per color,
     * but most diplays use at least 8 bits.
     */

    tmp = colorPtr->red;
    red = ((double) (tmp >> 8))/255.0;
    tmp = colorPtr->green;
    green = ((double) (tmp >> 8))/255.0;
    tmp = colorPtr->blue;
    blue = ((double) (tmp >> 8))/255.0;
    sprintf(string, "%.3f %.3f %.3f setrgbcolor AdjustColor\n",
	    red, green, blue);
    Tcl_AppendResult(interp, string, (char *) NULL);
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * Tk_CanvasPsFont --
 *
 *	This procedure is called by individual canvas items when
 *	they want to output text.  Given information about an X
 *	font, this procedure will generate Postscript commands
 *	to set up an appropriate font in Postscript.
 *
 * Results:
 *	Returns a standard Tcl return value.  If an error occurs
 *	then an error message will be left in interp->result.
 *	If no error occurs, then additional Postscript will be
 *	appended to the interp->result.
 *
 * Side effects:
 *	The Postscript font name is entered into psInfoPtr->fontTable
 *	if it wasn't already there.
 *
 *--------------------------------------------------------------
 */

int
Tk_CanvasPsFont(interp, canvas, fontStructPtr)
    Tcl_Interp *interp;			/* Interpreter for returning Postscript
					 * or error message. */
    Tk_Canvas canvas;			/* Information about canvas. */
    XFontStruct *fontStructPtr;		/* Information about font in which text
					 * is to be printed. */
{
    TkCanvas *canvasPtr = (TkCanvas *) canvas;
    TkPostscriptInfo *psInfoPtr = canvasPtr->psInfoPtr;
    char *name, *end, *weightString, *slantString;
#define TOTAL_FIELDS	8
#define FAMILY_FIELD	1
#define WEIGHT_FIELD	2
#define SLANT_FIELD	3
#define SIZE_FIELD	7
    char *fieldPtrs[TOTAL_FIELDS];
#define MAX_NAME_SIZE 100
    char fontName[MAX_NAME_SIZE+50], pointString[20];
    int i, c, weightSize, nameSize, points;
    char *p;

    name = Tk_NameOfFontStruct(fontStructPtr);

    /*
     * First, look up the font's name in the font map, if there is one.
     * If there is an entry for this font, it consists of a list
     * containing font name and size.  Use this information.
     */

    if (psInfoPtr->fontVar != NULL) {
	char *list, **argv;
	int argc;
	double size;

	list = Tcl_GetVar2(interp, psInfoPtr->fontVar,
		name, 0);
	if (list != NULL) {
	    if (Tcl_SplitList(interp, list, &argc, &argv) != TCL_OK) {
		badMapEntry:
		Tcl_ResetResult(interp);
		Tcl_AppendResult(interp, "bad font map entry for \"", name,
			"\": \"", list, "\"", (char *) NULL);
		return TCL_ERROR;
	    }
	    if (argc != 2) {
		goto badMapEntry;
	    }
	    size = strtod(argv[1], &end);
	    if ((size <= 0) || (*end != 0)) {
		goto badMapEntry;
	    }
	    sprintf(pointString, "%.15g", size);
	    Tcl_AppendResult(interp, "/", argv[0], " findfont ",
		    pointString, " scalefont ", (char *) NULL);
	    if (strncasecmp(argv[0], "Symbol", 7) != 0) {
		Tcl_AppendResult(interp, "ISOEncode ", (char *) NULL);
	    }
	    Tcl_AppendResult(interp, "setfont\n", (char *) NULL);
	    Tcl_CreateHashEntry(&psInfoPtr->fontTable, argv[0], &i);
	    ckfree((char *) argv);
	    return TCL_OK;
	}
    }

    /*
     * Not in the font map.  Try to parse the name to get four fields:
     * family name, weight, slant, and point size.  To do this, split the
     * font name up into fields, storing pointers to the first character
     * of each field in fieldPtrs.
     */

    if (name[0] != '-') {
	goto error;
    }
    for (p =  name+1, i = 0; i < TOTAL_FIELDS; i++) {
	fieldPtrs[i] = p;
	while (*p != '-') {
	    if (*p == 0) {
		goto error;
	    }
	    p++;
	}
	p++;
    }

    /*
     * Use the information from the X font name to make a guess at a
     * Postscript font name of the form "<family>-<weight><slant>" where
     * <weight> and <slant> may be omitted and if both are omitted then
     * the dash is also omitted.  Postscript is very picky about font names,
     * so there are several heuristics in the code below (e.g. don't
     * include a "Roman" slant except for "Times" font, and make sure
     * that the first letter of each field is capitalized but no other
     * letters are in caps).
     */

    nameSize = fieldPtrs[FAMILY_FIELD+1] - 1 - fieldPtrs[FAMILY_FIELD];
    if ((nameSize == 0) || (nameSize > MAX_NAME_SIZE)) {
	goto error;
    }
    strncpy(fontName, fieldPtrs[FAMILY_FIELD], (size_t) nameSize);
    if (islower(UCHAR(fontName[0]))) {
	fontName[0] = toupper(UCHAR(fontName[0]));
    }
    for (p = fontName+1, i = nameSize-1; i > 0; p++, i--) {
	if (isupper(UCHAR(*p))) {
	    *p = tolower(UCHAR(*p));
	}
    }
    *p = 0;
    weightSize = fieldPtrs[WEIGHT_FIELD+1] - 1 - fieldPtrs[WEIGHT_FIELD];
    if (weightSize == 0) {
	goto error;
    }
    if (strncasecmp(fieldPtrs[WEIGHT_FIELD], "medium",
	    (size_t) weightSize) == 0) {
	weightString = "";
    } else if (strncasecmp(fieldPtrs[WEIGHT_FIELD], "bold",
	    (size_t) weightSize) == 0) {
	weightString = "Bold";
    } else {
	goto error;
    }
    if (fieldPtrs[SLANT_FIELD+1] != (fieldPtrs[SLANT_FIELD] + 2)) {
	goto error;
    }
    c = fieldPtrs[SLANT_FIELD][0];
    if ((c == 'r') || (c == 'R')) {
	slantString = "";
	if ((weightString[0] == 0) && (nameSize == 5)
		&& (strncmp(fontName, "Times", 5) == 0)) {
	    slantString = "Roman";
	}
    } else if ((c == 'i') || (c == 'I')) {
	slantString = "Italic";
    } else if ((c == 'o') || (c == 'O')) {
	slantString = "Oblique";
    } else {
	goto error;
    }
    if ((weightString[0] != 0) || (slantString[0] != 0)) {
	sprintf(p, "-%s%s", weightString, slantString);
    }
    points = strtoul(fieldPtrs[SIZE_FIELD], &end, 0);
    if (points == 0) {
	goto error;
    }
    sprintf(pointString, "%.15g", ((double) points)/10.0);
    Tcl_AppendResult(interp, "/", fontName, " findfont ",
	    pointString, " scalefont ", (char *) NULL);
    if (strcmp(fontName, "Symbol") != 0) {
	Tcl_AppendResult(interp, "ISOEncode ", (char *) NULL);
    }
    Tcl_AppendResult(interp, "setfont\n", (char *) NULL);
    Tcl_CreateHashEntry(&psInfoPtr->fontTable, fontName, &i);
    return TCL_OK;

    error:
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, "couldn't translate font name \"",
	    name, "\" to Postscript", (char *) NULL);
    return TCL_ERROR;
}

/*
 *--------------------------------------------------------------
 *
 * Tk_CanvasPsBitmap --
 *
 *	This procedure is called to output the contents of a
 *	sub-region of a bitmap in proper image data format for
 *	Postscript (i.e. data between angle brackets, one bit
 *	per pixel).
 *
 * Results:
 *	Returns a standard Tcl return value.  If an error occurs
 *	then an error message will be left in interp->result.
 *	If no error occurs, then additional Postscript will be
 *	appended to interp->result.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

int
Tk_CanvasPsBitmap(interp, canvas, bitmap, startX, startY, width, height)
    Tcl_Interp *interp;			/* Interpreter for returning Postscript
					 * or error message. */
    Tk_Canvas canvas;			/* Information about canvas. */
    Pixmap bitmap;			/* Bitmap for which to generate
					 * Postscript. */
    int startX, startY;			/* Coordinates of upper-left corner
					 * of rectangular region to output. */
    int width, height;			/* Height of rectangular region. */
{
    TkCanvas *canvasPtr = (TkCanvas *) canvas;
    TkPostscriptInfo *psInfoPtr = canvasPtr->psInfoPtr;
    XImage *imagePtr;
    int charsInLine, x, y, lastX, lastY, value, mask;
    unsigned int totalWidth, totalHeight;
    char string[100];
    Window dummyRoot;
    int dummyX, dummyY;
    unsigned dummyBorderwidth, dummyDepth;

    if (psInfoPtr->prepass) {
	return TCL_OK;
    }

    /*
     * The following call should probably be a call to Tk_SizeOfBitmap
     * instead, but it seems that we are occasionally invoked by custom
     * item types that create their own bitmaps without registering them
     * with Tk.  XGetGeometry is a bit slower than Tk_SizeOfBitmap, but
     * it shouldn't matter here.
     */

    XGetGeometry(Tk_Display(Tk_CanvasTkwin(canvas)), bitmap, &dummyRoot,
	    (int *) &dummyX, (int *) &dummyY, (unsigned int *) &totalWidth,
	    (unsigned int *) &totalHeight, &dummyBorderwidth, &dummyDepth);
    imagePtr = XGetImage(Tk_Display(canvasPtr->tkwin), bitmap, 0, 0,
	    totalWidth, totalHeight, 1, XYPixmap);
    Tcl_AppendResult(interp, "<", (char *) NULL);
    mask = 0x80;
    value = 0;
    charsInLine = 0;
    lastX = startX + width - 1;
    lastY = startY + height - 1;
    for (y = lastY; y >= startY; y--) {
	for (x = startX; x <= lastX; x++) {
	    if (XGetPixel(imagePtr, x, y)) {
		value |= mask;
	    }
	    mask >>= 1;
	    if (mask == 0) {
		sprintf(string, "%02x", value);
		Tcl_AppendResult(interp, string, (char *) NULL);
		mask = 0x80;
		value = 0;
		charsInLine += 2;
		if (charsInLine >= 60) {
		    Tcl_AppendResult(interp, "\n", (char *) NULL);
		    charsInLine = 0;
		}
	    }
	}
	if (mask != 0x80) {
	    sprintf(string, "%02x", value);
	    Tcl_AppendResult(interp, string, (char *) NULL);
	    mask = 0x80;
	    value = 0;
	    charsInLine += 2;
	}
    }
    Tcl_AppendResult(interp, ">", (char *) NULL);
    XDestroyImage(imagePtr);
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * Tk_CanvasPsStipple --
 *
 *	This procedure is called by individual canvas items when
 *	they have created a path that they'd like to be filled with
 *	a stipple pattern.  Given information about an X bitmap,
 *	this procedure will generate Postscript commands to fill
 *	the current clip region using a stipple pattern defined by the
 *	bitmap.
 *
 * Results:
 *	Returns a standard Tcl return value.  If an error occurs
 *	then an error message will be left in interp->result.
 *	If no error occurs, then additional Postscript will be
 *	appended to interp->result.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

int
Tk_CanvasPsStipple(interp, canvas, bitmap)
    Tcl_Interp *interp;			/* Interpreter for returning Postscript
					 * or error message. */
    Tk_Canvas canvas;			/* Information about canvas. */
    Pixmap bitmap;			/* Bitmap to use for stippling. */
{
    TkCanvas *canvasPtr = (TkCanvas *) canvas;
    TkPostscriptInfo *psInfoPtr = canvasPtr->psInfoPtr;
    int width, height;
    char string[100];
    Window dummyRoot;
    int dummyX, dummyY;
    unsigned dummyBorderwidth, dummyDepth;

    if (psInfoPtr->prepass) {
	return TCL_OK;
    }

    /*
     * The following call should probably be a call to Tk_SizeOfBitmap
     * instead, but it seems that we are occasionally invoked by custom
     * item types that create their own bitmaps without registering them
     * with Tk.  XGetGeometry is a bit slower than Tk_SizeOfBitmap, but
     * it shouldn't matter here.
     */

    XGetGeometry(Tk_Display(Tk_CanvasTkwin(canvas)), bitmap, &dummyRoot,
	    (int *) &dummyX, (int *) &dummyY, (unsigned *) &width,
	    (unsigned *) &height, &dummyBorderwidth, &dummyDepth);
    sprintf(string, "%d %d ", width, height);
    Tcl_AppendResult(interp, string, (char *) NULL);
    if (Tk_CanvasPsBitmap(interp, (Tk_Canvas) canvasPtr, bitmap, 0, 0,
	    width, height) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_AppendResult(interp, " StippleFill\n", (char *) NULL);
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * Tk_CanvasPsY --
 *
 *	Given a y-coordinate in canvas coordinates, this procedure
 *	returns a y-coordinate to use for Postscript output.
 *
 * Results:
 *	Returns the Postscript coordinate that corresponds to
 *	"y".
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

double
Tk_CanvasPsY(canvas, y)
    Tk_Canvas canvas;			/* Token for canvas on whose behalf
					 * Postscript is being generated. */
    double y;				/* Y-coordinate in canvas coords. */
{
    TkPostscriptInfo *psInfoPtr = ((TkCanvas *) canvas)->psInfoPtr;

    return psInfoPtr->y2 - y;
}

/*
 *--------------------------------------------------------------
 *
 * Tk_CanvasPsPath --
 *
 *	Given an array of points for a path, generate Postscript
 *	commands to create the path.
 *
 * Results:
 *	Postscript commands get appended to what's in interp->result.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

void
Tk_CanvasPsPath(interp, canvas, coordPtr, numPoints)
    Tcl_Interp *interp;			/* Put generated Postscript in this
					 * interpreter's result field. */
    Tk_Canvas canvas;			/* Canvas on whose behalf Postscript
					 * is being generated. */
    double *coordPtr;			/* Pointer to first in array of
					 * 2*numPoints coordinates giving
					 * points for path. */
    int numPoints;			/* Number of points at *coordPtr. */
{
    TkPostscriptInfo *psInfoPtr = ((TkCanvas *) canvas)->psInfoPtr;
    char buffer[200];

    if (psInfoPtr->prepass) {
	return;
    }
    sprintf(buffer, "%.15g %.15g moveto\n", coordPtr[0],
	    Tk_CanvasPsY(canvas, coordPtr[1]));
    Tcl_AppendResult(interp, buffer, (char *) NULL);
    for (numPoints--, coordPtr += 2; numPoints > 0;
	    numPoints--, coordPtr += 2) {
	sprintf(buffer, "%.15g %.15g lineto\n", coordPtr[0],
		Tk_CanvasPsY(canvas, coordPtr[1]));
	Tcl_AppendResult(interp, buffer, (char *) NULL);
    }
}

/*
 *--------------------------------------------------------------
 *
 * GetPostscriptPoints --
 *
 *	Given a string, returns the number of Postscript points
 *	corresponding to that string.
 *
 * Results:
 *	The return value is a standard Tcl return result.  If
 *	TCL_OK is returned, then everything went well and the
 *	screen distance is stored at *doublePtr;  otherwise
 *	TCL_ERROR is returned and an error message is left in
 *	interp->result.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static int
GetPostscriptPoints(interp, string, doublePtr)
    Tcl_Interp *interp;		/* Use this for error reporting. */
    char *string;		/* String describing a screen distance. */
    double *doublePtr;		/* Place to store converted result. */
{
    char *end;
    double d;

    d = strtod(string, &end);
    if (end == string) {
	error:
	Tcl_AppendResult(interp, "bad distance \"", string,
		"\"", (char *) NULL);
	return TCL_ERROR;
    }
    while ((*end != '\0') && isspace(UCHAR(*end))) {
	end++;
    }
    switch (*end) {
	case 'c':
	    d *= 72.0/2.54;
	    end++;
	    break;
	case 'i':
	    d *= 72.0;
	    end++;
	    break;
	case 'm':
	    d *= 72.0/25.4;
	    end++;
	    break;
	case 0:
	    break;
	case 'p':
	    end++;
	    break;
	default:
	    goto error;
    }
    while ((*end != '\0') && isspace(UCHAR(*end))) {
	end++;
    }
    if (*end != 0) {
	goto error;
    }
    *doublePtr = d;
    return TCL_OK;
}
