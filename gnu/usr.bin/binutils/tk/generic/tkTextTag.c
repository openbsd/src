/* 
 * tkTextTag.c --
 *
 *	This module implements the "tag" subcommand of the widget command
 *	for text widgets, plus most of the other high-level functions
 *	related to tags.
 *
 * Copyright (c) 1992-1994 The Regents of the University of California.
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkTextTag.c 1.36 96/03/07 15:30:43
 */

#include "default.h"
#include "tkPort.h"
#include "tk.h"
#include "tkText.h"

/*
 * Information used for parsing tag configuration information:
 */

static Tk_ConfigSpec tagConfigSpecs[] = {
    {TK_CONFIG_BORDER, "-background", (char *) NULL, (char *) NULL,
	(char *) NULL, Tk_Offset(TkTextTag, border), TK_CONFIG_NULL_OK},
    {TK_CONFIG_BITMAP, "-bgstipple", (char *) NULL, (char *) NULL,
	(char *) NULL, Tk_Offset(TkTextTag, bgStipple), TK_CONFIG_NULL_OK},
    {TK_CONFIG_STRING, "-borderwidth", (char *) NULL, (char *) NULL,
	"0", Tk_Offset(TkTextTag, bdString),
		TK_CONFIG_DONT_SET_DEFAULT|TK_CONFIG_NULL_OK},
    {TK_CONFIG_BITMAP, "-fgstipple", (char *) NULL, (char *) NULL,
	(char *) NULL, Tk_Offset(TkTextTag, fgStipple), TK_CONFIG_NULL_OK},
    {TK_CONFIG_FONT, "-font", (char *) NULL, (char *) NULL,
	(char *) NULL, Tk_Offset(TkTextTag, fontPtr), TK_CONFIG_NULL_OK},
    {TK_CONFIG_COLOR, "-foreground", (char *) NULL, (char *) NULL,
	(char *) NULL, Tk_Offset(TkTextTag, fgColor), TK_CONFIG_NULL_OK},
    {TK_CONFIG_STRING, "-justify", (char *) NULL, (char *) NULL,
	(char *) NULL, Tk_Offset(TkTextTag, justifyString), TK_CONFIG_NULL_OK},
    {TK_CONFIG_STRING, "-lmargin1", (char *) NULL, (char *) NULL,
	(char *) NULL, Tk_Offset(TkTextTag, lMargin1String), TK_CONFIG_NULL_OK},
    {TK_CONFIG_STRING, "-lmargin2", (char *) NULL, (char *) NULL,
	(char *) NULL, Tk_Offset(TkTextTag, lMargin2String), TK_CONFIG_NULL_OK},
    {TK_CONFIG_STRING, "-offset", (char *) NULL, (char *) NULL,
	(char *) NULL, Tk_Offset(TkTextTag, offsetString), TK_CONFIG_NULL_OK},
    {TK_CONFIG_STRING, "-overstrike", (char *) NULL, (char *) NULL,
	(char *) NULL, Tk_Offset(TkTextTag, overstrikeString),
	TK_CONFIG_NULL_OK},
    {TK_CONFIG_STRING, "-relief", (char *) NULL, (char *) NULL,
	(char *) NULL, Tk_Offset(TkTextTag, reliefString), TK_CONFIG_NULL_OK},
    {TK_CONFIG_STRING, "-rmargin", (char *) NULL, (char *) NULL,
	(char *) NULL, Tk_Offset(TkTextTag, rMarginString), TK_CONFIG_NULL_OK},
    {TK_CONFIG_STRING, "-spacing1", (char *) NULL, (char *) NULL,
	(char *) NULL, Tk_Offset(TkTextTag, spacing1String), TK_CONFIG_NULL_OK},
    {TK_CONFIG_STRING, "-spacing2", (char *) NULL, (char *) NULL,
	(char *) NULL, Tk_Offset(TkTextTag, spacing2String), TK_CONFIG_NULL_OK},
    {TK_CONFIG_STRING, "-spacing3", (char *) NULL, (char *) NULL,
	(char *) NULL, Tk_Offset(TkTextTag, spacing3String), TK_CONFIG_NULL_OK},
    {TK_CONFIG_STRING, "-tabs", (char *) NULL, (char *) NULL,
	(char *) NULL, Tk_Offset(TkTextTag, tabString), TK_CONFIG_NULL_OK},
    {TK_CONFIG_STRING, "-underline", (char *) NULL, (char *) NULL,
	(char *) NULL, Tk_Offset(TkTextTag, underlineString),
	TK_CONFIG_NULL_OK},
    {TK_CONFIG_UID, "-wrap", (char *) NULL, (char *) NULL,
	(char *) NULL, Tk_Offset(TkTextTag, wrapMode),
	TK_CONFIG_NULL_OK},
    {TK_CONFIG_END, (char *) NULL, (char *) NULL, (char *) NULL,
	(char *) NULL, 0, 0}
};

/*
 * Forward declarations for procedures defined later in this file:
 */

static void		ChangeTagPriority _ANSI_ARGS_((TkText *textPtr,
			    TkTextTag *tagPtr, int prio));
static TkTextTag *	FindTag _ANSI_ARGS_((Tcl_Interp *interp,
			    TkText *textPtr, char *tagName));
static void		SortTags _ANSI_ARGS_((int numTags,
			    TkTextTag **tagArrayPtr));
static int		TagSortProc _ANSI_ARGS_((CONST VOID *first,
			    CONST VOID *second));

/*
 *--------------------------------------------------------------
 *
 * TkTextTagCmd --
 *
 *	This procedure is invoked to process the "tag" options of
 *	the widget command for text widgets. See the user documentation
 *	for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *--------------------------------------------------------------
 */

int
TkTextTagCmd(textPtr, interp, argc, argv)
    register TkText *textPtr;	/* Information about text widget. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings.  Someone else has already
				 * parsed this command enough to know that
				 * argv[1] is "tag". */
{
    int c, i, addTag;
    size_t length;
    char *fullOption;
    register TkTextTag *tagPtr;
    TkTextIndex first, last, index1, index2;

    if (argc < 3) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " tag option ?arg arg ...?\"", (char *) NULL);
	return TCL_ERROR;
    }
    c = argv[2][0];
    length = strlen(argv[2]);
    if ((c == 'a') && (strncmp(argv[2], "add", length) == 0)) {
	fullOption = "add";
	addTag = 1;

	addAndRemove:
	if (argc < 5) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " tag ", fullOption,
		    " tagName index1 ?index2 index1 index2 ...?\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	tagPtr = TkTextCreateTag(textPtr, argv[3]);
	for (i = 4; i < argc; i += 2) {
	    if (TkTextGetIndex(interp, textPtr, argv[i], &index1) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (argc > (i+1)) {
		if (TkTextGetIndex(interp, textPtr, argv[i+1], &index2)
			!= TCL_OK) {
		    return TCL_ERROR;
		}
		if (TkTextIndexCmp(&index1, &index2) >= 0) {
		    return TCL_OK;
		}
	    } else {
		index2 = index1;
		TkTextIndexForwChars(&index2, 1, &index2);
	    }
    
	    if (tagPtr->affectsDisplay) {
		TkTextRedrawTag(textPtr, &index1, &index2, tagPtr, !addTag);
	    } else {
		/*
		 * Still need to trigger enter/leave events on tags that
		 * have changed.
		 */
    
		TkTextEventuallyRepick(textPtr);
	    }
	    TkBTreeTag(&index1, &index2, tagPtr, addTag);
    
	    /*
	     * If the tag is "sel" then grab the selection if we're supposed
	     * to export it and don't already have it.  Also, invalidate
	     * partially-completed selection retrievals.
	     */
    
	    if (tagPtr == textPtr->selTagPtr) {
		if (addTag && textPtr->exportSelection
			&& !(textPtr->flags & GOT_SELECTION)) {
		    Tk_OwnSelection(textPtr->tkwin, XA_PRIMARY,
			    TkTextLostSelection, (ClientData) textPtr);
		    textPtr->flags |= GOT_SELECTION;
		}
		textPtr->abortSelections = 1;
	    }
	}
    } else if ((c == 'b') && (strncmp(argv[2], "bind", length) == 0)) {
	if ((argc < 4) || (argc > 6)) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " tag bind tagName ?sequence? ?command?\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	tagPtr = TkTextCreateTag(textPtr, argv[3]);

	/*
	 * Make a binding table if the widget doesn't already have
	 * one.
	 */

	if (textPtr->bindingTable == NULL) {
	    textPtr->bindingTable = Tk_CreateBindingTable(interp);
	}

	if (argc == 6) {
	    int append = 0;
	    unsigned long mask;

	    if (argv[5][0] == 0) {
		return Tk_DeleteBinding(interp, textPtr->bindingTable,
			(ClientData) tagPtr, argv[4]);
	    }
	    if (argv[5][0] == '+') {
		argv[5]++;
		append = 1;
	    }
	    mask = Tk_CreateBinding(interp, textPtr->bindingTable,
		    (ClientData) tagPtr, argv[4], argv[5], append);
	    if (mask == 0) {
		return TCL_ERROR;
	    }
	    if (mask & (unsigned) ~(ButtonMotionMask|Button1MotionMask
		    |Button2MotionMask|Button3MotionMask|Button4MotionMask
		    |Button5MotionMask|ButtonPressMask|ButtonReleaseMask
		    |EnterWindowMask|LeaveWindowMask|KeyPressMask
		    |KeyReleaseMask|PointerMotionMask)) {
		Tk_DeleteBinding(interp, textPtr->bindingTable,
			(ClientData) tagPtr, argv[4]);
		Tcl_ResetResult(interp);
		Tcl_AppendResult(interp, "requested illegal events; ",
			"only key, button, motion, and enter/leave ",
			"events may be used", (char *) NULL);
		return TCL_ERROR;
	    }
	} else if (argc == 5) {
	    char *command;
    
	    command = Tk_GetBinding(interp, textPtr->bindingTable,
		    (ClientData) tagPtr, argv[4]);
	    if (command == NULL) {
		return TCL_ERROR;
	    }
	    interp->result = command;
	} else {
	    Tk_GetAllBindings(interp, textPtr->bindingTable,
		    (ClientData) tagPtr);
	}
    } else if ((c == 'c') && (strncmp(argv[2], "cget", length) == 0)
	    && (length >= 2)) {
	if (argc != 5) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " tag cget tagName option\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	tagPtr = FindTag(interp, textPtr, argv[3]);
	if (tagPtr == NULL) {
	    return TCL_ERROR;
	}
	return Tk_ConfigureValue(interp, textPtr->tkwin, tagConfigSpecs,
		(char *) tagPtr, argv[4], 0);
    } else if ((c == 'c') && (strncmp(argv[2], "configure", length) == 0)
	    && (length >= 2)) {
	if (argc < 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " tag configure tagName ?option? ?value? ",
		    "?option value ...?\"", (char *) NULL);
	    return TCL_ERROR;
	}
	tagPtr = TkTextCreateTag(textPtr, argv[3]);
	if (argc == 4) {
	    return Tk_ConfigureInfo(interp, textPtr->tkwin, tagConfigSpecs,
		    (char *) tagPtr, (char *) NULL, 0);
	} else if (argc == 5) {
	    return Tk_ConfigureInfo(interp, textPtr->tkwin, tagConfigSpecs,
		    (char *) tagPtr, argv[4], 0);
	} else {
	    int result;

	    result = Tk_ConfigureWidget(interp, textPtr->tkwin, tagConfigSpecs,
		    argc-4, argv+4, (char *) tagPtr, 0);
	    /*
	     * Some of the configuration options, like -underline
	     * and -justify, require additional translation (this is
	     * needed because we need to distinguish a particular value
	     * of an option from "unspecified").
	     */

	    if (tagPtr->bdString != NULL) {
		if (Tk_GetPixels(interp, textPtr->tkwin, tagPtr->bdString,
			&tagPtr->borderWidth) != TCL_OK) {
		    return TCL_ERROR;
		}
		if (tagPtr->borderWidth < 0) {
		    tagPtr->borderWidth = 0;
		}
	    }
	    if (tagPtr->reliefString != NULL) {
		if (Tk_GetRelief(interp, tagPtr->reliefString,
			&tagPtr->relief) != TCL_OK) {
		    return TCL_ERROR;
		}
	    }
	    if (tagPtr->justifyString != NULL) {
		if (Tk_GetJustify(interp, tagPtr->justifyString,
			&tagPtr->justify) != TCL_OK) {
		    return TCL_ERROR;
		}
	    }
	    if (tagPtr->lMargin1String != NULL) {
		if (Tk_GetPixels(interp, textPtr->tkwin,
			tagPtr->lMargin1String, &tagPtr->lMargin1) != TCL_OK) {
		    return TCL_ERROR;
		}
	    }
	    if (tagPtr->lMargin2String != NULL) {
		if (Tk_GetPixels(interp, textPtr->tkwin,
			tagPtr->lMargin2String, &tagPtr->lMargin2) != TCL_OK) {
		    return TCL_ERROR;
		}
	    }
	    if (tagPtr->offsetString != NULL) {
		if (Tk_GetPixels(interp, textPtr->tkwin, tagPtr->offsetString,
			&tagPtr->offset) != TCL_OK) {
		    return TCL_ERROR;
		}
	    }
	    if (tagPtr->overstrikeString != NULL) {
		if (Tcl_GetBoolean(interp, tagPtr->overstrikeString,
			&tagPtr->overstrike) != TCL_OK) {
		    return TCL_ERROR;
		}
	    }
	    if (tagPtr->rMarginString != NULL) {
		if (Tk_GetPixels(interp, textPtr->tkwin,
			tagPtr->rMarginString, &tagPtr->rMargin) != TCL_OK) {
		    return TCL_ERROR;
		}
	    }
	    if (tagPtr->spacing1String != NULL) {
		if (Tk_GetPixels(interp, textPtr->tkwin,
			tagPtr->spacing1String, &tagPtr->spacing1) != TCL_OK) {
		    return TCL_ERROR;
		}
		if (tagPtr->spacing1 < 0) {
		    tagPtr->spacing1 = 0;
		}
	    }
	    if (tagPtr->spacing2String != NULL) {
		if (Tk_GetPixels(interp, textPtr->tkwin,
			tagPtr->spacing2String, &tagPtr->spacing2) != TCL_OK) {
		    return TCL_ERROR;
		}
		if (tagPtr->spacing2 < 0) {
		    tagPtr->spacing2 = 0;
		}
	    }
	    if (tagPtr->spacing3String != NULL) {
		if (Tk_GetPixels(interp, textPtr->tkwin,
			tagPtr->spacing3String, &tagPtr->spacing3) != TCL_OK) {
		    return TCL_ERROR;
		}
		if (tagPtr->spacing3 < 0) {
		    tagPtr->spacing3 = 0;
		}
	    }
	    if (tagPtr->tabArrayPtr != NULL) {
		ckfree((char *) tagPtr->tabArrayPtr);
		tagPtr->tabArrayPtr = NULL;
	    }
	    if (tagPtr->tabString != NULL) {
		tagPtr->tabArrayPtr = TkTextGetTabs(interp, textPtr->tkwin,
			tagPtr->tabString);
		if (tagPtr->tabArrayPtr == NULL) {
		    return TCL_ERROR;
		}
	    }
	    if (tagPtr->underlineString != NULL) {
		if (Tcl_GetBoolean(interp, tagPtr->underlineString,
			&tagPtr->underline) != TCL_OK) {
		    return TCL_ERROR;
		}
	    }
	    if ((tagPtr->wrapMode != NULL)
		    && (tagPtr->wrapMode != tkTextCharUid)
		    && (tagPtr->wrapMode != tkTextNoneUid)
		    && (tagPtr->wrapMode != tkTextWordUid)) {
		Tcl_AppendResult(interp, "bad wrap mode \"", tagPtr->wrapMode,
			"\": must be char, none, or word", (char *) NULL);
		tagPtr->wrapMode = NULL;
		return TCL_ERROR;
	    }

	    /*
	     * If the "sel" tag was changed, be sure to mirror information
	     * from the tag back into the text widget record.   NOTE: we
	     * don't have to free up information in the widget record
	     * before overwriting it, because it was mirrored in the tag
	     * and hence freed when the tag field was overwritten.
	     */

	    if (tagPtr == textPtr->selTagPtr) {
		textPtr->selBorder = tagPtr->border;
		textPtr->selBdString = tagPtr->bdString;
		textPtr->selFgColorPtr = tagPtr->fgColor;
	    }
	    tagPtr->affectsDisplay = 0;
	    if ((tagPtr->border != NULL)
		    || (tagPtr->bdString != NULL)
		    || (tagPtr->reliefString != NULL)
		    || (tagPtr->bgStipple != None)
		    || (tagPtr->fgColor != NULL) || (tagPtr->fontPtr != None)
		    || (tagPtr->fgStipple != None)
		    || (tagPtr->justifyString != NULL)
		    || (tagPtr->lMargin1String != NULL)
		    || (tagPtr->lMargin2String != NULL)
		    || (tagPtr->offsetString != NULL)
		    || (tagPtr->overstrikeString != NULL)
		    || (tagPtr->rMarginString != NULL)
		    || (tagPtr->spacing1String != NULL)
		    || (tagPtr->spacing2String != NULL)
		    || (tagPtr->spacing3String != NULL)
		    || (tagPtr->tabString != NULL)
		    || (tagPtr->underlineString != NULL)
		    || (tagPtr->wrapMode != NULL)) {
		tagPtr->affectsDisplay = 1;
	    }
	    TkTextRedrawTag(textPtr, (TkTextIndex *) NULL,
		    (TkTextIndex *) NULL, tagPtr, 1);
	    return result;
	}
    } else if ((c == 'd') && (strncmp(argv[2], "delete", length) == 0)) {
	Tcl_HashEntry *hPtr;

	if (argc < 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " tag delete tagName tagName ...\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	for (i = 3; i < argc; i++) {
	    hPtr = Tcl_FindHashEntry(&textPtr->tagTable, argv[i]);
	    if (hPtr == NULL) {
		continue;
	    }
	    tagPtr = (TkTextTag *) Tcl_GetHashValue(hPtr);
	    if (tagPtr == textPtr->selTagPtr) {
		continue;
	    }
	    if (tagPtr->affectsDisplay) {
		TkTextRedrawTag(textPtr, (TkTextIndex *) NULL,
			(TkTextIndex *) NULL, tagPtr, 1);
	    }
	    TkBTreeTag(TkTextMakeIndex(textPtr->tree, 0, 0, &first),
		    TkTextMakeIndex(textPtr->tree,
			    TkBTreeNumLines(textPtr->tree), 0, &last),
		    tagPtr, 0);
	    Tcl_DeleteHashEntry(hPtr);
	    if (textPtr->bindingTable != NULL) {
		Tk_DeleteAllBindings(textPtr->bindingTable,
			(ClientData) tagPtr);
	    }
	
	    /*
	     * Update the tag priorities to reflect the deletion of this tag.
	     */

	    ChangeTagPriority(textPtr, tagPtr, textPtr->numTags-1);
	    textPtr->numTags -= 1;
	    TkTextFreeTag(textPtr, tagPtr);
	}
    } else if ((c == 'l') && (strncmp(argv[2], "lower", length) == 0)) {
	TkTextTag *tagPtr2;
	int prio;

	if ((argc != 4) && (argc != 5)) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " tag lower tagName ?belowThis?\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	tagPtr = FindTag(interp, textPtr, argv[3]);
	if (tagPtr == NULL) {
	    return TCL_ERROR;
	}
	if (argc == 5) {
	    tagPtr2 = FindTag(interp, textPtr, argv[4]);
	    if (tagPtr2 == NULL) {
		return TCL_ERROR;
	    }
	    if (tagPtr->priority < tagPtr2->priority) {
		prio = tagPtr2->priority - 1;
	    } else {
		prio = tagPtr2->priority;
	    }
	} else {
	    prio = 0;
	}
	ChangeTagPriority(textPtr, tagPtr, prio);
	TkTextRedrawTag(textPtr, (TkTextIndex *) NULL, (TkTextIndex *) NULL,
		tagPtr, 1);
    } else if ((c == 'n') && (strncmp(argv[2], "names", length) == 0)
	    && (length >= 2)) {
	TkTextTag **arrayPtr;
	int arraySize;

	if ((argc != 3) && (argc != 4)) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " tag names ?index?\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 3) {
	    Tcl_HashSearch search;
	    Tcl_HashEntry *hPtr;

	    arrayPtr = (TkTextTag **) ckalloc((unsigned)
		    (textPtr->numTags * sizeof(TkTextTag *)));
	    for (i = 0, hPtr = Tcl_FirstHashEntry(&textPtr->tagTable, &search);
		    hPtr != NULL; i++, hPtr = Tcl_NextHashEntry(&search)) {
		arrayPtr[i] = (TkTextTag *) Tcl_GetHashValue(hPtr);
	    }
	    arraySize = textPtr->numTags;
	} else {
	    if (TkTextGetIndex(interp, textPtr, argv[3], &index1)
		    != TCL_OK) {
		return TCL_ERROR;
	    }
	    arrayPtr = TkBTreeGetTags(&index1, &arraySize);
	    if (arrayPtr == NULL) {
		return TCL_OK;
	    }
	}
	SortTags(arraySize, arrayPtr);
	for (i = 0; i < arraySize; i++) {
	    tagPtr = arrayPtr[i];
	    Tcl_AppendElement(interp, tagPtr->name);
	}
	ckfree((char *) arrayPtr);
    } else if ((c == 'n') && (strncmp(argv[2], "nextrange", length) == 0)
	    && (length >= 2)) {
	TkTextSearch tSearch;
	char position[TK_POS_CHARS];

	if ((argc != 5) && (argc != 6)) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " tag nextrange tagName index1 ?index2?\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	tagPtr = FindTag((Tcl_Interp *) NULL, textPtr, argv[3]);
	if (tagPtr == NULL) {
	    return TCL_OK;
	}
	if (TkTextGetIndex(interp, textPtr, argv[4], &index1) != TCL_OK) {
	    return TCL_ERROR;
	}
	TkTextMakeIndex(textPtr->tree, TkBTreeNumLines(textPtr->tree),
		0, &last);
	if (argc == 5) {
	    index2 = last;
	} else if (TkTextGetIndex(interp, textPtr, argv[5], &index2)
		!= TCL_OK) {
	    return TCL_ERROR;
	}

	/*
	 * The search below is a bit tricky.  Rather than use the B-tree
	 * facilities to stop the search at index2, let it search up
	 * until the end of the file but check for a position past index2
	 * ourselves.  The reason for doing it this way is that we only
	 * care whether the *start* of the range is before index2;  once
	 * we find the start, we don't want TkBTreeNextTag to abort the
	 * search because the end of the range is after index2.
	 */

	TkBTreeStartSearch(&index1, &last, tagPtr, &tSearch);
	if (TkBTreeCharTagged(&index1, tagPtr)) {
	    TkTextSegment *segPtr;
	    int offset;

	    /*
	     * The first character is tagged.  See if there is an
	     * on-toggle just before the character.  If not, then
	     * skip to the end of this tagged range.
	     */

	    for (segPtr = index1.linePtr->segPtr, offset = index1.charIndex; 
		    offset >= 0;
		    offset -= segPtr->size, segPtr = segPtr->nextPtr) {
		if ((offset == 0) && (segPtr->typePtr == &tkTextToggleOnType)
			&& (segPtr->body.toggle.tagPtr == tagPtr)) {
		    goto gotStart;
		}
	    }
	    if (!TkBTreeNextTag(&tSearch)) {
		 return TCL_OK;
	    }
	}

	/*
	 * Find the start of the tagged range.
	 */

	if (!TkBTreeNextTag(&tSearch)) {
	    return TCL_OK;
	}
	gotStart:
	if (TkTextIndexCmp(&tSearch.curIndex, &index2) >= 0) {
	    return TCL_OK;
	}
	TkTextPrintIndex(&tSearch.curIndex, position);
	Tcl_AppendElement(interp, position);
	TkBTreeNextTag(&tSearch);
	TkTextPrintIndex(&tSearch.curIndex, position);
	Tcl_AppendElement(interp, position);
    } else if ((c == 'p') && (strncmp(argv[2], "prevrange", length) == 0)
	    && (length >= 2)) {
	TkTextSearch tSearch;
	char position1[TK_POS_CHARS];
	char position2[TK_POS_CHARS];

	if ((argc != 5) && (argc != 6)) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " tag prevrange tagName index1 ?index2?\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	tagPtr = FindTag((Tcl_Interp *) NULL, textPtr, argv[3]);
	if (tagPtr == NULL) {
	    return TCL_OK;
	}
	if (TkTextGetIndex(interp, textPtr, argv[4], &index1) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (argc == 5) {
	    TkTextMakeIndex(textPtr->tree, 0, 0, &index2);
	} else if (TkTextGetIndex(interp, textPtr, argv[5], &index2)
		!= TCL_OK) {
	    return TCL_ERROR;
	}

	/*
	 * The search below is a bit weird.  The previous toggle can be
	 * either an on or off toggle. If it is an on toggle, then we
	 * need to turn around and search forward for the end toggle.
	 * Otherwise we keep searching backwards.
	 */

	TkBTreeStartSearchBack(&index1, &index2, tagPtr, &tSearch);

	if (!TkBTreePrevTag(&tSearch)) {
	    return TCL_OK;
	}
	if (tSearch.segPtr->typePtr == &tkTextToggleOnType) {
	    TkTextPrintIndex(&tSearch.curIndex, position1);
	    TkTextMakeIndex(textPtr->tree, TkBTreeNumLines(textPtr->tree),
		    0, &last);
	    TkBTreeStartSearch(&tSearch.curIndex, &last, tagPtr, &tSearch);
	    TkBTreeNextTag(&tSearch);
	    TkTextPrintIndex(&tSearch.curIndex, position2);
	} else {
	    TkTextPrintIndex(&tSearch.curIndex, position2);
	    TkBTreePrevTag(&tSearch);
	    if (TkTextIndexCmp(&tSearch.curIndex, &index2) < 0) {
		return TCL_OK;
	    }
	    TkTextPrintIndex(&tSearch.curIndex, position1);
	}
	Tcl_AppendElement(interp, position1);
	Tcl_AppendElement(interp, position2);
    } else if ((c == 'r') && (strncmp(argv[2], "raise", length) == 0)
	    && (length >= 3)) {
	TkTextTag *tagPtr2;
	int prio;

	if ((argc != 4) && (argc != 5)) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " tag raise tagName ?aboveThis?\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	tagPtr = FindTag(interp, textPtr, argv[3]);
	if (tagPtr == NULL) {
	    return TCL_ERROR;
	}
	if (argc == 5) {
	    tagPtr2 = FindTag(interp, textPtr, argv[4]);
	    if (tagPtr2 == NULL) {
		return TCL_ERROR;
	    }
	    if (tagPtr->priority <= tagPtr2->priority) {
		prio = tagPtr2->priority;
	    } else {
		prio = tagPtr2->priority + 1;
	    }
	} else {
	    prio = textPtr->numTags-1;
	}
	ChangeTagPriority(textPtr, tagPtr, prio);
	TkTextRedrawTag(textPtr, (TkTextIndex *) NULL, (TkTextIndex *) NULL,
		tagPtr, 1);
    } else if ((c == 'r') && (strncmp(argv[2], "ranges", length) == 0)
	    && (length >= 3)) {
	TkTextSearch tSearch;
	char position[TK_POS_CHARS];

	if (argc != 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " tag ranges tagName\"", (char *) NULL);
	    return TCL_ERROR;
	}
	tagPtr = FindTag((Tcl_Interp *) NULL, textPtr, argv[3]);
	if (tagPtr == NULL) {
	    return TCL_OK;
	}
	TkTextMakeIndex(textPtr->tree, 0, 0, &first);
	TkTextMakeIndex(textPtr->tree, TkBTreeNumLines(textPtr->tree),
		0, &last);
	TkBTreeStartSearch(&first, &last, tagPtr, &tSearch);
	if (TkBTreeCharTagged(&first, tagPtr)) {
	    TkTextPrintIndex(&first, position);
	    Tcl_AppendElement(interp, position);
	}
	while (TkBTreeNextTag(&tSearch)) {
	    TkTextPrintIndex(&tSearch.curIndex, position);
	    Tcl_AppendElement(interp, position);
	}
    } else if ((c == 'r') && (strncmp(argv[2], "remove", length) == 0)
	    && (length >= 2)) {
	fullOption = "remove";
	addTag = 0;
	goto addAndRemove;
    } else {
	Tcl_AppendResult(interp, "bad tag option \"", argv[2],
		"\": must be add, bind, cget, configure, delete, lower, ",
		"names, nextrange, raise, ranges, or remove",
		(char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TkTextCreateTag --
 *
 *	Find the record describing a tag within a given text widget,
 *	creating a new record if one doesn't already exist.
 *
 * Results:
 *	The return value is a pointer to the TkTextTag record for tagName.
 *
 * Side effects:
 *	A new tag record is created if there isn't one already defined
 *	for tagName.
 *
 *----------------------------------------------------------------------
 */

TkTextTag *
TkTextCreateTag(textPtr, tagName)
    TkText *textPtr;		/* Widget in which tag is being used. */
    char *tagName;		/* Name of desired tag. */
{
    register TkTextTag *tagPtr;
    Tcl_HashEntry *hPtr;
    int new;

    hPtr = Tcl_CreateHashEntry(&textPtr->tagTable, tagName, &new);
    if (!new) {
	return (TkTextTag *) Tcl_GetHashValue(hPtr);
    }

    /*
     * No existing entry.  Create a new one, initialize it, and add a
     * pointer to it to the hash table entry.
     */

    tagPtr = (TkTextTag *) ckalloc(sizeof(TkTextTag));
    tagPtr->name = Tcl_GetHashKey(&textPtr->tagTable, hPtr);
    tagPtr->toggleCount = 0;
    tagPtr->tagRootPtr = NULL;
    tagPtr->priority = textPtr->numTags;
    tagPtr->border = NULL;
    tagPtr->bdString = NULL;
    tagPtr->borderWidth = 0;
    tagPtr->reliefString = NULL;
    tagPtr->relief = TK_RELIEF_FLAT;
    tagPtr->bgStipple = None;
    tagPtr->fgColor = NULL;
    tagPtr->fontPtr = NULL;
    tagPtr->fgStipple = None;
    tagPtr->justifyString = NULL;
    tagPtr->justify = TK_JUSTIFY_LEFT;
    tagPtr->lMargin1String = NULL;
    tagPtr->lMargin1 = 0;
    tagPtr->lMargin2String = NULL;
    tagPtr->lMargin2 = 0;
    tagPtr->offsetString = NULL;
    tagPtr->offset = 0;
    tagPtr->overstrikeString = NULL;
    tagPtr->overstrike = 0;
    tagPtr->rMarginString = NULL;
    tagPtr->rMargin = 0;
    tagPtr->spacing1String = NULL;
    tagPtr->spacing1 = 0;
    tagPtr->spacing2String = NULL;
    tagPtr->spacing2 = 0;
    tagPtr->spacing3String = NULL;
    tagPtr->spacing3 = 0;
    tagPtr->tabString = NULL;
    tagPtr->tabArrayPtr = NULL;
    tagPtr->underlineString = NULL;
    tagPtr->underline = 0;
    tagPtr->wrapMode = NULL;
    tagPtr->affectsDisplay = 0;
    textPtr->numTags++;
    Tcl_SetHashValue(hPtr, tagPtr);
    return tagPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * FindTag --
 *
 *	See if tag is defined for a given widget.
 *
 * Results:
 *	If tagName is defined in textPtr, a pointer to its TkTextTag
 *	structure is returned.  Otherwise NULL is returned and an
 *	error message is recorded in interp->result unless interp
 *	is NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static TkTextTag *
FindTag(interp, textPtr, tagName)
    Tcl_Interp *interp;		/* Interpreter to use for error message;
				 * if NULL, then don't record an error
				 * message. */
    TkText *textPtr;		/* Widget in which tag is being used. */
    char *tagName;		/* Name of desired tag. */
{
    Tcl_HashEntry *hPtr;

    hPtr = Tcl_FindHashEntry(&textPtr->tagTable, tagName);
    if (hPtr != NULL) {
	return (TkTextTag *) Tcl_GetHashValue(hPtr);
    }
    if (interp != NULL) {
	Tcl_AppendResult(interp, "tag \"", tagName,
		"\" isn't defined in text widget", (char *) NULL);
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TkTextFreeTag --
 *
 *	This procedure is called when a tag is deleted to free up the
 *	memory and other resources associated with the tag.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory and other resources are freed.
 *
 *----------------------------------------------------------------------
 */

void
TkTextFreeTag(textPtr, tagPtr)
    TkText *textPtr;			/* Info about overall widget. */
    register TkTextTag *tagPtr;		/* Tag being deleted. */
{
    if (tagPtr->border != None) {
	Tk_Free3DBorder(tagPtr->border);
    }
    if (tagPtr->bdString != NULL) {
	ckfree(tagPtr->bdString);
    }
    if (tagPtr->reliefString != NULL) {
	ckfree(tagPtr->reliefString);
    }
    if (tagPtr->bgStipple != None) {
	Tk_FreeBitmap(textPtr->display, tagPtr->bgStipple);
    }
    if (tagPtr->fgColor != None) {
	Tk_FreeColor(tagPtr->fgColor);
    }
    if (tagPtr->fontPtr != None) {
	Tk_FreeFontStruct(tagPtr->fontPtr);
    }
    if (tagPtr->fgStipple != None) {
	Tk_FreeBitmap(textPtr->display, tagPtr->fgStipple);
    }
    if (tagPtr->justifyString != NULL) {
	ckfree(tagPtr->justifyString);
    }
    if (tagPtr->lMargin1String != NULL) {
	ckfree(tagPtr->lMargin1String);
    }
    if (tagPtr->lMargin2String != NULL) {
	ckfree(tagPtr->lMargin2String);
    }
    if (tagPtr->offsetString != NULL) {
	ckfree(tagPtr->offsetString);
    }
    if (tagPtr->overstrikeString != NULL) {
	ckfree(tagPtr->overstrikeString);
    }
    if (tagPtr->rMarginString != NULL) {
	ckfree(tagPtr->rMarginString);
    }
    if (tagPtr->spacing1String != NULL) {
	ckfree(tagPtr->spacing1String);
    }
    if (tagPtr->spacing2String != NULL) {
	ckfree(tagPtr->spacing2String);
    }
    if (tagPtr->spacing3String != NULL) {
	ckfree(tagPtr->spacing3String);
    }
    if (tagPtr->tabString != NULL) {
	ckfree(tagPtr->tabString);
    }
    if (tagPtr->tabArrayPtr != NULL) {
	ckfree((char *) tagPtr->tabArrayPtr);
    }
    if (tagPtr->underlineString != NULL) {
	ckfree(tagPtr->underlineString);
    }
    ckfree((char *) tagPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * SortTags --
 *
 *	This procedure sorts an array of tag pointers in increasing
 *	order of priority, optimizing for the common case where the
 *	array is small.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
SortTags(numTags, tagArrayPtr)
    int numTags;		/* Number of tag pointers at *tagArrayPtr. */
    TkTextTag **tagArrayPtr;	/* Pointer to array of pointers. */
{
    int i, j, prio;
    register TkTextTag **tagPtrPtr;
    TkTextTag **maxPtrPtr, *tmp;

    if (numTags < 2) {
	return;
    }
    if (numTags < 20) {
	for (i = numTags-1; i > 0; i--, tagArrayPtr++) {
	    maxPtrPtr = tagPtrPtr = tagArrayPtr;
	    prio = tagPtrPtr[0]->priority;
	    for (j = i, tagPtrPtr++; j > 0; j--, tagPtrPtr++) {
		if (tagPtrPtr[0]->priority < prio) {
		    prio = tagPtrPtr[0]->priority;
		    maxPtrPtr = tagPtrPtr;
		}
	    }
	    tmp = *maxPtrPtr;
	    *maxPtrPtr = *tagArrayPtr;
	    *tagArrayPtr = tmp;
	}
    } else {
	qsort((VOID *) tagArrayPtr, (unsigned) numTags, sizeof (TkTextTag *),
		    TagSortProc);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TagSortProc --
 *
 *	This procedure is called by qsort when sorting an array of
 *	tags in priority order.
 *
 * Results:
 *	The return value is -1 if the first argument should be before
 *	the second element (i.e. it has lower priority), 0 if it's
 *	equivalent (this should never happen!), and 1 if it should be
 *	after the second element.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TagSortProc(first, second)
    CONST VOID *first, *second;		/* Elements to be compared. */
{
    TkTextTag *tagPtr1, *tagPtr2;

    tagPtr1 = * (TkTextTag **) first;
    tagPtr2 = * (TkTextTag **) second;
    return tagPtr1->priority - tagPtr2->priority;
}

/*
 *----------------------------------------------------------------------
 *
 * ChangeTagPriority --
 *
 *	This procedure changes the priority of a tag by modifying
 *	its priority and the priorities of other tags that are affected
 *	by the change.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Priorities may be changed for some or all of the tags in
 *	textPtr.  The tags will be arranged so that there is exactly
 *	one tag at each priority level between 0 and textPtr->numTags-1,
 *	with tagPtr at priority "prio".
 *
 *----------------------------------------------------------------------
 */

static void
ChangeTagPriority(textPtr, tagPtr, prio)
    TkText *textPtr;			/* Information about text widget. */
    TkTextTag *tagPtr;			/* Tag whose priority is to be
					 * changed. */
    int prio;				/* New priority for tag. */
{
    int low, high, delta;
    register TkTextTag *tagPtr2;
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;

    if (prio < 0) {
	prio = 0;
    }
    if (prio >= textPtr->numTags) {
	prio = textPtr->numTags-1;
    }
    if (prio == tagPtr->priority) {
	return;
    } else if (prio < tagPtr->priority) {
	low = prio;
	high = tagPtr->priority-1;
	delta = 1;
    } else {
	low = tagPtr->priority+1;
	high = prio;
	delta = -1;
    }
    for (hPtr = Tcl_FirstHashEntry(&textPtr->tagTable, &search);
	    hPtr != NULL; hPtr = Tcl_NextHashEntry(&search)) {
	tagPtr2 = (TkTextTag *) Tcl_GetHashValue(hPtr);
	if ((tagPtr2->priority >= low) && (tagPtr2->priority <= high)) {
	    tagPtr2->priority += delta;
	}
    }
    tagPtr->priority = prio;
}

/*
 *--------------------------------------------------------------
 *
 * TkTextBindProc --
 *
 *	This procedure is invoked by the Tk dispatcher to handle
 *	events associated with bindings on items.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Depends on the command invoked as part of the binding
 *	(if there was any).
 *
 *--------------------------------------------------------------
 */

void
TkTextBindProc(clientData, eventPtr)
    ClientData clientData;		/* Pointer to canvas structure. */
    XEvent *eventPtr;			/* Pointer to X event that just
					 * happened. */
{
    TkText *textPtr = (TkText *) clientData;
    int repick  = 0;

# define AnyButtonMask (Button1Mask|Button2Mask|Button3Mask\
	|Button4Mask|Button5Mask)

    Tcl_Preserve((ClientData) textPtr);

    /*
     * This code simulates grabs for mouse buttons by keeping track
     * of whether a button is pressed and refusing to pick a new current
     * character while a button is pressed.
     */

    if (eventPtr->type == ButtonPress) {
	textPtr->flags |= BUTTON_DOWN;
    } else if (eventPtr->type == ButtonRelease) {
	int mask;

	switch (eventPtr->xbutton.button) {
	    case Button1:
		mask = Button1Mask;
		break;
	    case Button2:
		mask = Button2Mask;
		break;
	    case Button3:
		mask = Button3Mask;
		break;
	    case Button4:
		mask = Button4Mask;
		break;
	    case Button5:
		mask = Button5Mask;
		break;
	    default:
		mask = 0;
		break;
	}
	if ((eventPtr->xbutton.state & AnyButtonMask) == mask) {
	    textPtr->flags &= ~BUTTON_DOWN;
	    repick = 1;
	}
    } else if ((eventPtr->type == EnterNotify)
	    || (eventPtr->type == LeaveNotify)) {
	if (eventPtr->xcrossing.state & AnyButtonMask)  {
	    textPtr->flags |= BUTTON_DOWN;
	} else {
	    textPtr->flags &= ~BUTTON_DOWN;
	}
	TkTextPickCurrent(textPtr, eventPtr);
	goto done;
    } else if (eventPtr->type == MotionNotify) {
	if (eventPtr->xmotion.state & AnyButtonMask)  {
	    textPtr->flags |= BUTTON_DOWN;
	} else {
	    textPtr->flags &= ~BUTTON_DOWN;
	}
	TkTextPickCurrent(textPtr, eventPtr);
    }
    if ((textPtr->numCurTags > 0) && (textPtr->bindingTable != NULL)
	    && (textPtr->tkwin != NULL)) {
	Tk_BindEvent(textPtr->bindingTable, eventPtr, textPtr->tkwin,
		textPtr->numCurTags, (ClientData *) textPtr->curTagArrayPtr);
    }
    if (repick) {
	unsigned int oldState;

	oldState = eventPtr->xbutton.state;
	eventPtr->xbutton.state &= ~(Button1Mask|Button2Mask
		|Button3Mask|Button4Mask|Button5Mask);
	TkTextPickCurrent(textPtr, eventPtr);
	eventPtr->xbutton.state = oldState;
    }

    done:
    Tcl_Release((ClientData) textPtr);
}

/*
 *--------------------------------------------------------------
 *
 * TkTextPickCurrent --
 *
 *	Find the character containing the coordinates in an event
 *	and place the "current" mark on that character.  If the
 *	"current" mark has moved then generate a fake leave event
 *	on the old current character and a fake enter event on the new
 *	current character.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The current mark for textPtr may change.  If it does,
 *	then the commands associated with character entry and leave
 *	could do just about anything.  For example, the text widget
 *	might be deleted.  It is up to the caller to protect itself
 *	with calls to Tcl_Preserve and Tcl_Release.
 *
 *--------------------------------------------------------------
 */

void
TkTextPickCurrent(textPtr, eventPtr)
    register TkText *textPtr;		/* Text widget in which to select
					 * current character. */
    XEvent *eventPtr;			/* Event describing location of
					 * mouse cursor.  Must be EnterWindow,
					 * LeaveWindow, ButtonRelease, or
					 * MotionNotify. */
{
    TkTextIndex index;
    TkTextTag **oldArrayPtr, **newArrayPtr;
    TkTextTag **copyArrayPtr = NULL;	/* Initialization needed to prevent
					 * compiler warning. */

    int numOldTags, numNewTags, i, j, size;
    XEvent event;

    /*
     * If a button is down, then don't do anything at all;  we'll be
     * called again when all buttons are up, and we can repick then.
     * This implements a form of mouse grabbing.
     */

    if (textPtr->flags & BUTTON_DOWN) {
	if (((eventPtr->type == EnterNotify) || (eventPtr->type == LeaveNotify))
		&& ((eventPtr->xcrossing.mode == NotifyGrab)
		|| (eventPtr->xcrossing.mode == NotifyUngrab))) {
	    /*
	     * Special case:  the window is being entered or left because
	     * of a grab or ungrab.  In this case, repick after all.
	     * Furthermore, clear BUTTON_DOWN to release the simulated
	     * grab.
	     */

	    textPtr->flags &= ~BUTTON_DOWN;
	} else {
	    return;
	}
    }

    /*
     * Save information about this event in the widget in case we have
     * to synthesize more enter and leave events later (e.g. because a
     * character was deleted, causing a new character to be underneath
     * the mouse cursor).  Also translate MotionNotify events into
     * EnterNotify events, since that's what gets reported to event
     * handlers when the current character changes.
     */

    if (eventPtr != &textPtr->pickEvent) {
	if ((eventPtr->type == MotionNotify)
		|| (eventPtr->type == ButtonRelease)) {
	    textPtr->pickEvent.xcrossing.type = EnterNotify;
	    textPtr->pickEvent.xcrossing.serial = eventPtr->xmotion.serial;
	    textPtr->pickEvent.xcrossing.send_event
		    = eventPtr->xmotion.send_event;
	    textPtr->pickEvent.xcrossing.display = eventPtr->xmotion.display;
	    textPtr->pickEvent.xcrossing.window = eventPtr->xmotion.window;
	    textPtr->pickEvent.xcrossing.root = eventPtr->xmotion.root;
	    textPtr->pickEvent.xcrossing.subwindow = None;
	    textPtr->pickEvent.xcrossing.time = eventPtr->xmotion.time;
	    textPtr->pickEvent.xcrossing.x = eventPtr->xmotion.x;
	    textPtr->pickEvent.xcrossing.y = eventPtr->xmotion.y;
	    textPtr->pickEvent.xcrossing.x_root = eventPtr->xmotion.x_root;
	    textPtr->pickEvent.xcrossing.y_root = eventPtr->xmotion.y_root;
	    textPtr->pickEvent.xcrossing.mode = NotifyNormal;
	    textPtr->pickEvent.xcrossing.detail = NotifyNonlinear;
	    textPtr->pickEvent.xcrossing.same_screen
		    = eventPtr->xmotion.same_screen;
	    textPtr->pickEvent.xcrossing.focus = False;
	    textPtr->pickEvent.xcrossing.state = eventPtr->xmotion.state;
	} else  {
	    textPtr->pickEvent = *eventPtr;
	}
    }

    /*
     * Find the new current character, then find and sort all of the
     * tags associated with it.
     */

    if (textPtr->pickEvent.type != LeaveNotify) {
	TkTextPixelIndex(textPtr, textPtr->pickEvent.xcrossing.x,
		textPtr->pickEvent.xcrossing.y, &index);
	newArrayPtr = TkBTreeGetTags(&index, &numNewTags);
	SortTags(numNewTags, newArrayPtr);
    } else {
	newArrayPtr = NULL;
	numNewTags = 0;
    }

    /*
     * Resort the tags associated with the previous marked character
     * (the priorities might have changed), then make a copy of the
     * new tags, and compare the old tags to the copy, nullifying
     * any tags that are present in both groups (i.e. the tags that
     * haven't changed).
     */

    SortTags(textPtr->numCurTags, textPtr->curTagArrayPtr);
    if (numNewTags > 0) {
	size = numNewTags * sizeof(TkTextTag *);
	copyArrayPtr = (TkTextTag **) ckalloc((unsigned) size);
	memcpy((VOID *) copyArrayPtr, (VOID *) newArrayPtr, (size_t) size);
	for (i = 0; i < textPtr->numCurTags; i++) {
	    for (j = 0; j < numNewTags; j++) {
		if (textPtr->curTagArrayPtr[i] == copyArrayPtr[j]) {
		    textPtr->curTagArrayPtr[i] = NULL;
		    copyArrayPtr[j] = NULL;
		    break;
		}
	    }
	}
    }

    /*
     * Invoke the binding system with a LeaveNotify event for all of
     * the tags that have gone away.  We have to be careful here,
     * because it's possible that the binding could do something
     * (like calling tkwait) that eventually modifies
     * textPtr->curTagArrayPtr.  To avoid problems in situations like
     * this, update curTagArrayPtr to its new value before invoking
     * any bindings, and don't use it any more here.
     */

    numOldTags = textPtr->numCurTags;
    textPtr->numCurTags = numNewTags;
    oldArrayPtr = textPtr->curTagArrayPtr;
    textPtr->curTagArrayPtr = newArrayPtr;
    if (numOldTags != 0) {
	if ((textPtr->bindingTable != NULL) && (textPtr->tkwin != NULL)) {
	    event = textPtr->pickEvent;
	    event.type = LeaveNotify;

	    /*
	     * Always use a detail of NotifyAncestor.  Besides being
	     * consistent, this avoids problems where the binding code
	     * will discard NotifyInferior events.
	     */

	    event.xcrossing.detail = NotifyAncestor;
	    Tk_BindEvent(textPtr->bindingTable, &event, textPtr->tkwin,
		    numOldTags, (ClientData *) oldArrayPtr);
	}
	ckfree((char *) oldArrayPtr);
    }

    /*
     * Reset the "current" mark (be careful to recompute its location,
     * since it might have changed during an event binding).  Then
     * invoke the binding system with an EnterNotify event for all of
     * the tags that have just appeared.
     */

    TkTextPixelIndex(textPtr, textPtr->pickEvent.xcrossing.x,
	    textPtr->pickEvent.xcrossing.y, &index);
    TkTextSetMark(textPtr, "current", &index);
    if (numNewTags != 0) {
	if ((textPtr->bindingTable != NULL) && (textPtr->tkwin != NULL)) {
	    event = textPtr->pickEvent;
	    event.type = EnterNotify;
	    event.xcrossing.detail = NotifyAncestor;
	    Tk_BindEvent(textPtr->bindingTable, &event, textPtr->tkwin,
		    numNewTags, (ClientData *) copyArrayPtr);
	}
	ckfree((char *) copyArrayPtr);
    }
}
