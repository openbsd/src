/* 
 * tkWinFont.c --
 *
 *	This file contains the Xlib emulation routines relating to
 *	creating and manipulating fonts.
 *
 * Copyright (c) 1995 Sun Microsystems, Inc.
 * Copyright (c) 1994 Software Research Associates, Inc. 
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkWinFont.c 1.8 96/04/05 15:21:22
 */

#include "tkWinInt.h"

/*
 * Forward declarations for functions used in this file.
 */

static int		NameToFont _ANSI_ARGS_((_Xconst char *name,
			    LOGFONT *logfont));
static int		XNameToFont _ANSI_ARGS_((_Xconst char *name,
			    LOGFONT *logfont));

/*
 *----------------------------------------------------------------------
 *
 * NameToFont --
 *
 *	Converts a three part font name into a logical font
 *	description.  Font name is of the form:
 *		"Family point_size style_list"
 *	Style_list contains a list of one or more attributes:
 *		normal, bold, italic, underline, strikeout
 *
 * Results:
 *	Returns false if the font name was syntactically invalid,
 *	else true.  Sets the fields of the passed in LOGFONT.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
NameToFont(name, logfont)
    _Xconst char *name;
    LOGFONT *logfont;
{
    int argc, argc2;
    char **argv, **argv2;
    int nameLen, i, pointSize = 0;
    Tcl_Interp *dummy = Tcl_CreateInterp();

    if (Tcl_SplitList(dummy, (char *) name, &argc, &argv) != TCL_OK) {
	goto nomatch;
    }
    if (argc != 3) {
	ckfree((char *) argv);
	goto nomatch;
    }

    memset(logfont, '\0', sizeof(LOGFONT));

    /*
     * Determine the font family name.
     */

    nameLen = strlen(argv[0]);
    if (nameLen > LF_FACESIZE) {
	nameLen = LF_FACESIZE;
    }
    strncpy(logfont->lfFaceName, argv[0], nameLen);

    /*
     * Check the character set.
     */

    logfont->lfCharSet = ANSI_CHARSET;
    if (stricmp(logfont->lfFaceName, "Symbol") == 0) {
	logfont->lfCharSet = SYMBOL_CHARSET;
    } else if (stricmp(logfont->lfFaceName, "WingDings") == 0) {
	logfont->lfCharSet = SYMBOL_CHARSET;
    }
	
    /*
     * Determine the font size.
     */

    if (Tcl_GetInt(dummy, argv[1], &pointSize) != TCL_OK) {
	ckfree((char *) argv);
	goto nomatch;
    }
    logfont->lfHeight = -pointSize;

    /*
     * Apply any style modifiers.
     */
	
    if (Tcl_SplitList(dummy, (char *) argv[2], &argc2, &argv2) != TCL_OK) {
	ckfree((char*) argv);
	goto nomatch;
    }
    for (i = 0; i < argc2; i++) {
	if (stricmp(argv2[i], "normal") == 0) {
	    logfont->lfWeight = FW_NORMAL;
	} else if (stricmp(argv2[i], "bold") == 0) {
	    logfont->lfWeight = FW_BOLD;
	} else if (stricmp(argv2[i], "medium") == 0) {
	    logfont->lfWeight = FW_MEDIUM;
	} else if (stricmp(argv2[i], "heavy") == 0) {
	    logfont->lfWeight = FW_HEAVY;
	} else if (stricmp(argv2[i], "thin") == 0) {
	    logfont->lfWeight = FW_THIN;
	} else if (stricmp(argv2[i], "extralight") == 0) {
	    logfont->lfWeight = FW_EXTRALIGHT;
	} else if (stricmp(argv2[i], "light") == 0) {
	    logfont->lfWeight = FW_LIGHT;
	} else if (stricmp(argv2[i], "semibold") == 0) {
	    logfont->lfWeight = FW_SEMIBOLD;
	} else if (stricmp(argv2[i], "extrabold") == 0) {
	    logfont->lfWeight = FW_EXTRABOLD;
	} else if (stricmp(argv2[i], "italic") == 0) {
	    logfont->lfItalic = TRUE;
	} else if (stricmp(argv2[i], "oblique") == 0) {
	    logfont->lfOrientation = 3600 - 150; /* 15 degree forward slant */
	} else if (stricmp(argv2[i], "underline") == 0) {
	    logfont->lfUnderline = TRUE;
	} else if (stricmp(argv2[i], "strikeout") == 0) {
	    logfont->lfStrikeOut = TRUE;
	} else {
	    /* ignore for now */
	}
    }

    ckfree((char *) argv);
    ckfree((char *) argv2);
    return True;

    nomatch:
    Tcl_DeleteInterp(dummy);
    return False;
}

/*
 *----------------------------------------------------------------------
 *
 * XNameToFont --
 *
 *	This function constructs a logical font description from an
 *	X font name.  This code only handles font names with all 13
 *	parts, although a part can be '*'.
 *
 * Results:
 *	Returns false if the font name was syntactically invalid,
 *	else true.  Sets the fields of the passed in LOGFONT.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
XNameToFont(name, logfont)
    _Xconst char *name;
    LOGFONT *logfont;
{
    const char *head, *tail;
    const char *field[13];
    int flen[13];
    int i, len, parsefields;

    /*
     * Valid font name patterns must have a leading '-' or '*'.
     */

    head = tail = name;
    if (*tail == '-') {
	head++; tail++;
    } else if (*tail != '*') {
	return FALSE;
    }

    /*
     * Identify field boundaries.  Stores a pointer to the beginning
     * of each field in field[i], and the length of the field in flen[i].
     * Fields are separated by dashes.  Each '*' becomes a field by itself.
     */

    i = 0;
    while (*tail != '\0' && i < 12) {
	if (*tail == '-') {
	    flen[i] = tail - head;
	    field[i] = head;
	    tail++;
	    head = tail;
	    i++;
	} else if (*tail == '*') {
	    len = tail - head;
	    if (len > 0) {
		flen[i] = tail - head;
		field[i] = head;
	    } else {
		flen[i] = 1;
		field[i] = head;
		tail++;
		if (*tail == '-') {
		    tail++;
		}
	    }
	    head = tail;
	    i++;
	} else {
	    tail++;
	}
    }

    /*
     * We handle the last field as a special case, since it may contain
     * an emedded hyphen.
     */

    flen[i] = strlen(head);
    field[i] = head;

    /*
     * Bail if we don't have all of the fields.
     */

    if (i != 12) {
	return FALSE;
    } 

    /*
     * Now fill in the logical font description from the fields we have
     * identified.
     */

    memset(logfont, '\0', sizeof(LOGFONT));

    /*
     * Field 1: Foundry.  Skip.
     */

    /*
     * Field 2: Font Family.
     */

    i = 1;
    if (!(flen[i] == 0 ||
	  (flen[i] == 1 && (field[i][0] == '*' || field[i][0] == '?'))))
    {
	len = (flen[i] < LF_FACESIZE) ? flen[i] : LF_FACESIZE - 1;
	strncpy(logfont->lfFaceName, field[i], len);

	/*
	 * Need to handle Symbol and WingDings specially.
	 */

	if (stricmp(logfont->lfFaceName, "Symbol") == 0) {
	    logfont->lfCharSet = SYMBOL_CHARSET;
	} else if (stricmp(logfont->lfFaceName, "WingDings") == 0) {
	    logfont->lfCharSet = SYMBOL_CHARSET;
	}
    }

    /*
     * Field 3: Weight.  Default is medium.
     */

    i = 2;
    if ((flen[i] > 0) && (strnicmp(field[i], "bold", flen[i]) == 0)) {
	logfont->lfWeight = FW_BOLD;
    } else {
	logfont->lfWeight = FW_MEDIUM;
    }
	    
    /*
     * Field 4: Slant.  Default is Roman.
     */
    
    i = 3;
    if (!(flen[i] == 0 ||
	  (flen[i] == 1 && (field[i][0] == '*' || field[i][0] == '?'))))
    {
	if (strnicmp(field[i], "r", flen[i]) == 0) {
	    /* Roman.  Don't do anything */
	} else if (strnicmp(field[i], "i", flen[i]) == 0) {
	    /* Italic */
	    logfont->lfItalic = TRUE;
	} else if (strnicmp(field[i], "o", flen[i]) == 0) {
	    /* Oblique */
	    logfont->lfOrientation = 3600 - 150; /* 15 degree slant forward */
	} else if (strnicmp(field[i], "ri", flen[i]) == 0) {
	    /* Reverse Italic */
	    logfont->lfOrientation = 300;        /* 30 degree slant backward */
	} else if (strnicmp(field[i], "ro", flen[i]) == 0) {
	    /* Reverse Oblique */
	    logfont->lfOrientation = 150;        /* 30 degree slant backward */
	} else if (strnicmp(field[i], "ot", flen[i]) == 0) {
	    /* Other */
	} else {
	    return FALSE;
	}
    }

    /*
     * Field 5 & 6: Set Width & Blank.  Skip.
     */

    /*
     * Field 7: Pixels.  Use this as the points if no points set.
     */

    i = 6;
    if (!(flen[i] == 0 ||
	  (flen[i] == 1 && (field[i][0] == '*' || field[i][0] == '?'))))
    {
	logfont->lfHeight = -atoi(field[i]);
    }

    /*
     * Field 8: Points in tenths of a point.
     */

    i = 7;
    if (!(flen[i] == 0 ||
	  (flen[i] == 1 && (field[i][0] == '*' || field[i][0] == '?'))))
    {
	logfont->lfHeight = -(atoi(field[i]) / 10);
    }

    /*
     * Field 9: Horizontal Resolution in DPI.  Skip.
     * Field 10: Vertical Resolution in DPI.  Skip.
     */

    /*
     * Field 11: Spacing.
     */

    i = 10;
    if (!(flen[i] == 0 ||
	  (flen[i] == 1 && (field[i][0] == '*' || field[i][0] == '?'))))
    {
	if (flen[i] != 1) {
	    return FALSE;
	}
	if (field[i][0] == 'p' || field[i][0] == 'P') {
	    logfont->lfPitchAndFamily |= VARIABLE_PITCH;
	} else if (field[i][0] == 'm' || field[i][0] == 'm' ||
		   field[i][0] == 'c' || field[i][0] == 'c')
	{
	    logfont->lfPitchAndFamily |= FIXED_PITCH;
	} else {
	    return FALSE;
	}
    }

    /*
     * Field 12: Average Width.
     */

    i = 11;
    if (!(flen[i] == 0 ||
	  (flen[i] == 1 && (field[i][0] == '*' || field[i][0] == '?'))))
    {
	logfont->lfWidth = (atoi(field[i]) / 10);
    }

    /*
     * Field 13: Character Set.  Skip.
     */

    return TRUE;
}

/*
 *----------------------------------------------------------------------
 *
 * XLoadFont --
 *
 *	Get the font handle for the specified font.
 *
 * Results:
 *	Returns the font handle.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Font
XLoadFont(display, name)
    Display* display;
    _Xconst char* name;
{
    HFONT font;
    LOGFONT logfont;

    if (((name[0] == '-') || (name[0] == '*'))
	    && XNameToFont(name, &logfont)) {
	font = CreateFontIndirect(&logfont);
    } else if (NameToFont(name, &logfont)) {
	font = CreateFontIndirect(&logfont);
    } else {
	int object = SYSTEM_FONT;

	if (stricmp(name, "system") == 0) {
	    object = SYSTEM_FONT;
	} else if (stricmp(name, "systemfixed") == 0) {
	    object = SYSTEM_FIXED_FONT;
	} else if (stricmp(name, "ansi") == 0) {
	    object = ANSI_VAR_FONT;
	} else if (stricmp(name, "ansifixed") == 0) {
	    object = ANSI_FIXED_FONT;
	} else if (stricmp(name, "device") == 0) {
	    object = DEVICE_DEFAULT_FONT;
	} else if (stricmp(name, "oemfixed") == 0) {
	    object = OEM_FIXED_FONT;
	}
	font = GetStockObject(object);
    }
    if (font == NULL) {
	font = GetStockObject(SYSTEM_FONT);
    }
    return (Font) font;
}

/*
 *----------------------------------------------------------------------
 *
 * XQueryFont --
 *
 *	Retrieve information about the specified font.
 *
 * Results:
 *	Returns a newly allocated XFontStruct.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

XFontStruct *
XQueryFont(display, font_ID)
    Display* display;
    XID font_ID;
{
    XFontStruct *fontPtr = (XFontStruct *) ckalloc(sizeof(XFontStruct));
    HFONT oldFont;
    HDC dc;
    TEXTMETRIC tm;
    XCharStruct bounds;

    if (!fontPtr) {
	return NULL;
    }
    
    fontPtr->fid = font_ID;

    dc = GetDC(NULL);
    oldFont = SelectObject(dc, (HFONT) fontPtr->fid);

    /*
     * Determine the font metrics and store the values into the appropriate
     * X data structures.
     */

    if (GetTextMetrics(dc, &tm)) {
	fontPtr->direction = FontLeftToRight;
	fontPtr->min_byte1 = 0;
	fontPtr->max_byte1 = 0;
	fontPtr->min_char_or_byte2 = tm.tmFirstChar;
	fontPtr->max_char_or_byte2 = tm.tmLastChar;
	fontPtr->all_chars_exist = True;
	fontPtr->default_char = tm.tmDefaultChar;
	fontPtr->n_properties = 0;
	fontPtr->properties = NULL;
	bounds.lbearing = 0;
	bounds.rbearing = tm.tmMaxCharWidth;
	bounds.width = tm.tmMaxCharWidth;
	bounds.ascent = tm.tmAscent;
	bounds.descent = tm.tmDescent;
	bounds.attributes = 0;
	fontPtr->min_bounds = bounds;
	fontPtr->max_bounds = bounds;
	fontPtr->ascent = tm.tmAscent;
	fontPtr->descent = tm.tmDescent;

	/*
	 * If the font is not fixed pitch, then we need to construct
	 * the per_char array.
	 */

	if (tm.tmAveCharWidth != tm.tmMaxCharWidth) {
	    int i;
	    int nchars = tm.tmLastChar - tm.tmFirstChar + 1;
	    int minWidth = 30000;

	    fontPtr->per_char =
		(XCharStruct *)ckalloc(sizeof(XCharStruct) * nchars);

	    if (tm.tmPitchAndFamily & TMPF_TRUETYPE) {
		ABC *chars = (ABC*)ckalloc(sizeof(ABC) * nchars);

		GetCharABCWidths(dc, tm.tmFirstChar, tm.tmLastChar, chars);
		for (i = 0; i < nchars; i++) {
		    fontPtr->per_char[i].ascent = tm.tmAscent;
		    fontPtr->per_char[i].descent = tm.tmDescent;
		    fontPtr->per_char[i].attributes = 0;
		    fontPtr->per_char[i].lbearing = chars[i].abcA;
		    fontPtr->per_char[i].rbearing = chars[i].abcA
			+ chars[i].abcB;
		    fontPtr->per_char[i].width = chars[i].abcA + chars[i].abcB
			+ chars[i].abcC;
		}
		ckfree((char *)chars);
	    } else {
		int *chars = (int *)ckalloc(sizeof(int) * nchars);

		GetCharWidth(dc, tm.tmFirstChar, tm.tmLastChar, chars);

		for (i = 0; i < nchars ; i++ ) {
		    fontPtr->per_char[i] = bounds;
		    fontPtr->per_char[i].width = chars[i];
		    if (minWidth > chars[i]) {
			minWidth = chars[i];
		    }
		}
		ckfree((char *)chars);
	    }
	    fontPtr->min_bounds.width = minWidth;
	} else {
	    fontPtr->per_char = NULL;
	}
    } else {
	ckfree((char *)fontPtr);
	fontPtr = NULL;
    }    

    SelectObject(dc, oldFont);
    ReleaseDC(NULL, dc);
    
    return fontPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * XLoadQueryFont --
 *
 *	Finds the closest available Windows font for the specified
 *	font name.
 *
 * Results:
 *	Allocates and returns an XFontStruct containing a description
 *	of the matching font.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

XFontStruct *
XLoadQueryFont(display, name)
    Display* display;
    _Xconst char* name;
{
    Font font;
    font = XLoadFont(display, name);
    return XQueryFont(display, font);
}

/*
 *----------------------------------------------------------------------
 *
 * XFreeFont --
 *
 *	Releases resources associated with the specified font.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Frees the memory referenced by font_struct.
 *
 *----------------------------------------------------------------------
 */

void
XFreeFont(display, font_struct)
    Display* display;
    XFontStruct* font_struct;
{
    DeleteObject((HFONT)font_struct->fid);
    if (font_struct->per_char != NULL) {
	ckfree((char *) font_struct->per_char);
    }
    ckfree((char *) font_struct);
}

/*
 *----------------------------------------------------------------------
 *
 * XTextExtents --
 *
 *	Compute the width of an 8-bit character string.
 *
 * Results:
 *	Returns the computed width of the specified string.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
XTextWidth(font_struct, string, count)
    XFontStruct* font_struct;
    _Xconst char* string;
    int count;
{
    TEXTMETRIC tm;
    int width;
    SIZE size;
    HFONT oldFont;
    HDC dc;

    dc = GetDC(NULL);
    oldFont = SelectObject(dc, (HFONT)font_struct->fid);

    GetTextExtentPoint(dc, string, count, &size);
    GetTextMetrics(dc, &tm);
    size.cx -= tm.tmOverhang;

    SelectObject(dc, oldFont);
    ReleaseDC(NULL, dc);

    return size.cx;
}

/*
 *----------------------------------------------------------------------
 *
 * XTextExtents --
 *
 *	Compute the bounding box for a string.
 *
 * Results:
 *	Sets the direction_return, ascent_return, descent_return, and
 *	overall_return values as defined by Xlib.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
XTextExtents(font_struct, string, nchars, direction_return,
	font_ascent_return, font_descent_return, overall_return)
    XFontStruct* font_struct;
    _Xconst char* string;
    int nchars;
    int* direction_return;
    int* font_ascent_return;
    int* font_descent_return;
    XCharStruct* overall_return;
{
    HDC dc;
    HFONT oldFont;
    TEXTMETRIC tm;
    SIZE size;

    *direction_return = font_struct->direction;
    *font_ascent_return = font_struct->ascent;
    *font_descent_return = font_struct->descent;

    dc = GetDC(NULL);
    oldFont = SelectObject(dc, (HFONT)font_struct->fid);

    GetTextMetrics(dc, &tm);
    overall_return->ascent = tm.tmAscent;
    overall_return->descent = tm.tmDescent;
    GetTextExtentPoint(dc, string, nchars, &size);
    overall_return->width = size.cx;
    overall_return->lbearing = 0;
    overall_return->rbearing = overall_return->width - tm.tmOverhang;

    SelectObject(dc, oldFont);
    ReleaseDC(NULL, dc);
}


/*
 *----------------------------------------------------------------------
 *
 * XGetFontProperty --
 *
 *	Called to get font properties.  Since font properties are not
 *	supported under Windows, this function is a no-op.
 *
 * Results:
 *	Always returns false
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Bool
XGetFontProperty(font_struct, atom, value_return)
    XFontStruct* font_struct;
    Atom atom;
    unsigned long* value_return;
{
    return False;
}

