/* 
 * tkImage.c --
 *
 *	This module implements the image protocol, which allows lots
 *	of different kinds of images to be used in lots of different
 *	widgets.
 *
 * Copyright (c) 1994 The Regents of the University of California.
 * Copyright (c) 1994-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkImage.c 1.11 96/03/01 17:19:28
 */

#include "tkInt.h"
#include "tkPort.h"

/*
 * Each call to Tk_GetImage returns a pointer to one of the following
 * structures, which is used as a token by clients (widgets) that
 * display images.
 */

typedef struct Image {
    Tk_Window tkwin;		/* Window passed to Tk_GetImage (needed to
				 * "re-get" the image later if the manager
				 * changes). */
    Display *display;		/* Display for tkwin.  Needed because when
				 * the image is eventually freed tkwin may
				 * not exist anymore. */
    struct ImageMaster *masterPtr;
				/* Master for this image (identifiers image
				 * manager, for example). */
    ClientData instanceData;
				/* One word argument to pass to image manager
				 * when dealing with this image instance. */
    Tk_ImageChangedProc *changeProc;
				/* Code in widget to call when image changes
				 * in a way that affects redisplay. */
    ClientData widgetClientData;
				/* Argument to pass to changeProc. */
    struct Image *nextPtr;	/* Next in list of all image instances
				 * associated with the same name. */

} Image;

/*
 * For each image master there is one of the following structures,
 * which represents a name in the image table and all of the images
 * instantiated from it.  Entries in mainPtr->imageTable point to
 * these structures.
 */

typedef struct ImageMaster {
    Tk_ImageType *typePtr;	/* Information about image type.  NULL means
				 * that no image manager owns this image:  the
				 * image was deleted. */
    ClientData masterData;	/* One-word argument to pass to image mgr
				 * when dealing with the master, as opposed
				 * to instances. */
    int width, height;		/* Last known dimensions for image. */
    Tcl_HashTable *tablePtr;	/* Pointer to hash table containing image
				 * (the imageTable field in some TkMainInfo
				 * structure). */
    Tcl_HashEntry *hPtr;	/* Hash entry in mainPtr->imageTable for
				 * this structure (used to delete the hash
				 * entry). */
    Image *instancePtr;		/* Pointer to first in list of instances
				 * derived from this name. */
} ImageMaster;

/*
 * The following variable points to the first in a list of all known
 * image types.
 */

static Tk_ImageType *imageTypeList = NULL;

/*
 * Prototypes for local procedures:
 */

static void		DeleteImage _ANSI_ARGS_((ImageMaster *masterPtr));

/*
 *----------------------------------------------------------------------
 *
 * Tk_CreateImageType --
 *
 *	This procedure is invoked by an image manager to tell Tk about
 *	a new kind of image and the procedures that manage the new type.
 *	The procedure is typically invoked during Tcl_AppInit.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The new image type is entered into a table used in the "image
 *	create" command.
 *
 *----------------------------------------------------------------------
 */

void
Tk_CreateImageType(typePtr)
    Tk_ImageType *typePtr;	/* Structure describing the type.  All of
				 * the fields except "nextPtr" must be filled
				 * in by caller.  Must not have been passed
				 * to Tk_CreateImageType previously. */
{
    Tk_ImageType *typePtr2;

    typePtr2 = (Tk_ImageType *) ckalloc(sizeof(Tk_ImageType));
    *typePtr2 = *typePtr;
    typePtr2->name = (char *) ckalloc((unsigned) (strlen(typePtr->name) + 1));
    strcpy(typePtr2->name, typePtr->name);
    typePtr2->nextPtr = imageTypeList;
    imageTypeList = typePtr2;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_ImageCmd --
 *
 *	This procedure is invoked to process the "image" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Tk_ImageCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    TkWindow *winPtr = (TkWindow *) clientData;
    int c, i, new, firstOption;
    size_t length;
    Tk_ImageType *typePtr;
    ImageMaster *masterPtr;
    Image *imagePtr;
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;
    char idString[30], *name;
    static int id = 0;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" option ?args?\"", (char *) NULL);
	return TCL_ERROR;
    }
    c = argv[1][0];
    length = strlen(argv[1]);
    if ((c == 'c') && (strncmp(argv[1], "create", length) == 0)) {
	if (argc < 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		    " create type ?name? ?options?\"", (char *) NULL);
	    return TCL_ERROR;
	}
	c = argv[2][0];

	/*
	 * Look up the image type.
	 */

	for (typePtr = imageTypeList; typePtr != NULL;
		typePtr = typePtr->nextPtr) {
	    if ((c == typePtr->name[0])
		    && (strcmp(argv[2], typePtr->name) == 0)) {
		break;
	    }
	}
	if (typePtr == NULL) {
	    Tcl_AppendResult(interp, "image type \"", argv[2],
		    "\" doesn't exist", (char *) NULL);
	    return TCL_ERROR;
	}

	/*
	 * Figure out a name to use for the new image.
	 */

	if ((argc == 3) || (argv[3][0] == '-')) {
	    id++;
	    sprintf(idString, "image%d", id);
	    name = idString;
	    firstOption = 3;
	} else {
	    name = argv[3];
	    firstOption = 4;
	}

	/*
	 * Create the data structure for the new image.
	 */

	hPtr = Tcl_CreateHashEntry(&winPtr->mainPtr->imageTable, name, &new);
	if (new) {
	    masterPtr = (ImageMaster *) ckalloc(sizeof(ImageMaster));
	    masterPtr->typePtr = NULL;
	    masterPtr->masterData = NULL;
	    masterPtr->width = masterPtr->height = 1;
	    masterPtr->tablePtr = &winPtr->mainPtr->imageTable;
	    masterPtr->hPtr = hPtr;
	    masterPtr->instancePtr = NULL;
	    Tcl_SetHashValue(hPtr, masterPtr);
	} else {
	    /*
	     * An image already exists by this name.  Disconnect the
	     * instances from the master.
	     */

	    masterPtr = (ImageMaster *) Tcl_GetHashValue(hPtr);
	    if (masterPtr->typePtr != NULL) {
		for (imagePtr = masterPtr->instancePtr; imagePtr != NULL;
			imagePtr = imagePtr->nextPtr) {
		   (*masterPtr->typePtr->freeProc)(
			   imagePtr->instanceData, imagePtr->display);
		   (*imagePtr->changeProc)(imagePtr->widgetClientData, 0, 0,
			masterPtr->width, masterPtr->height, masterPtr->width,
			masterPtr->height);
		}
		(*masterPtr->typePtr->deleteProc)(masterPtr->masterData);
		masterPtr->typePtr = NULL;
	    }
	}

	/*
	 * Call the image type manager so that it can perform its own
	 * initialization, then re-"get" for any existing instances of
	 * the image.
	 */

	if ((*typePtr->createProc)(interp, name, argc-firstOption,
		argv+firstOption, typePtr, (Tk_ImageMaster) masterPtr,
		&masterPtr->masterData) != TCL_OK) {
	    DeleteImage(masterPtr);
	    return TCL_ERROR;
	}
	masterPtr->typePtr = typePtr;
	for (imagePtr = masterPtr->instancePtr; imagePtr != NULL;
		imagePtr = imagePtr->nextPtr) {
	   imagePtr->instanceData = (*typePtr->getProc)(
		   imagePtr->tkwin, masterPtr->masterData);
	}
	interp->result = Tcl_GetHashKey(&winPtr->mainPtr->imageTable, hPtr);
    } else if ((c == 'd') && (strncmp(argv[1], "delete", length) == 0)) {
	for (i = 2; i < argc; i++) {
	    hPtr = Tcl_FindHashEntry(&winPtr->mainPtr->imageTable, argv[i]);
	    if (hPtr == NULL) {
	    Tcl_AppendResult(interp, "image \"", argv[i],
		    "\" doesn't exist", (char *) NULL);
		return TCL_ERROR;
	    }
	    masterPtr = (ImageMaster *) Tcl_GetHashValue(hPtr);
	    DeleteImage(masterPtr);
	}
    } else if ((c == 'h') && (strncmp(argv[1], "height", length) == 0)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		    " height name\"", (char *) NULL);
	    return TCL_ERROR;
	}
	hPtr = Tcl_FindHashEntry(&winPtr->mainPtr->imageTable, argv[2]);
	if (hPtr == NULL) {
	    Tcl_AppendResult(interp, "image \"", argv[2],
		    "\" doesn't exist", (char *) NULL);
	    return TCL_ERROR;
	}
	masterPtr = (ImageMaster *) Tcl_GetHashValue(hPtr);
	sprintf(interp->result, "%d", masterPtr->height);
    } else if ((c == 'n') && (strncmp(argv[1], "names", length) == 0)) {
	if (argc != 2) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		    " names\"", (char *) NULL);
	    return TCL_ERROR;
	}
	for (hPtr = Tcl_FirstHashEntry(&winPtr->mainPtr->imageTable, &search);
		hPtr != NULL; hPtr = Tcl_NextHashEntry(&search)) {
	    Tcl_AppendElement(interp, Tcl_GetHashKey(
		    &winPtr->mainPtr->imageTable, hPtr));
	}
    } else if ((c == 't') && (strcmp(argv[1], "type") == 0)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		    " type name\"", (char *) NULL);
	    return TCL_ERROR;
	}
	hPtr = Tcl_FindHashEntry(&winPtr->mainPtr->imageTable, argv[2]);
	if (hPtr == NULL) {
	    Tcl_AppendResult(interp, "image \"", argv[2],
		    "\" doesn't exist", (char *) NULL);
	    return TCL_ERROR;
	}
	masterPtr = (ImageMaster *) Tcl_GetHashValue(hPtr);
	if (masterPtr->typePtr != NULL) {
	    interp->result = masterPtr->typePtr->name;
	}
    } else if ((c == 't') && (strcmp(argv[1], "types") == 0)) {
	if (argc != 2) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		    " types\"", (char *) NULL);
	    return TCL_ERROR;
	}
	for (typePtr = imageTypeList; typePtr != NULL;
		typePtr = typePtr->nextPtr) {
	    Tcl_AppendElement(interp, typePtr->name);
	}
    } else if ((c == 'w') && (strncmp(argv[1], "width", length) == 0)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		    " width name\"", (char *) NULL);
	    return TCL_ERROR;
	}
	hPtr = Tcl_FindHashEntry(&winPtr->mainPtr->imageTable, argv[2]);
	if (hPtr == NULL) {
	    Tcl_AppendResult(interp, "image \"", argv[2],
		    "\" doesn't exist", (char *) NULL);
	    return TCL_ERROR;
	}
	masterPtr = (ImageMaster *) Tcl_GetHashValue(hPtr);
	sprintf(interp->result, "%d", masterPtr->width);
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\": must be create, delete, height, names, type, types,",
		" or width", (char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_ImageChanged --
 *
 *	This procedure is called by an image manager whenever something
 *	has happened that requires the image to be redrawn (some of its
 *	pixels have changed, or its size has changed).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Any widgets that display the image are notified so that they
 *	can redisplay themselves as appropriate.
 *
 *----------------------------------------------------------------------
 */

void
Tk_ImageChanged(imageMaster, x, y, width, height, imageWidth,
	imageHeight)
    Tk_ImageMaster imageMaster;	/* Image that needs redisplay. */
    int x, y;			/* Coordinates of upper-left pixel of
				 * region of image that needs to be
				 * redrawn. */
    int width, height;		/* Dimensions (in pixels) of region of
				 * image to redraw.  If either dimension
				 * is zero then the image doesn't need to
				 * be redrawn (perhaps all that happened is
				 * that its size changed). */
    int imageWidth, imageHeight;/* New dimensions of image. */
{
    ImageMaster *masterPtr = (ImageMaster *) imageMaster;
    Image *imagePtr;

    masterPtr->width = imageWidth;
    masterPtr->height = imageHeight;
    for (imagePtr = masterPtr->instancePtr; imagePtr != NULL;
	    imagePtr = imagePtr->nextPtr) {
	(*imagePtr->changeProc)(imagePtr->widgetClientData, x, y,
	    width, height, imageWidth, imageHeight);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_NameOfImage --
 *
 *	Given a token for an image master, this procedure returns
 *	the name of the image.
 *
 * Results:
 *	The return value is the string name for imageMaster.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char *
Tk_NameOfImage(imageMaster)
    Tk_ImageMaster imageMaster;		/* Token for image. */
{
    ImageMaster *masterPtr = (ImageMaster *) imageMaster;

    return Tcl_GetHashKey(masterPtr->tablePtr, masterPtr->hPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_GetImage --
 *
 *	This procedure is invoked by a widget when it wants to use
 *	a particular image in a particular window.
 *
 * Results:
 *	The return value is a token for the image.  If there is no image
 *	by the given name, then NULL is returned and an error message is
 *	left in interp->result.
 *
 * Side effects:
 *	Tk records the fact that the widget is using the image, and
 *	it will invoke changeProc later if the widget needs redisplay
 *	(i.e. its size changes or some of its pixels change).  The
 *	caller must eventually invoke Tk_FreeImage when it no longer
 *	needs the image.
 *
 *----------------------------------------------------------------------
 */

Tk_Image
Tk_GetImage(interp, tkwin, name, changeProc, clientData)
    Tcl_Interp *interp;		/* Place to leave error message if image
				 * can't be found. */
    Tk_Window tkwin;		/* Token for window in which image will
				 * be used. */
    char *name;			/* Name of desired image. */
    Tk_ImageChangedProc *changeProc;
				/* Procedure to invoke when redisplay is
				 * needed because image's pixels or size
				 * changed. */
    ClientData clientData;	/* One-word argument to pass to damageProc. */
{
    Tcl_HashEntry *hPtr;
    ImageMaster *masterPtr;
    Image *imagePtr;

    hPtr = Tcl_FindHashEntry(&((TkWindow *) tkwin)->mainPtr->imageTable, name);
    if (hPtr == NULL) {
	goto noSuchImage;
    }
    masterPtr = (ImageMaster *) Tcl_GetHashValue(hPtr);
    if (masterPtr->typePtr == NULL) {
	goto noSuchImage;
    }
    imagePtr = (Image *) ckalloc(sizeof(Image));
    imagePtr->tkwin = tkwin;
    imagePtr->display = Tk_Display(tkwin);
    imagePtr->masterPtr = masterPtr;
    imagePtr->instanceData =
	    (*masterPtr->typePtr->getProc)(tkwin, masterPtr->masterData);
    imagePtr->changeProc = changeProc;
    imagePtr->widgetClientData = clientData;
    imagePtr->nextPtr = masterPtr->instancePtr;
    masterPtr->instancePtr = imagePtr;
    return (Tk_Image) imagePtr;

    noSuchImage:
    Tcl_AppendResult(interp, "image \"", name, "\" doesn't exist",
	    (char *) NULL);
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_FreeImage --
 *
 *	This procedure is invoked by a widget when it no longer needs
 *	an image acquired by a previous call to Tk_GetImage.  For each
 *	call to Tk_GetImage there must be exactly one call to Tk_FreeImage.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The association between the image and the widget is removed.
 *
 *----------------------------------------------------------------------
 */

void
Tk_FreeImage(image)
    Tk_Image image;		/* Token for image that is no longer
				 * needed by a widget. */
{
    Image *imagePtr = (Image *) image;
    ImageMaster *masterPtr = imagePtr->masterPtr;
    Image *prevPtr;

    /*
     * Clean up the particular instance.
     */

    if (masterPtr->typePtr != NULL) {
	(*masterPtr->typePtr->freeProc)(imagePtr->instanceData,
		imagePtr->display);
    }
    prevPtr = masterPtr->instancePtr;
    if (prevPtr == imagePtr) {
	masterPtr->instancePtr = imagePtr->nextPtr;
    } else {
	while (prevPtr->nextPtr != imagePtr) {
	    prevPtr = prevPtr->nextPtr;
	}
	prevPtr->nextPtr = imagePtr->nextPtr;
    }
    ckfree((char *) imagePtr);

    /* 
     * If there are no more instances left for the master, and if the
     * master image has been deleted, then delete the master too.
     */

    if ((masterPtr->typePtr == NULL) && (masterPtr->instancePtr == NULL)) {
	Tcl_DeleteHashEntry(masterPtr->hPtr);
	ckfree((char *) masterPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_RedrawImage --
 *
 *	This procedure is called by widgets that contain images in order
 *	to redisplay an image on the screen or an off-screen pixmap.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The image's manager is notified, and it redraws the desired
 *	portion of the image before returning.
 *
 *----------------------------------------------------------------------
 */

void
Tk_RedrawImage(image, imageX, imageY, width, height, drawable,
	drawableX, drawableY)
    Tk_Image image;		/* Token for image to redisplay. */
    int imageX, imageY;		/* Upper-left pixel of region in image that
				 * needs to be redisplayed. */
    int width, height;		/* Dimensions of region to redraw. */
    Drawable drawable;		/* Drawable in which to display image
				 * (window or pixmap).  If this is a pixmap,
				 * it must have the same depth as the window
				 * used in the Tk_GetImage call for the
				 * image. */
    int drawableX, drawableY;	/* Coordinates in drawable that correspond
				 * to imageX and imageY. */
{
    Image *imagePtr = (Image *) image;

    if (imagePtr->masterPtr->typePtr == NULL) {
	/*
	 * No master for image, so nothing to display.
	 */

	return;
    }

    /*
     * Clip the redraw area to the area of the image.
     */

    if (imageX < 0) {
	width += imageX;
	drawableX -= imageX;
	imageX = 0;
    }
    if (imageY < 0) {
	height += imageY;
	drawableY -= imageY;
	imageY = 0;
    }
    if ((imageX + width) > imagePtr->masterPtr->width) {
	width = imagePtr->masterPtr->width - imageX;
    }
    if ((imageY + height) > imagePtr->masterPtr->height) {
	height = imagePtr->masterPtr->height - imageY;
    }
    (*imagePtr->masterPtr->typePtr->displayProc)(
	    imagePtr->instanceData, imagePtr->display, drawable,
	    imageX, imageY, width, height, drawableX, drawableY);
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_SizeOfImage --
 *
 *	This procedure returns the current dimensions of an image.
 *
 * Results:
 *	The width and height of the image are returned in *widthPtr
 *	and *heightPtr.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
Tk_SizeOfImage(image, widthPtr, heightPtr)
    Tk_Image image;		/* Token for image whose size is wanted. */
    int *widthPtr;		/* Return width of image here. */
    int *heightPtr;		/* Return height of image here. */
{
    Image *imagePtr = (Image *) image;

    *widthPtr = imagePtr->masterPtr->width;
    *heightPtr = imagePtr->masterPtr->height;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_DeleteImage --
 *
 *	Given the name of an image, this procedure destroys the
 *	image.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The image is destroyed; existing instances will display as
 *	blank areas.  If no such image exists then the procedure does
 *	nothing.
 *
 *----------------------------------------------------------------------
 */

void
Tk_DeleteImage(interp, name)
    Tcl_Interp *interp;		/* Interpreter in which the image was
				 * created. */
    char *name;			/* Name of image. */
{
    Tcl_HashEntry *hPtr;
    Tcl_CmdInfo info;
    TkWindow *winPtr;

    if (Tcl_GetCommandInfo(interp, "winfo", &info) == 0) {
	return;
    }
    winPtr = (TkWindow *) info.clientData;
    hPtr = Tcl_FindHashEntry(&winPtr->mainPtr->imageTable, name);
    if (hPtr == NULL) {
	return;
    }
    DeleteImage((ImageMaster *) Tcl_GetHashValue(hPtr));
}

/*
 *----------------------------------------------------------------------
 *
 * DeleteImage --
 *
 *	This procedure is responsible for deleting an image.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The connection is dropped between instances of this image and
 *	an image master.  Image instances will redisplay themselves
 *	as empty areas, but existing instances will not be deleted.
 *
 *----------------------------------------------------------------------
 */

static void
DeleteImage(masterPtr)
    ImageMaster *masterPtr;	/* Pointer to main data structure for image. */
{
    Image *imagePtr;
    Tk_ImageType *typePtr;

    typePtr = masterPtr->typePtr;
    masterPtr->typePtr = NULL;
    if (typePtr != NULL) {
	for (imagePtr = masterPtr->instancePtr; imagePtr != NULL;
		imagePtr = imagePtr->nextPtr) {
	   (*typePtr->freeProc)(imagePtr->instanceData,
		   imagePtr->display);
	   (*imagePtr->changeProc)(imagePtr->widgetClientData, 0, 0,
		    masterPtr->width, masterPtr->height, masterPtr->width,
		    masterPtr->height);
	}
	(*typePtr->deleteProc)(masterPtr->masterData);
    }
    if (masterPtr->instancePtr == NULL) {
	Tcl_DeleteHashEntry(masterPtr->hPtr);
	ckfree((char *) masterPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkDeleteAllImages --
 *
 *	This procedure is called when an application is deleted.  It
 *	calls back all of the managers for all images so that they
 *	can cleanup, then it deletes all of Tk's internal information
 *	about images.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	All information for all images gets deleted.
 *
 *----------------------------------------------------------------------
 */

void
TkDeleteAllImages(mainPtr)
    TkMainInfo *mainPtr;	/* Structure describing application that is
				 * going away. */
{
    Tcl_HashSearch search;
    Tcl_HashEntry *hPtr;
    ImageMaster *masterPtr;

    for (hPtr = Tcl_FirstHashEntry(&mainPtr->imageTable, &search);
	    hPtr != NULL; hPtr = Tcl_NextHashEntry(&search)) {
	masterPtr = (ImageMaster *) Tcl_GetHashValue(hPtr);
	DeleteImage(masterPtr);
    }
    Tcl_DeleteHashTable(&mainPtr->imageTable);
}
