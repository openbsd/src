/*
 * tkImgPhoto.c --
 *
 *	Implements images of type "photo" for Tk.  Photo images are
 *	stored in full color (24 bits per pixel) and displayed using
 *	dithering if necessary.
 *
 * Copyright (c) 1994 The Australian National University.
 * Copyright (c) 1994-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Author: Paul Mackerras (paulus@cs.anu.edu.au),
 *	   Department of Computer Science,
 *	   Australian National University.
 *
 * SCCS: @(#) tkImgPhoto.c 1.44 96/03/01 17:35:11
 */

#include "tkInt.h"
#include "tkPort.h"
#include <math.h>
#include <ctype.h>

/*
 * Declaration for internal Xlib function used here:
 */

extern _XInitImageFuncPtrs _ANSI_ARGS_((XImage *image));

/*
 * A signed 8-bit integral type.  If chars are unsigned and the compiler
 * isn't an ANSI one, then we have to use short instead (which wastes
 * space) to get signed behavior.
 */

#if defined(__STDC__) || defined(_AIX)
    typedef signed char schar;
#else
#   ifndef __CHAR_UNSIGNED__
	typedef char schar;
#   else
	typedef short schar;
#   endif
#endif

/*
 * An unsigned 32-bit integral type, used for pixel values.
 * We use int rather than long here to accommodate those systems
 * where longs are 64 bits.
 */

typedef unsigned int pixel;

/*
 * The maximum number of pixels to transmit to the server in a
 * single XPutImage call.
 */

#define MAX_PIXELS 65536

/*
 * The set of colors required to display a photo image in a window depends on:
 *	- the visual used by the window
 *	- the palette, which specifies how many levels of each primary
 *	  color to use, and
 *	- the gamma value for the image.
 *
 * Pixel values allocated for specific colors are valid only for the
 * colormap in which they were allocated.  Sets of pixel values
 * allocated for displaying photos are re-used in other windows if
 * possible, that is, if the display, colormap, palette and gamma
 * values match.  A hash table is used to locate these sets of pixel
 * values, using the following data structure as key:
 */

typedef struct {
    Display *display;		/* Qualifies the colormap resource ID */
    Colormap colormap;		/* Colormap that the windows are using. */
    double gamma;		/* Gamma exponent value for images. */
    Tk_Uid palette;		/* Specifies how many shades of each primary
				 * we want to allocate. */
} ColorTableId;

/*
 * For a particular (display, colormap, palette, gamma) combination,
 * a data structure of the following type is used to store the allocated
 * pixel values and other information:
 */

typedef struct ColorTable {
    ColorTableId id;		/* Information used in selecting this
				 * color table. */
    int	flags;			/* See below. */
    int	refCount;		/* Number of instances using this map. */
    int liveRefCount;		/* Number of instances which are actually
				 * in use, using this map. */
    int	numColors;		/* Number of colors allocated for this map. */

    XVisualInfo	visualInfo;	/* Information about the visual for windows
				 * using this color table. */

    pixel redValues[256];	/* Maps 8-bit values of red intensity
				 * to a pixel value or index in pixelMap. */
    pixel greenValues[256];	/* Ditto for green intensity */
    pixel blueValues[256];	/* Ditto for blue intensity */
    unsigned long *pixelMap;	/* Actual pixel values allocated. */

    unsigned char colorQuant[3][256];
				/* Maps 8-bit intensities to quantized
				 * intensities.  The first index is 0 for
				 * red, 1 for green, 2 for blue. */
} ColorTable;

/*
 * Bit definitions for the flags field of a ColorTable.
 * BLACK_AND_WHITE:		1 means only black and white colors are
 *				available.
 * COLOR_WINDOW:		1 means a full 3-D color cube has been
 *				allocated.
 * DISPOSE_PENDING:		1 means a call to DisposeColorTable has
 *				been scheduled as an idle handler, but it
 *				hasn't been invoked yet.
 * MAP_COLORS:			1 means pixel values should be mapped
 *				through pixelMap.
 */

#define BLACK_AND_WHITE		1
#define COLOR_WINDOW		2
#define DISPOSE_PENDING		4
#define MAP_COLORS		8

/*
 * Definition of the data associated with each photo image master.
 */

typedef struct PhotoMaster {
    Tk_ImageMaster tkMaster;	/* Tk's token for image master.  NULL means
				 * the image is being deleted. */
    Tcl_Interp *interp;		/* Interpreter associated with the
				 * application using this image. */
    Tcl_Command imageCmd;	/* Token for image command (used to delete
				 * it when the image goes away).  NULL means
				 * the image command has already been
				 * deleted. */
    int	flags;			/* Sundry flags, defined below. */
    int	width, height;		/* Dimensions of image. */
    int userWidth, userHeight;	/* User-declared image dimensions. */
    Tk_Uid palette;		/* User-specified default palette for
				 * instances of this image. */
    double gamma;		/* Display gamma value to correct for. */
    char *fileString;		/* Name of file to read into image. */
    char *dataString;		/* String value to use as contents of image. */
    char *format;		/* User-specified format of data in image
				 * file or string value. */
    unsigned char *pix24;	/* Local storage for 24-bit image. */
    int ditherX, ditherY;	/* Location of first incorrectly
				 * dithered pixel in image. */
    TkRegion validRegion;	/* Tk region indicating which parts of
				 * the image have valid image data. */
    struct PhotoInstance *instancePtr;
				/* First in the list of instances
				 * associated with this master. */
} PhotoMaster;

/*
 * Bit definitions for the flags field of a PhotoMaster.
 * COLOR_IMAGE:			1 means that the image has different color
 *				components.
 * IMAGE_CHANGED:		1 means that the instances of this image
 *				need to be redithered.
 */

#define COLOR_IMAGE		1
#define IMAGE_CHANGED		2

/*
 * The following data structure represents all of the instances of
 * a photo image in windows on a given screen that are using the
 * same colormap.
 */

typedef struct PhotoInstance {
    PhotoMaster *masterPtr;	/* Pointer to master for image. */
    Display *display;		/* Display for windows using this instance. */
    Colormap colormap;		/* The image may only be used in windows with
				 * this particular colormap. */
    struct PhotoInstance *nextPtr;
				/* Pointer to the next instance in the list
				 * of instances associated with this master. */
    int refCount;		/* Number of instances using this structure. */
    Tk_Uid palette;		/* Palette for these particular instances. */
    double gamma;		/* Gamma value for these instances. */
    Tk_Uid defaultPalette;	/* Default palette to use if a palette
				 * is not specified for the master. */
    ColorTable *colorTablePtr;	/* Pointer to information about colors
				 * allocated for image display in windows
				 * like this one. */
    Pixmap pixels;		/* X pixmap containing dithered image. */
    int width, height;		/* Dimensions of the pixmap. */
    schar *error;		/* Error image, used in dithering. */
    XImage *imagePtr;		/* Image structure for converted pixels. */
    XVisualInfo visualInfo;	/* Information about the visual that these
				 * windows are using. */
    GC gc;			/* Graphics context for writing images
				 * to the pixmap. */
} PhotoInstance;

/*
 * The following data structure is used to return information
 * from ParseSubcommandOptions:
 */

struct SubcommandOptions {
    int options;		/* Individual bits indicate which
				 * options were specified - see below. */
    char *name;			/* Name specified without an option. */
    int fromX, fromY;		/* Values specified for -from option. */
    int fromX2, fromY2;		/* Second coordinate pair for -from option. */
    int toX, toY;		/* Values specified for -to option. */
    int toX2, toY2;		/* Second coordinate pair for -to option. */
    int zoomX, zoomY;		/* Values specified for -zoom option. */
    int subsampleX, subsampleY;	/* Values specified for -subsample option. */
    char *format;		/* Value specified for -format option. */
};

/*
 * Bit definitions for use with ParseSubcommandOptions:
 * Each bit is set in the allowedOptions parameter on a call to
 * ParseSubcommandOptions if that option is allowed for the current
 * photo image subcommand.  On return, the bit is set in the options
 * field of the SubcommandOptions structure if that option was specified.
 *
 * OPT_FORMAT:			Set if -format option allowed/specified.
 * OPT_FROM:			Set if -from option allowed/specified.
 * OPT_SHRINK:			Set if -shrink option allowed/specified.
 * OPT_SUBSAMPLE:		Set if -subsample option allowed/spec'd.
 * OPT_TO:			Set if -to option allowed/specified.
 * OPT_ZOOM:			Set if -zoom option allowed/specified.
 */

#define OPT_FORMAT	1
#define OPT_FROM	2
#define OPT_SHRINK	4
#define OPT_SUBSAMPLE	8
#define OPT_TO		0x10
#define OPT_ZOOM	0x20

/*
 * List of option names.  The order here must match the order of
 * declarations of the OPT_* constants above.
 */

static char *optionNames[] = {
    "-format",
    "-from",
    "-shrink",
    "-subsample",
    "-to",
    "-zoom",
    (char *) NULL
};

/*
 * The type record for photo images:
 */

static int		ImgPhotoCreate _ANSI_ARGS_((Tcl_Interp *interp,
			    char *name, int argc, char **argv,
			    Tk_ImageType *typePtr, Tk_ImageMaster master,
			    ClientData *clientDataPtr));
static ClientData	ImgPhotoGet _ANSI_ARGS_((Tk_Window tkwin,
			    ClientData clientData));
static void		ImgPhotoDisplay _ANSI_ARGS_((ClientData clientData,
			    Display *display, Drawable drawable,
			    int imageX, int imageY, int width, int height,
			    int drawableX, int drawableY));
static void		ImgPhotoFree _ANSI_ARGS_((ClientData clientData,
			    Display *display));
static void		ImgPhotoDelete _ANSI_ARGS_((ClientData clientData));

Tk_ImageType tkPhotoImageType = {
    "photo",			/* name */
    ImgPhotoCreate,		/* createProc */
    ImgPhotoGet,		/* getProc */
    ImgPhotoDisplay,		/* displayProc */
    ImgPhotoFree,		/* freeProc */
    ImgPhotoDelete,		/* deleteProc */
    (Tk_ImageType *) NULL	/* nextPtr */
};

/*
 * Default configuration
 */

#define DEF_PHOTO_GAMMA		"1"
#define DEF_PHOTO_HEIGHT	"0"
#define DEF_PHOTO_PALETTE	""
#define DEF_PHOTO_WIDTH		"0"

/*
 * Information used for parsing configuration specifications:
 */
static Tk_ConfigSpec configSpecs[] = {
    {TK_CONFIG_STRING, "-data", (char *) NULL, (char *) NULL,
	 (char *) NULL, Tk_Offset(PhotoMaster, dataString), TK_CONFIG_NULL_OK},
    {TK_CONFIG_STRING, "-format", (char *) NULL, (char *) NULL,
	 (char *) NULL, Tk_Offset(PhotoMaster, format), TK_CONFIG_NULL_OK},
    {TK_CONFIG_STRING, "-file", (char *) NULL, (char *) NULL,
	 (char *) NULL, Tk_Offset(PhotoMaster, fileString), TK_CONFIG_NULL_OK},
    {TK_CONFIG_DOUBLE, "-gamma", (char *) NULL, (char *) NULL,
	 DEF_PHOTO_GAMMA, Tk_Offset(PhotoMaster, gamma), 0},
    {TK_CONFIG_INT, "-height", (char *) NULL, (char *) NULL,
	 DEF_PHOTO_HEIGHT, Tk_Offset(PhotoMaster, userHeight), 0},
    {TK_CONFIG_UID, "-palette", (char *) NULL, (char *) NULL,
	 DEF_PHOTO_PALETTE, Tk_Offset(PhotoMaster, palette), 0},
    {TK_CONFIG_INT, "-width", (char *) NULL, (char *) NULL,
	 DEF_PHOTO_WIDTH, Tk_Offset(PhotoMaster, userWidth), 0},
    {TK_CONFIG_END, (char *) NULL, (char *) NULL, (char *) NULL,
	 (char *) NULL, 0, 0}
};

/*
 * Hash table used to provide access to photo images from C code.
 */

static Tcl_HashTable imgPhotoHash;
static int imgPhotoHashInitialized;	/* set when Tcl_InitHashTable done */

/*
 * Hash table used to hash from (display, colormap, palette, gamma)
 * to ColorTable address.
 */

static Tcl_HashTable imgPhotoColorHash;
static int imgPhotoColorHashInitialized;
#define N_COLOR_HASH	(sizeof(ColorTableId) / sizeof(int))

/*
 * Pointer to the first in the list of known photo image formats.
 */

static Tk_PhotoImageFormat *formatList = NULL;

/*
 * Forward declarations
 */

static int		ImgPhotoCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
static int		ParseSubcommandOptions _ANSI_ARGS_((
			    struct SubcommandOptions *optPtr,
			    Tcl_Interp *interp, int allowedOptions,
			    int *indexPtr, int argc, char **argv));
static void		ImgPhotoCmdDeletedProc _ANSI_ARGS_((
			    ClientData clientData));
static int		ImgPhotoConfigureMaster _ANSI_ARGS_((
			    Tcl_Interp *interp, PhotoMaster *masterPtr,
			    int argc, char **argv, int flags));
static void		ImgPhotoConfigureInstance _ANSI_ARGS_((
			    PhotoInstance *instancePtr));
static void		ImgPhotoSetSize _ANSI_ARGS_((PhotoMaster *masterPtr,
			    int width, int height));
static void		ImgPhotoInstanceSetSize _ANSI_ARGS_((
			    PhotoInstance *instancePtr));
static int		IsValidPalette _ANSI_ARGS_((PhotoInstance *instancePtr,
			    char *palette));
static int		CountBits _ANSI_ARGS_((pixel mask));
static void		GetColorTable _ANSI_ARGS_((PhotoInstance *instancePtr));
static void		FreeColorTable _ANSI_ARGS_((ColorTable *colorPtr));
static void		AllocateColors _ANSI_ARGS_((ColorTable *colorPtr));
static void		DisposeColorTable _ANSI_ARGS_((ClientData clientData));
static void		DisposeInstance _ANSI_ARGS_((ClientData clientData));
static int		ReclaimColors _ANSI_ARGS_((ColorTableId *id,
			    int numColors));
static int		MatchFileFormat _ANSI_ARGS_((Tcl_Interp *interp,
			    FILE *f, char *fileName, char *formatString,
			    Tk_PhotoImageFormat **imageFormatPtr,
			    int *widthPtr, int *heightPtr));
static int		MatchStringFormat _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string, char *formatString,
			    Tk_PhotoImageFormat **imageFormatPtr,
			    int *widthPtr, int *heightPtr));
static void		Dither _ANSI_ARGS_((PhotoMaster *masterPtr,
			    int x, int y, int width, int height));
static void		DitherInstance _ANSI_ARGS_((PhotoInstance *instancePtr,
			    int x, int y, int width, int height));

#undef MIN
#define MIN(a, b)	((a) < (b)? (a): (b))
#undef MAX
#define MAX(a, b)	((a) > (b)? (a): (b))

/*
 *----------------------------------------------------------------------
 *
 * Tk_CreatePhotoImageFormat --
 *
 *	This procedure is invoked by an image file handler to register
 *	a new photo image format and the procedures that handle the
 *	new format.  The procedure is typically invoked during
 *	Tcl_AppInit.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The new image file format is entered into a table used in the
 *	photo image "read" and "write" subcommands.
 *
 *----------------------------------------------------------------------
 */

void
Tk_CreatePhotoImageFormat(formatPtr)
    Tk_PhotoImageFormat *formatPtr;
				/* Structure describing the format.  All of
				 * the fields except "nextPtr" must be filled
				 * in by caller.  Must not have been passed
				 * to Tk_CreatePhotoImageFormat previously. */
{
    Tk_PhotoImageFormat *copyPtr;

    copyPtr = (Tk_PhotoImageFormat *) ckalloc(sizeof(Tk_PhotoImageFormat));
    *copyPtr = *formatPtr;
    copyPtr->name = (char *) ckalloc((unsigned) (strlen(formatPtr->name) + 1));
    strcpy(copyPtr->name, formatPtr->name);
    copyPtr->nextPtr = formatList;
    formatList = copyPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * ImgPhotoCreate --
 *
 *	This procedure is called by the Tk image code to create
 *	a new photo image.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	The data structure for a new photo image is allocated and
 *	initialized.
 *
 *----------------------------------------------------------------------
 */

static int
ImgPhotoCreate(interp, name, argc, argv, typePtr, master, clientDataPtr)
    Tcl_Interp *interp;		/* Interpreter for application containing
				 * image. */
    char *name;			/* Name to use for image. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings for options (doesn't
				 * include image name or type). */
    Tk_ImageType *typePtr;	/* Pointer to our type record (not used). */
    Tk_ImageMaster master;	/* Token for image, to be used by us in
				 * later callbacks. */
    ClientData *clientDataPtr;	/* Store manager's token for image here;
				 * it will be returned in later callbacks. */
{
    PhotoMaster *masterPtr;
    Tcl_HashEntry *entry;
    int isNew;

    /*
     * Allocate and initialize the photo image master record.
     */

    masterPtr = (PhotoMaster *) ckalloc(sizeof(PhotoMaster));
    memset((void *) masterPtr, 0, sizeof(PhotoMaster));
    masterPtr->tkMaster = master;
    masterPtr->interp = interp;
    masterPtr->imageCmd = Tcl_CreateCommand(interp, name, ImgPhotoCmd,
	    (ClientData) masterPtr, ImgPhotoCmdDeletedProc);
    masterPtr->palette = NULL;
    masterPtr->pix24 = NULL;
    masterPtr->instancePtr = NULL;
    masterPtr->validRegion = TkCreateRegion();

    /*
     * Process configuration options given in the image create command.
     */

    if (ImgPhotoConfigureMaster(interp, masterPtr, argc, argv, 0) != TCL_OK) {
	ImgPhotoDelete((ClientData) masterPtr);
	return TCL_ERROR;
    }

    /*
     * Enter this photo image in the hash table.
     */

    if (!imgPhotoHashInitialized) {
	Tcl_InitHashTable(&imgPhotoHash, TCL_STRING_KEYS);
	imgPhotoHashInitialized = 1;
    }
    entry = Tcl_CreateHashEntry(&imgPhotoHash, name, &isNew);
    Tcl_SetHashValue(entry, masterPtr);

    *clientDataPtr = (ClientData) masterPtr;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ImgPhotoCmd --
 *
 *	This procedure is invoked to process the Tcl command that
 *	corresponds to a photo image.  See the user documentation
 *	for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
ImgPhotoCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Information about photo master. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    PhotoMaster *masterPtr = (PhotoMaster *) clientData;
    int c, result, index;
    int x, y, width, height;
    int dataWidth, dataHeight;
    struct SubcommandOptions options;
    int listArgc;
    char **listArgv;
    char **srcArgv;
    unsigned char *pixelPtr;
    Tk_PhotoImageBlock block;
    Tk_Window tkwin;
    char string[16];
    XColor color;
    Tk_PhotoImageFormat *imageFormat;
    int imageWidth, imageHeight;
    int matched;
    FILE *f;
    Tk_PhotoHandle srcHandle;
    size_t length;
    Tcl_DString buffer;
    char *realFileName;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" option ?arg arg ...?\"", (char *) NULL);
	return TCL_ERROR;
    }
    c = argv[1][0];
    length = strlen(argv[1]);

    if ((c == 'b') && (strncmp(argv[1], "blank", length) == 0)) {
	/*
	 * photo blank command - just call Tk_PhotoBlank.
	 */

	if (argc == 2) {
	    Tk_PhotoBlank(masterPtr);
	} else {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " blank\"", (char *) NULL);
	    return TCL_ERROR;
	}
    } else if ((c == 'c') && (length >= 2)
	    && (strncmp(argv[1], "cget", length) == 0)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " cget option\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	result = Tk_ConfigureValue(interp, Tk_MainWindow(interp), configSpecs,
		(char *) masterPtr, argv[2], 0);
    } else if ((c == 'c') && (length >= 3)
	    && (strncmp(argv[1], "configure", length) == 0)) {
	/*
	 * photo configure command - handle this in the standard way.
	 */

	if (argc == 2) {
	    return Tk_ConfigureInfo(interp, Tk_MainWindow(interp),
		    configSpecs, (char *) masterPtr, (char *) NULL, 0);
	}
	if (argc == 3) {
	    return Tk_ConfigureInfo(interp, Tk_MainWindow(interp),
		    configSpecs, (char *) masterPtr, argv[2], 0);
	}
	return ImgPhotoConfigureMaster(interp, masterPtr, argc-2, argv+2,
		TK_CONFIG_ARGV_ONLY);
    } else if ((c == 'c') && (length >= 3)
	    && (strncmp(argv[1], "copy", length) == 0)) {
	/*
	 * photo copy command - first parse options.
	 */

	index = 2;
	memset((VOID *) &options, 0, sizeof(options));
	options.zoomX = options.zoomY = 1;
	options.subsampleX = options.subsampleY = 1;
	options.name = NULL;
	if (ParseSubcommandOptions(&options, interp,
		OPT_FROM | OPT_TO | OPT_ZOOM | OPT_SUBSAMPLE | OPT_SHRINK,
		&index, argc, argv) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (options.name == NULL || index < argc) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		    " copy source-image ?-from x1 y1 x2 y2?",
		    " ?-to x1 y1 x2 y2? ?-zoom x y? ?-subsample x y?",
		    "\"", (char *) NULL);
	    return TCL_ERROR;
	}

	/*
	 * Look for the source image and get a pointer to its image data.
	 * Check the values given for the -from option.
	 */

	if ((srcHandle = Tk_FindPhoto(options.name)) == NULL) {
	    Tcl_AppendResult(interp, "image \"", argv[2], "\" doesn't",
		    " exist or is not a photo image", (char *) NULL);
	    return TCL_ERROR;
	}
	Tk_PhotoGetImage(srcHandle, &block);
	if ((options.fromX2 > block.width) || (options.fromY2 > block.height)
		|| (options.fromX2 > block.width)
		|| (options.fromY2 > block.height)) {
	    Tcl_AppendResult(interp, "coordinates for -from option extend ",
		    "outside source image", (char *) NULL);
	    return TCL_ERROR;
	}

	/*
	 * Fill in default values for unspecified parameters.
	 */

	if (((options.options & OPT_FROM) == 0) || (options.fromX2 < 0)) {
	    options.fromX2 = block.width;
	    options.fromY2 = block.height;
	}
	if (((options.options & OPT_TO) == 0) || (options.toX2 < 0)) {
	    width = options.fromX2 - options.fromX;
	    if (options.subsampleX > 0) {
		width = (width + options.subsampleX - 1) / options.subsampleX;
	    } else if (options.subsampleX == 0) {
		width = 0;
	    } else {
		width = (width - options.subsampleX - 1) / -options.subsampleX;
	    }
	    options.toX2 = options.toX + width * options.zoomX;

	    height = options.fromY2 - options.fromY;
	    if (options.subsampleY > 0) {
		height = (height + options.subsampleY - 1)
			/ options.subsampleY;
	    } else if (options.subsampleY == 0) {
		height = 0;
	    } else {
		height = (height - options.subsampleY - 1)
			/ -options.subsampleY;
	    }
	    options.toY2 = options.toY + height * options.zoomY;
	}

	/*
	 * Set the destination image size if the -shrink option was specified.
	 */

	if (options.options & OPT_SHRINK) {
	    ImgPhotoSetSize(masterPtr, options.toX2, options.toY2);
	}

	/*
	 * Copy the image data over using Tk_PhotoPutZoomedBlock.
	 */

	block.pixelPtr += options.fromX * block.pixelSize
	    + options.fromY * block.pitch;
	block.width = options.fromX2 - options.fromX;
	block.height = options.fromY2 - options.fromY;
	Tk_PhotoPutZoomedBlock((Tk_PhotoHandle) masterPtr, &block,
		options.toX, options.toY, options.toX2 - options.toX,
		options.toY2 - options.toY, options.zoomX, options.zoomY,
		options.subsampleX, options.subsampleY);

    } else if ((c == 'g') && (strncmp(argv[1], "get", length) == 0)) {
	/*
	 * photo get command - first parse and check parameters.
	 */

	if (argc != 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		    " get x y\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if ((Tcl_GetInt(interp, argv[2], &x) != TCL_OK)
		|| (Tcl_GetInt(interp, argv[3], &y) != TCL_OK)) {
	    return TCL_ERROR;
	}
	if ((x < 0) || (x >= masterPtr->width)
		|| (y < 0) || (y >= masterPtr->height)) {
	    Tcl_AppendResult(interp, argv[0], " get: ",
		    "coordinates out of range", (char *) NULL);
	    return TCL_ERROR;
	}

	/*
	 * Extract the value of the desired pixel and format it as a string.
	 */

	pixelPtr = masterPtr->pix24 + (y * masterPtr->width + x) * 3;
	sprintf(string, "%d %d %d", pixelPtr[0], pixelPtr[1],
		pixelPtr[2]);
	Tcl_AppendResult(interp, string, (char *) NULL);
    } else if ((c == 'p') && (strncmp(argv[1], "put", length) == 0)) {
	/*
	 * photo put command - first parse the options and colors specified.
	 */

	index = 2;
	memset((VOID *) &options, 0, sizeof(options));
	options.name = NULL;
	if (ParseSubcommandOptions(&options, interp, OPT_TO,
	       &index, argc, argv) != TCL_OK) {
	    return TCL_ERROR;
	}
	if ((options.name == NULL) || (index < argc)) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		     " put {{colors...}...} ?-to x1 y1 x2 y2?\"",
		     (char *) NULL);
	    return TCL_ERROR;
	}
	if (Tcl_SplitList(interp, options.name, &dataHeight, &srcArgv)
		!= TCL_OK) {
	    return TCL_ERROR;
	}
	tkwin = Tk_MainWindow(interp);
	block.pixelPtr = NULL;
	dataWidth = 0;
	pixelPtr = NULL;
	for (y = 0; y < dataHeight; ++y) {
	    if (Tcl_SplitList(interp, srcArgv[y], &listArgc, &listArgv)
		    != TCL_OK) {
		break;
	    }
	    if (y == 0) {
		dataWidth = listArgc;
		pixelPtr = (unsigned char *) ckalloc((unsigned)
			dataWidth * dataHeight * 3);
		block.pixelPtr = pixelPtr;
	    } else {
		if (listArgc != dataWidth) {
		    Tcl_AppendResult(interp, "all elements of color list must",
			     " have the same number of elements",
			    (char *) NULL);
		    ckfree((char *) listArgv);
		    break;
		}
	    }
	    for (x = 0; x < dataWidth; ++x) {
		if (!XParseColor(Tk_Display(tkwin), Tk_Colormap(tkwin),
			listArgv[x], &color)) {
		    Tcl_AppendResult(interp, "can't parse color \"",
			    listArgv[x], "\"", (char *) NULL);
		    break;
		}
		*pixelPtr++ = color.red >> 8;
		*pixelPtr++ = color.green >> 8;
		*pixelPtr++ = color.blue >> 8;
	    }
	    ckfree((char *) listArgv);
	    if (x < dataWidth)
		break;
	}
	ckfree((char *) srcArgv);
	if (y < dataHeight || dataHeight == 0 || dataWidth == 0) {
	    if (block.pixelPtr != NULL) {
		ckfree((char *) block.pixelPtr);
	    }
	    if (y < dataHeight) {
		return TCL_ERROR;
	    }
	    return TCL_OK;
	}

	/*
	 * Fill in default values for the -to option, then
	 * copy the block in using Tk_PhotoPutBlock.
	 */

	if (((options.options & OPT_TO) == 0) || (options.toX2 < 0)) {
	    options.toX2 = options.toX + dataWidth;
	    options.toY2 = options.toY + dataHeight;
	}
	block.width = dataWidth;
	block.height = dataHeight;
	block.pitch = dataWidth * 3;
	block.pixelSize = 3;
	block.offset[0] = 0;
	block.offset[1] = 1;
	block.offset[2] = 2;
	Tk_PhotoPutBlock((ClientData)masterPtr, &block,
		options.toX, options.toY, options.toX2 - options.toX,
		options.toY2 - options.toY);
	ckfree((char *) block.pixelPtr);
    } else if ((c == 'r') && (length >= 3)
	       && (strncmp(argv[1], "read", length) == 0)) {
	/*
	 * photo read command - first parse the options specified.
	 */

	index = 2;
	memset((VOID *) &options, 0, sizeof(options));
	options.name = NULL;
	options.format = NULL;
	if (ParseSubcommandOptions(&options, interp,
		OPT_FORMAT | OPT_FROM | OPT_TO | OPT_SHRINK,
		&index, argc, argv) != TCL_OK) {
	    return TCL_ERROR;
	}
	if ((options.name == NULL) || (index < argc)) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		    " read fileName ?-format format-name?",
		    " ?-from x1 y1 x2 y2? ?-to x y? ?-shrink?\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}

	/*
	 * Open the image file and look for a handler for it.
	 */

	realFileName = Tcl_TranslateFileName(interp, options.name, &buffer);
	if (realFileName == NULL) {
	    return TCL_ERROR;
	}
	f = fopen(realFileName, "rb");
	Tcl_DStringFree(&buffer);
	if (f == NULL) {
	    Tcl_AppendResult(interp, "couldn't read image file \"",
		    options.name, "\": ", Tcl_PosixError(interp),
		    (char *) NULL);
	    return TCL_ERROR;
	}
	if (MatchFileFormat(interp, f, options.name, options.format,
		&imageFormat, &imageWidth, &imageHeight) != TCL_OK) {
	    fclose(f);
	    return TCL_ERROR;
	}

	/*
	 * Check the values given for the -from option.
	 */

	if ((options.fromX > imageWidth) || (options.fromY > imageHeight)
		|| (options.fromX2 > imageWidth)
		|| (options.fromY2 > imageHeight)) {
	    Tcl_AppendResult(interp, "coordinates for -from option extend ",
		    "outside source image", (char *) NULL);
	    fclose(f);
	    return TCL_ERROR;
	}
	if (((options.options & OPT_FROM) == 0) || (options.fromX2 < 0)) {
	    width = imageWidth - options.fromX;
	    height = imageHeight - options.fromY;
	} else {
	    width = options.fromX2 - options.fromX;
	    height = options.fromY2 - options.fromY;
	}

	/*
	 * If the -shrink option was specified, set the size of the image.
	 */

	if (options.options & OPT_SHRINK) {
	    ImgPhotoSetSize(masterPtr, options.toX + width,
		    options.toY + height);
	}

	/*
	 * Call the handler's file read procedure to read the data
	 * into the image.
	 */

	result = (*imageFormat->fileReadProc)(interp, f, options.name,
		options.format, (Tk_PhotoHandle) masterPtr, options.toX,
		options.toY, width, height, options.fromX, options.fromY);
	if (f != NULL) {
	    fclose(f);
	}
	return result;
    } else if ((c == 'r') && (length >= 3)
	       && (strncmp(argv[1], "redither", length) == 0)) {

	if (argc == 2) {
	    /*
	     * Call Dither if any part of the image is not correctly
	     * dithered at present.
	     */

	    x = masterPtr->ditherX;
	    y = masterPtr->ditherY;
	    if (masterPtr->ditherX != 0) {
		Dither(masterPtr, x, y, masterPtr->width - x, 1);
	    }
	    if (masterPtr->ditherY < masterPtr->height) {
		x = 0;
		Dither(masterPtr, 0, masterPtr->ditherY, masterPtr->width,
			masterPtr->height - masterPtr->ditherY);
	    }

	    if (y < masterPtr->height) {
		/*
		 * Tell the core image code that part of the image has changed.
		 */

		Tk_ImageChanged(masterPtr->tkMaster, x, y,
			(masterPtr->width - x), (masterPtr->height - y),
			masterPtr->width, masterPtr->height);
	    }

	} else {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		    " redither\"", (char *) NULL);
	    return TCL_ERROR;
	}
    } else if ((c == 'w') && (strncmp(argv[1], "write", length) == 0)) {
	/*
	 * photo write command - first parse and check any options given.
	 */

	index = 2;
	memset((VOID *) &options, 0, sizeof(options));
	options.name = NULL;
	options.format = NULL;
	if (ParseSubcommandOptions(&options, interp, OPT_FORMAT | OPT_FROM,
		&index, argc, argv) != TCL_OK) {
	    return TCL_ERROR;
	}
	if ((options.name == NULL) || (index < argc)) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		    " write fileName ?-format format-name?",
		    "?-from x1 y1 x2 y2?\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if ((options.fromX > masterPtr->width)
		|| (options.fromY > masterPtr->height)
		|| (options.fromX2 > masterPtr->width)
		|| (options.fromY2 > masterPtr->height)) {
	    Tcl_AppendResult(interp, "coordinates for -from option extend ",
		    "outside image", (char *) NULL);
	    return TCL_ERROR;
	}

	/*
	 * Fill in default values for unspecified parameters.
	 */

	if (((options.options & OPT_FROM) == 0) || (options.fromX2 < 0)) {
	    options.fromX2 = masterPtr->width;
	    options.fromY2 = masterPtr->height;
	}

	/*
	 * Search for an appropriate image file format handler,
	 * and give an error if none is found.
	 */

	matched = 0;
	for (imageFormat = formatList; imageFormat != NULL;
	     imageFormat = imageFormat->nextPtr) {
	    if ((options.format == NULL)
		    || (strncasecmp(options.format, imageFormat->name,
		    strlen(imageFormat->name)) == 0)) {
		matched = 1;
		if (imageFormat->fileWriteProc != NULL) {
		    break;
		}
	    }
	}
	if (imageFormat == NULL) {
	    if (options.format == NULL) {
		Tcl_AppendResult(interp, "no available image file format ",
			"has file writing capability", (char *) NULL);
	    } else if (!matched) {
		Tcl_AppendResult(interp, "image file format \"",
			options.format, "\" is unknown", (char *) NULL);
	    } else {
		Tcl_AppendResult(interp, "image file format \"",
			options.format, "\" has no file writing capability",
			(char *) NULL);
	    }
	    return TCL_ERROR;
	}

	/*
	 * Call the handler's file write procedure to write out
	 * the image.
	 */

	Tk_PhotoGetImage((Tk_PhotoHandle) masterPtr, &block);
	block.pixelPtr += options.fromY * block.pitch + options.fromX * 3;
	block.width = options.fromX2 - options.fromX;
	block.height = options.fromY2 - options.fromY;
	return (*imageFormat->fileWriteProc)(interp, options.name,
		options.format, &block);
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\": must be blank, cget, configure, copy, get, put,",
		" read, redither, or write", (char *) NULL);
	return TCL_ERROR;
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ParseSubcommandOptions --
 *
 *	This procedure is invoked to process one of the options
 *	which may be specified for the photo image subcommands,
 *	namely, -from, -to, -zoom, -subsample, -format, and -shrink.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Fields in *optPtr get filled in.
 *
 *----------------------------------------------------------------------
 */

static int
ParseSubcommandOptions(optPtr, interp, allowedOptions, optIndexPtr, argc, argv)
    struct SubcommandOptions *optPtr;
				/* Information about the options specified
				 * and the values given is returned here. */
    Tcl_Interp *interp;		/* Interpreter to use for reporting errors. */
    int allowedOptions;		/* Indicates which options are valid for
				 * the current command. */
    int *optIndexPtr;		/* Points to a variable containing the
				 * current index in argv; this variable is
				 * updated by this procedure. */
    int argc;			/* Number of arguments in argv[]. */
    char **argv;		/* Arguments to be parsed. */
{
    int index, c, bit, currentBit;
    size_t length;
    char *option, **listPtr;
    int values[4];
    int numValues, maxValues, argIndex;

    for (index = *optIndexPtr; index < argc; *optIndexPtr = ++index) {
	/*
	 * We can have one value specified without an option;
	 * it goes into optPtr->name.
	 */

	option = argv[index];
	if (option[0] != '-') {
	    if (optPtr->name == NULL) {
		optPtr->name = option;
		continue;
	    }
	    break;
	}

	/*
	 * Work out which option this is.
	 */

	length = strlen(option);
	c = option[0];
	bit = 0;
	currentBit = 1;
	for (listPtr = optionNames; *listPtr != NULL; ++listPtr) {
	    if ((c == *listPtr[0])
		    && (strncmp(option, *listPtr, length) == 0)) {
		if (bit != 0) {
		    bit = 0;	/* An ambiguous option. */
		    break;
		}
		bit = currentBit;
	    }
	    currentBit <<= 1;
	}

	/*
	 * If this option is not recognized and allowed, put
	 * an error message in the interpreter and return.
	 */

	if ((allowedOptions & bit) == 0) {
	    Tcl_AppendResult(interp, "unrecognized option \"", argv[index],
		    "\": must be ", (char *)NULL);
	    bit = 1;
	    for (listPtr = optionNames; *listPtr != NULL; ++listPtr) {
		if ((allowedOptions & bit) != 0) {
		    if ((allowedOptions & (bit - 1)) != 0) {
			Tcl_AppendResult(interp, ", ", (char *) NULL);
			if ((allowedOptions & ~((bit << 1) - 1)) == 0) {
			    Tcl_AppendResult(interp, "or ", (char *) NULL);
			}
		    }
		    Tcl_AppendResult(interp, *listPtr, (char *) NULL);
		}
		bit <<= 1;
	    }
	    return TCL_ERROR;
	}

	/*
	 * For the -from, -to, -zoom and -subsample options,
	 * parse the values given.  Report an error if too few
	 * or too many values are given.
	 */

	if ((bit != OPT_SHRINK) && (bit != OPT_FORMAT)) {
	    maxValues = ((bit == OPT_FROM) || (bit == OPT_TO))? 4: 2;
	    argIndex = index + 1;
	    for (numValues = 0; numValues < maxValues; ++numValues) {
		if ((argIndex < argc) && (isdigit(UCHAR(argv[argIndex][0]))
			|| ((argv[argIndex][0] == '-')
			&& (isdigit(UCHAR(argv[argIndex][1])))))) {
		    if (Tcl_GetInt(interp, argv[argIndex], &values[numValues])
			    != TCL_OK) {
			return TCL_ERROR;
		    }
		} else {
		    break;
		}
		++argIndex;
	    }

	    if (numValues == 0) {
		Tcl_AppendResult(interp, "the \"", argv[index], "\" option ",
			 "requires one ", maxValues == 2? "or two": "to four",
			 " integer values", (char *) NULL);
		return TCL_ERROR;
	    }
	    *optIndexPtr = (index += numValues);

	    /*
	     * Y values default to the corresponding X value if not specified.
	     */

	    if (numValues == 1) {
		values[1] = values[0];
	    }
	    if (numValues == 3) {
		values[3] = values[2];
	    }

	    /*
	     * Check the values given and put them in the appropriate
	     * field of the SubcommandOptions structure.
	     */

	    switch (bit) {
		case OPT_FROM:
		    if ((values[0] < 0) || (values[1] < 0) || ((numValues > 2)
			    && ((values[2] < 0) || (values[3] < 0)))) {
			Tcl_AppendResult(interp, "value(s) for the -from",
				" option must be non-negative", (char *) NULL);
			return TCL_ERROR;
		    }
		    if (numValues <= 2) {
			optPtr->fromX = values[0];
			optPtr->fromY = values[1];
			optPtr->fromX2 = -1;
			optPtr->fromY2 = -1;
		    } else {
			optPtr->fromX = MIN(values[0], values[2]);
			optPtr->fromY = MIN(values[1], values[3]);
			optPtr->fromX2 = MAX(values[0], values[2]);
			optPtr->fromY2 = MAX(values[1], values[3]);
		    }
		    break;
		case OPT_SUBSAMPLE:
		    optPtr->subsampleX = values[0];
		    optPtr->subsampleY = values[1];
		    break;
		case OPT_TO:
		    if ((values[0] < 0) || (values[1] < 0) || ((numValues > 2)
			    && ((values[2] < 0) || (values[3] < 0)))) {
			Tcl_AppendResult(interp, "value(s) for the -to",
				" option must be non-negative", (char *) NULL);
			return TCL_ERROR;
		    }
		    if (numValues <= 2) {
			optPtr->toX = values[0];
			optPtr->toY = values[1];
			optPtr->toX2 = -1;
			optPtr->toY2 = -1;
		    } else {
			optPtr->toX = MIN(values[0], values[2]);
			optPtr->toY = MIN(values[1], values[3]);
			optPtr->toX2 = MAX(values[0], values[2]);
			optPtr->toY2 = MAX(values[1], values[3]);
		    }
		    break;
		case OPT_ZOOM:
		    if ((values[0] <= 0) || (values[1] <= 0)) {
			Tcl_AppendResult(interp, "value(s) for the -zoom",
				" option must be positive", (char *) NULL);
			return TCL_ERROR;
		    }
		    optPtr->zoomX = values[0];
		    optPtr->zoomY = values[1];
		    break;
	    }
	} else if (bit == OPT_FORMAT) {
	    /*
	     * The -format option takes a single string value.
	     */

	    if (index + 1 < argc) {
		*optIndexPtr = ++index;
		optPtr->format = argv[index];
	    } else {
		Tcl_AppendResult(interp, "the \"-format\" option ",
			"requires a value", (char *) NULL);
		return TCL_ERROR;
	    }
	}

	/*
	 * Remember that we saw this option.
	 */

	optPtr->options |= bit;
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ImgPhotoConfigureMaster --
 *
 *	This procedure is called when a photo image is created or
 *	reconfigured.  It processes configuration options and resets
 *	any instances of the image.
 *
 * Results:
 *	A standard Tcl return value.  If TCL_ERROR is returned then
 *	an error message is left in masterPtr->interp->result.
 *
 * Side effects:
 *	Existing instances of the image will be redisplayed to match
 *	the new configuration options.
 *
 *----------------------------------------------------------------------
 */

static int
ImgPhotoConfigureMaster(interp, masterPtr, argc, argv, flags)
    Tcl_Interp *interp;		/* Interpreter to use for reporting errors. */
    PhotoMaster *masterPtr;	/* Pointer to data structure describing
				 * overall photo image to (re)configure. */
    int argc;			/* Number of entries in argv. */
    char **argv;		/* Pairs of configuration options for image. */
    int flags;			/* Flags to pass to Tk_ConfigureWidget,
				 * such as TK_CONFIG_ARGV_ONLY. */
{
    PhotoInstance *instancePtr;
    char *oldFileString, *oldDataString, *realFileName, *oldPaletteString;
    double oldGamma;
    int result;
    FILE *f;
    Tk_PhotoImageFormat *imageFormat;
    int imageWidth, imageHeight;
    Tcl_DString buffer;

    /*
     * Save the current values for fileString and dataString, so we
     * can tell if the user specifies them anew.
     */

    oldFileString = masterPtr->fileString;
    oldDataString = (oldFileString == NULL)? masterPtr->dataString: NULL;
    oldPaletteString = masterPtr->palette;
    oldGamma = masterPtr->gamma;

    /*
     * Process the configuration options specified.
     */

    if (Tk_ConfigureWidget(interp, Tk_MainWindow(interp), configSpecs,
	    argc, argv, (char *) masterPtr, flags) != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * Regard the empty string for -file, -data or -format as the null
     * value.
     */

    if ((masterPtr->fileString != NULL) && (masterPtr->fileString[0] == 0)) {
	ckfree(masterPtr->fileString);
	masterPtr->fileString = NULL;
    }
    if ((masterPtr->dataString != NULL) && (masterPtr->dataString[0] == 0)) {
	ckfree(masterPtr->dataString);
	masterPtr->dataString = NULL;
    }
    if ((masterPtr->format != NULL) && (masterPtr->format[0] == 0)) {
	ckfree(masterPtr->format);
	masterPtr->format = NULL;
    }

    /*
     * Set the image to the user-requested size, if any,
     * and make sure storage is correctly allocated for this image.
     */

    ImgPhotoSetSize(masterPtr, masterPtr->width, masterPtr->height);

    /*
     * Read in the image from the file or string if the user has
     * specified the -file or -data option.
     */

    if ((masterPtr->fileString != NULL)
	    && (masterPtr->fileString != oldFileString)) {

	realFileName = Tcl_TranslateFileName(interp, masterPtr->fileString,
		&buffer);
	if (realFileName == NULL) {
	    return TCL_ERROR;
	}
	f = fopen(realFileName, "rb");
	Tcl_DStringFree(&buffer);
	if (f == NULL) {
	    Tcl_AppendResult(interp, "couldn't read image file \"",
		    masterPtr->fileString, "\": ", Tcl_PosixError(interp),
		    (char *) NULL);
	    return TCL_ERROR;
	}
	if (MatchFileFormat(interp, f, masterPtr->fileString,
		masterPtr->format, &imageFormat, &imageWidth,
		&imageHeight) != TCL_OK) {
	    fclose(f);
	    return TCL_ERROR;
	}
	ImgPhotoSetSize(masterPtr, imageWidth, imageHeight);
	result = (*imageFormat->fileReadProc)(interp, f, masterPtr->fileString,
		masterPtr->format, (Tk_PhotoHandle) masterPtr, 0, 0,
		imageWidth, imageHeight, 0, 0);
	fclose(f);
	if (result != TCL_OK) {
	    return TCL_ERROR;
	}

	masterPtr->flags |= IMAGE_CHANGED;
    }

    if ((masterPtr->fileString == NULL) && (masterPtr->dataString != NULL)
	    && (masterPtr->dataString != oldDataString)) {

	if (MatchStringFormat(interp, masterPtr->dataString, 
		masterPtr->format, &imageFormat, &imageWidth,
		&imageHeight) != TCL_OK) {
	    return TCL_ERROR;
	}
	ImgPhotoSetSize(masterPtr, imageWidth, imageHeight);
	if ((*imageFormat->stringReadProc)(interp, masterPtr->dataString,
		masterPtr->format, (Tk_PhotoHandle) masterPtr,
		0, 0, imageWidth, imageHeight, 0, 0) != TCL_OK) {
	    return TCL_ERROR;
	}

	masterPtr->flags |= IMAGE_CHANGED;
    }

    /*
     * Enforce a reasonable value for gamma.
     */

    if (masterPtr->gamma <= 0) {
	masterPtr->gamma = 1.0;
    }

    if ((masterPtr->gamma != oldGamma)
	    || (masterPtr->palette != oldPaletteString)) {
	masterPtr->flags |= IMAGE_CHANGED;
    }

    /*
     * Cycle through all of the instances of this image, regenerating
     * the information for each instance.  Then force the image to be
     * redisplayed everywhere that it is used.
     */

    for (instancePtr = masterPtr->instancePtr; instancePtr != NULL;
	    instancePtr = instancePtr->nextPtr) {
	ImgPhotoConfigureInstance(instancePtr);
    }

    /*
     * Inform the generic image code that the image
     * has (potentially) changed.
     */

    Tk_ImageChanged(masterPtr->tkMaster, 0, 0, masterPtr->width,
	    masterPtr->height, masterPtr->width, masterPtr->height);
    masterPtr->flags &= ~IMAGE_CHANGED;

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ImgPhotoConfigureInstance --
 *
 *	This procedure is called to create displaying information for
 *	a photo image instance based on the configuration information
 *	in the master.  It is invoked both when new instances are
 *	created and when the master is reconfigured.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Generates errors via Tcl_BackgroundError if there are problems
 *	in setting up the instance.
 *
 *----------------------------------------------------------------------
 */

static void
ImgPhotoConfigureInstance(instancePtr)
    PhotoInstance *instancePtr;	/* Instance to reconfigure. */
{
    PhotoMaster *masterPtr = instancePtr->masterPtr;
    XImage *imagePtr;
    int bitsPerPixel;
    ColorTable *colorTablePtr;
    XRectangle validBox;

    /*
     * If the -palette configuration option has been set for the master,
     * use the value specified for our palette, but only if it is
     * a valid palette for our windows.  Use the gamma value specified
     * the master.
     */

    if ((masterPtr->palette && masterPtr->palette[0])
	    && IsValidPalette(instancePtr, masterPtr->palette)) {
	instancePtr->palette = masterPtr->palette;
    } else {
	instancePtr->palette = instancePtr->defaultPalette;
    }
    instancePtr->gamma = masterPtr->gamma;

    /*
     * If we don't currently have a color table, or if the one we
     * have no longer applies (e.g. because our palette or gamma
     * has changed), get a new one.
     */

    colorTablePtr = instancePtr->colorTablePtr;
    if ((colorTablePtr == NULL)
	    || (instancePtr->colormap != colorTablePtr->id.colormap)
	    || (instancePtr->palette != colorTablePtr->id.palette)
	    || (instancePtr->gamma != colorTablePtr->id.gamma)) {
	/*
	 * Free up our old color table, and get a new one.
	 */

	if (colorTablePtr != NULL) {
	    colorTablePtr->liveRefCount -= 1;
	    FreeColorTable(colorTablePtr);
	}
	GetColorTable(instancePtr);

	/*
	 * Create a new XImage structure for sending data to
	 * the X server, if necessary.
	 */

	if (instancePtr->colorTablePtr->flags & BLACK_AND_WHITE) {
	    bitsPerPixel = 1;
	} else {
	    bitsPerPixel = instancePtr->visualInfo.depth;
	}

	if ((instancePtr->imagePtr == NULL)
		|| (instancePtr->imagePtr->bits_per_pixel != bitsPerPixel)) {
	    if (instancePtr->imagePtr != NULL) {
		XFree((char *) instancePtr->imagePtr);
	    }
	    imagePtr = XCreateImage(instancePtr->display,
		    instancePtr->visualInfo.visual, (unsigned) bitsPerPixel,
		    (bitsPerPixel > 1? ZPixmap: XYBitmap), 0, (char *) NULL,
		    1, 1, 32, 0);
	    instancePtr->imagePtr = imagePtr;

	    /*
	     * Determine the endianness of this machine.
	     * We create images using the local host's endianness, rather
	     * than the endianness of the server; otherwise we would have
	     * to byte-swap any 16 or 32 bit values that we store in the
	     * image in those situations where the server's endianness
	     * is different from ours.
	     */

	    if (imagePtr != NULL) {
		union {
		    int i;
		    char c[sizeof(int)];
		} kludge;

		imagePtr->bitmap_unit = sizeof(pixel) * NBBY;
		kludge.i = 0;
		kludge.c[0] = 1;
		imagePtr->byte_order = (kludge.i == 1) ? LSBFirst : MSBFirst;
		_XInitImageFuncPtrs(imagePtr);
	    }
	}
    }

    /*
     * If the user has specified a width and/or height for the master
     * which is different from our current width/height, set the size
     * to the values specified by the user.  If we have no pixmap, we
     * do this also, since it has the side effect of allocating a
     * pixmap for us.
     */

    if ((instancePtr->pixels == None) || (instancePtr->error == NULL)
	    || (instancePtr->width != masterPtr->width)
	    || (instancePtr->height != masterPtr->height)) {
	ImgPhotoInstanceSetSize(instancePtr);
    }

    /*
     * Redither this instance if necessary.
     */

    if ((masterPtr->flags & IMAGE_CHANGED)
	    || (instancePtr->colorTablePtr != colorTablePtr)) {
	TkClipBox(masterPtr->validRegion, &validBox);
	if ((validBox.width > 0) && (validBox.height > 0)) {
	    DitherInstance(instancePtr, validBox.x, validBox.y,
		    validBox.width, validBox.height);
	}
    }

}

/*
 *----------------------------------------------------------------------
 *
 * ImgPhotoGet --
 *
 *	This procedure is called for each use of a photo image in a
 *	widget.
 *
 * Results:
 *	The return value is a token for the instance, which is passed
 *	back to us in calls to ImgPhotoDisplay and ImgPhotoFree.
 *
 * Side effects:
 *	A data structure is set up for the instance (or, an existing
 *	instance is re-used for the new one).
 *
 *----------------------------------------------------------------------
 */

static ClientData
ImgPhotoGet(tkwin, masterData)
    Tk_Window tkwin;		/* Window in which the instance will be
				 * used. */
    ClientData masterData;	/* Pointer to our master structure for the
				 * image. */
{
    PhotoMaster *masterPtr = (PhotoMaster *) masterData;
    PhotoInstance *instancePtr;
    Colormap colormap;
    int mono, nRed, nGreen, nBlue;
    XVisualInfo visualInfo, *visInfoPtr;
    XRectangle validBox;
    char buf[16];
    int numVisuals;
    XColor *white, *black;
    XGCValues gcValues;

    /*
     * Table of "best" choices for palette for PseudoColor displays
     * with between 3 and 15 bits/pixel.
     */

    static int paletteChoice[13][3] = {
	/*  #red, #green, #blue */
	 {2,  2,  2,			/* 3 bits, 8 colors */},
	 {2,  3,  2,			/* 4 bits, 12 colors */},
	 {3,  4,  2,			/* 5 bits, 24 colors */},
	 {4,  5,  3,			/* 6 bits, 60 colors */},
	 {5,  6,  4,			/* 7 bits, 120 colors */},
	 {7,  7,  4,			/* 8 bits, 198 colors */},
	 {8, 10,  6,			/* 9 bits, 480 colors */},
	{10, 12,  8,			/* 10 bits, 960 colors */},
	{14, 15,  9,			/* 11 bits, 1890 colors */},
	{16, 20, 12,			/* 12 bits, 3840 colors */},
	{20, 24, 16,			/* 13 bits, 7680 colors */},
	{26, 30, 20,			/* 14 bits, 15600 colors */},
	{32, 32, 30,			/* 15 bits, 30720 colors */}
    };

    /*
     * See if there is already an instance for windows using
     * the same colormap.  If so then just re-use it.
     */

    colormap = Tk_Colormap(tkwin);
    for (instancePtr = masterPtr->instancePtr; instancePtr != NULL;
	    instancePtr = instancePtr->nextPtr) {
	if ((colormap == instancePtr->colormap)
		&& (Tk_Display(tkwin) == instancePtr->display)) {

	    /*
	     * Re-use this instance.
	     */

	    if (instancePtr->refCount == 0) {
		/*
		 * We are resurrecting this instance.
		 */

		Tcl_CancelIdleCall(DisposeInstance, (ClientData) instancePtr);
		if (instancePtr->colorTablePtr != NULL) {
		    FreeColorTable(instancePtr->colorTablePtr);
		}
		GetColorTable(instancePtr);
	    }
	    instancePtr->refCount++;
	    return (ClientData) instancePtr;
	}
    }

    /*
     * The image isn't already in use in a window with the same colormap.
     * Make a new instance of the image.
     */

    instancePtr = (PhotoInstance *) ckalloc(sizeof(PhotoInstance));
    instancePtr->masterPtr = masterPtr;
    instancePtr->display = Tk_Display(tkwin);
    instancePtr->colormap = Tk_Colormap(tkwin);
    Tk_PreserveColormap(instancePtr->display, instancePtr->colormap);
    instancePtr->refCount = 1;
    instancePtr->colorTablePtr = NULL;
    instancePtr->pixels = None;
    instancePtr->error = NULL;
    instancePtr->width = 0;
    instancePtr->height = 0;
    instancePtr->imagePtr = 0;
    instancePtr->nextPtr = masterPtr->instancePtr;
    masterPtr->instancePtr = instancePtr;

    /*
     * Obtain information about the visual and decide on the
     * default palette.
     */

    visualInfo.screen = Tk_ScreenNumber(tkwin);
    visualInfo.visualid = XVisualIDFromVisual(Tk_Visual(tkwin));
    visInfoPtr = XGetVisualInfo(Tk_Display(tkwin),
	    VisualScreenMask | VisualIDMask, &visualInfo, &numVisuals);
    nRed = 2;
    nGreen = nBlue = 0;
    mono = 1;
    if (visInfoPtr != NULL) {
	instancePtr->visualInfo = *visInfoPtr;
	switch (visInfoPtr->class) {
	    case DirectColor:
	    case TrueColor:
		nRed = 1 << CountBits(visInfoPtr->red_mask);
		nGreen = 1 << CountBits(visInfoPtr->green_mask);
		nBlue = 1 << CountBits(visInfoPtr->blue_mask);
		mono = 0;
		break;
	    case PseudoColor:
	    case StaticColor:
		if (visInfoPtr->depth > 15) {
		    nRed = 32;
		    nGreen = 32;
		    nBlue = 32;
		    mono = 0;
		} else if (visInfoPtr->depth >= 3) {
		    int *ip = paletteChoice[visInfoPtr->depth - 3];
    
		    nRed = ip[0];
		    nGreen = ip[1];
		    nBlue = ip[2];
		    mono = 0;
		}
		break;
	    case GrayScale:
	    case StaticGray:
		nRed = 1 << visInfoPtr->depth;
		break;
	}
	XFree((char *) visInfoPtr);

    } else {
	panic("ImgPhotoGet couldn't find visual for window");
    }

    sprintf(buf, ((mono) ? "%d": "%d/%d/%d"), nRed, nGreen, nBlue);
    instancePtr->defaultPalette = Tk_GetUid(buf);

    /*
     * Make a GC with background = black and foreground = white.
     */

    white = Tk_GetColor(masterPtr->interp, tkwin, "white");
    black = Tk_GetColor(masterPtr->interp, tkwin, "black");
    gcValues.foreground = (white != NULL)? white->pixel:
	    WhitePixelOfScreen(Tk_Screen(tkwin));
    gcValues.background = (black != NULL)? black->pixel:
	    BlackPixelOfScreen(Tk_Screen(tkwin));
    gcValues.graphics_exposures = False;
    instancePtr->gc = Tk_GetGC(tkwin,
	    GCForeground|GCBackground|GCGraphicsExposures, &gcValues);

    /*
     * Set configuration options and finish the initialization of the instance.
     */

    ImgPhotoConfigureInstance(instancePtr);

    /*
     * If this is the first instance, must set the size of the image.
     */

    if (instancePtr->nextPtr == NULL) {
	Tk_ImageChanged(masterPtr->tkMaster, 0, 0, 0, 0,
		masterPtr->width, masterPtr->height);
    }

    /*
     * Dither the image to fill in this instance's pixmap.
     */

    TkClipBox(masterPtr->validRegion, &validBox);
    if ((validBox.width > 0) && (validBox.height > 0)) {
	DitherInstance(instancePtr, validBox.x, validBox.y, validBox.width,
		validBox.height);
    }

    return (ClientData) instancePtr;
}

/*
 *----------------------------------------------------------------------
 *
 * ImgPhotoDisplay --
 *
 *	This procedure is invoked to draw a photo image.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A portion of the image gets rendered in a pixmap or window.
 *
 *----------------------------------------------------------------------
 */

static void
ImgPhotoDisplay(clientData, display, drawable, imageX, imageY, width,
	height, drawableX, drawableY)
    ClientData clientData;	/* Pointer to PhotoInstance structure for
				 * for instance to be displayed. */
    Display *display;		/* Display on which to draw image. */
    Drawable drawable;		/* Pixmap or window in which to draw image. */
    int imageX, imageY;		/* Upper-left corner of region within image
				 * to draw. */
    int width, height;		/* Dimensions of region within image to draw. */
    int drawableX, drawableY;	/* Coordinates within drawable that
				 * correspond to imageX and imageY. */
{
    PhotoInstance *instancePtr = (PhotoInstance *) clientData;

    /*
     * If there's no pixmap, it means that an error occurred
     * while creating the image instance so it can't be displayed.
     */

    if (instancePtr->pixels == None) {
	return;
    }

    /*
     * masterPtr->region describes which parts of the image contain
     * valid data.  We set this region as the clip mask for the gc,
     * setting its origin appropriately, and use it when drawing the
     * image.
     */

    TkSetRegion(display, instancePtr->gc, instancePtr->masterPtr->validRegion);
    XSetClipOrigin(display, instancePtr->gc, drawableX - imageX,
	    drawableY - imageY);
    XCopyArea(display, instancePtr->pixels, drawable, instancePtr->gc,
	    imageX, imageY, (unsigned) width, (unsigned) height,
	    drawableX, drawableY);
    XSetClipMask(display, instancePtr->gc, None);
    XSetClipOrigin(display, instancePtr->gc, 0, 0);
}

/*
 *----------------------------------------------------------------------
 *
 * ImgPhotoFree --
 *
 *	This procedure is called when a widget ceases to use a
 *	particular instance of an image.  We don't actually get
 *	rid of the instance until later because we may be about
 *	to get this instance again.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Internal data structures get cleaned up, later.
 *
 *----------------------------------------------------------------------
 */

static void
ImgPhotoFree(clientData, display)
    ClientData clientData;	/* Pointer to PhotoInstance structure for
				 * for instance to be displayed. */
    Display *display;		/* Display containing window that used image. */
{
    PhotoInstance *instancePtr = (PhotoInstance *) clientData;
    ColorTable *colorPtr;

    instancePtr->refCount -= 1;
    if (instancePtr->refCount > 0) {
	return;
    }

    /*
     * There are no more uses of the image within this widget.
     * Decrement the count of live uses of its color table, so
     * that its colors can be reclaimed if necessary, and
     * set up an idle call to free the instance structure.
     */

    colorPtr = instancePtr->colorTablePtr;
    if (colorPtr != NULL) {
	colorPtr->liveRefCount -= 1;
    }
    
    Tcl_DoWhenIdle(DisposeInstance, (ClientData) instancePtr);
}

/*
 *----------------------------------------------------------------------
 *
 * ImgPhotoDelete --
 *
 *	This procedure is called by the image code to delete the
 *	master structure for an image.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Resources associated with the image get freed.
 *
 *----------------------------------------------------------------------
 */

static void
ImgPhotoDelete(masterData)
    ClientData masterData;	/* Pointer to PhotoMaster structure for
				 * image.  Must not have any more instances. */
{
    PhotoMaster *masterPtr = (PhotoMaster *) masterData;
    PhotoInstance *instancePtr;

    while ((instancePtr = masterPtr->instancePtr) != NULL) {
	if (instancePtr->refCount > 0) {
	    panic("tried to delete photo image when instances still exist");
	}
	Tcl_CancelIdleCall(DisposeInstance, (ClientData) instancePtr);
	DisposeInstance((ClientData) instancePtr);
    }
    masterPtr->tkMaster = NULL;
    if (masterPtr->imageCmd != NULL) {
	Tcl_DeleteCommand(masterPtr->interp,
		Tcl_GetCommandName(masterPtr->interp, masterPtr->imageCmd));
    }
    if (masterPtr->pix24 != NULL) {
	ckfree((char *) masterPtr->pix24);
    }
    if (masterPtr->validRegion != NULL) {
	TkDestroyRegion(masterPtr->validRegion);
    }
    Tk_FreeOptions(configSpecs, (char *) masterPtr, (Display *) NULL, 0);
    ckfree((char *) masterPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * ImgPhotoCmdDeletedProc --
 *
 *	This procedure is invoked when the image command for an image
 *	is deleted.  It deletes the image.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The image is deleted.
 *
 *----------------------------------------------------------------------
 */

static void
ImgPhotoCmdDeletedProc(clientData)
    ClientData clientData;	/* Pointer to PhotoMaster structure for
				 * image. */
{
    PhotoMaster *masterPtr = (PhotoMaster *) clientData;

    masterPtr->imageCmd = NULL;
    if (masterPtr->tkMaster != NULL) {
	Tk_DeleteImage(masterPtr->interp, Tk_NameOfImage(masterPtr->tkMaster));
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ImgPhotoSetSize --
 *
 *	This procedure reallocates the image storage and instance
 *	pixmaps for a photo image, as necessary, to change the
 *	image's size to `width' x `height' pixels.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Storage gets reallocated, for the master and all its instances.
 *
 *----------------------------------------------------------------------
 */

static void
ImgPhotoSetSize(masterPtr, width, height)
    PhotoMaster *masterPtr;
    int width, height;
{
    unsigned char *newPix24;
    int h, offset, pitch;
    unsigned char *srcPtr, *destPtr;
    XRectangle validBox, clipBox;
    TkRegion clipRegion;
    PhotoInstance *instancePtr;

    if (masterPtr->userWidth > 0) {
	width = masterPtr->userWidth;
    }
    if (masterPtr->userHeight > 0) {
	height = masterPtr->userHeight;
    }

    /*
     * We have to trim the valid region if it is currently
     * larger than the new image size.
     */

    TkClipBox(masterPtr->validRegion, &validBox);
    if ((validBox.x + validBox.width > (unsigned) width)
	    || (validBox.y + validBox.height > (unsigned) height)) {
	clipBox.x = 0;
	clipBox.y = 0;
	clipBox.width = width;
	clipBox.height = height;
	clipRegion = TkCreateRegion();
	TkUnionRectWithRegion(&clipBox, clipRegion, clipRegion);
	TkIntersectRegion(masterPtr->validRegion, clipRegion,
		masterPtr->validRegion);
	TkDestroyRegion(clipRegion);
	TkClipBox(masterPtr->validRegion, &validBox);
    }

    if ((width != masterPtr->width) || (height != masterPtr->height)
	    || (masterPtr->pix24 == NULL)) {

	/*
	 * Reallocate storage for the 24-bit image and copy
	 * over valid regions.
	 */

	pitch = width * 3;
	newPix24 = (unsigned char *) ckalloc((unsigned) (height * pitch));

	/*
	 * Zero the new array.  The dithering code shouldn't read the
	 * areas outside validBox, but they might be copied to another
	 * photo image or written to a file.
	 */

	if ((masterPtr->pix24 != NULL)
	    && ((width == masterPtr->width) || (width == validBox.width))) {
	    if (validBox.y > 0) {
		memset((VOID *) newPix24, 0, (size_t) (validBox.y * pitch));
	    }
	    h = validBox.y + validBox.height;
	    if (h < height) {
		memset((VOID *) (newPix24 + h * pitch), 0,
			(size_t) ((height - h) * pitch));
	    }
	} else {
	    memset((VOID *) newPix24, 0, (size_t) (height * pitch));
	}

	if (masterPtr->pix24 != NULL) {

	    /*
	     * Copy the common area over to the new array array and
	     * free the old array.
	     */

	    if (width == masterPtr->width) {

		/*
		 * The region to be copied is contiguous.
		 */

		offset = validBox.y * pitch;
		memcpy((VOID *) (newPix24 + offset),
			(VOID *) (masterPtr->pix24 + offset),
			(size_t) (validBox.height * pitch));

	    } else if ((validBox.width > 0) && (validBox.height > 0)) {

		/*
		 * Area to be copied is not contiguous - copy line by line.
		 */

		destPtr = newPix24 + (validBox.y * width + validBox.x) * 3;
		srcPtr = masterPtr->pix24 + (validBox.y * masterPtr->width
			+ validBox.x) * 3;
		for (h = validBox.height; h > 0; h--) {
		    memcpy((VOID *) destPtr, (VOID *) srcPtr,
			    (size_t) (validBox.width * 3));
		    destPtr += width * 3;
		    srcPtr += masterPtr->width * 3;
		}
	    }

	    ckfree((char *) masterPtr->pix24);
	}

	masterPtr->pix24 = newPix24;
	masterPtr->width = width;
	masterPtr->height = height;

	/*
	 * Dithering will be correct up to the end of the last
	 * pre-existing complete scanline.
	 */

	if ((validBox.x > 0) || (validBox.y > 0)) {
	    masterPtr->ditherX = 0;
	    masterPtr->ditherY = 0;
	} else if (validBox.width == width) {
	    if ((int) validBox.height < masterPtr->ditherY) {
		masterPtr->ditherX = 0;
		masterPtr->ditherY = validBox.height;
	    }
	} else {
	    if ((masterPtr->ditherY > 0)
		    || ((int) validBox.width < masterPtr->ditherX)) {
		masterPtr->ditherX = validBox.width;
		masterPtr->ditherY = 0;
	    }
	}
    }

    /*
     * Now adjust the sizes of the pixmaps for all of the instances.
     */

    for (instancePtr = masterPtr->instancePtr; instancePtr != NULL;
	    instancePtr = instancePtr->nextPtr) {
	ImgPhotoInstanceSetSize(instancePtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ImgPhotoInstanceSetSize --
 *
 * 	This procedure reallocates the instance pixmap and dithering
 *	error array for a photo instance, as necessary, to change the
 *	image's size to `width' x `height' pixels.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Storage gets reallocated, here and in the X server.
 *
 *----------------------------------------------------------------------
 */

static void
ImgPhotoInstanceSetSize(instancePtr)
    PhotoInstance *instancePtr;		/* Instance whose size is to be
					 * changed. */
{
    PhotoMaster *masterPtr;
    schar *newError;
    schar *errSrcPtr, *errDestPtr;
    int h, offset;
    XRectangle validBox;
    Pixmap newPixmap;

    masterPtr = instancePtr->masterPtr;
    TkClipBox(masterPtr->validRegion, &validBox);

    if ((instancePtr->width != masterPtr->width)
	    || (instancePtr->height != masterPtr->height)
	    || (instancePtr->pixels == None)) {
	newPixmap = Tk_GetPixmap(instancePtr->display,
		RootWindow(instancePtr->display,
		    instancePtr->visualInfo.screen),
		(masterPtr->width > 0) ? masterPtr->width: 1,
		(masterPtr->height > 0) ? masterPtr->height: 1,
		instancePtr->visualInfo.depth);

	/*
	 * The following is a gross hack needed to properly support colormaps
	 * under Windows.  Before the pixels can be copied to the pixmap,
	 * the relevent colormap must be associated with the drawable.
	 * Normally we can infer this association from the window that
	 * was used to create the pixmap.  However, in this case we're
	 * using the root window, so we have to be more explicit.
	 */

	TkSetPixmapColormap(newPixmap, instancePtr->colormap);

	if (instancePtr->pixels != None) {
	    /*
	     * Copy any common pixels from the old pixmap and free it.
	     */
	    XCopyArea(instancePtr->display, instancePtr->pixels, newPixmap,
		    instancePtr->gc, validBox.x, validBox.y,
		    validBox.width, validBox.height, validBox.x, validBox.y);
	    Tk_FreePixmap(instancePtr->display, instancePtr->pixels);
	}
	instancePtr->pixels = newPixmap;
    }

    if ((instancePtr->width != masterPtr->width)
	    || (instancePtr->height != masterPtr->height)
	    || (instancePtr->error == NULL)) {

	newError = (schar *) ckalloc((unsigned)
		(masterPtr->height * masterPtr->width * 3 * sizeof(schar)));

	/*
	 * Zero the new array so that we don't get bogus error values
	 * propagating into areas we dither later.
	 */

	if ((instancePtr->error != NULL)
	    && ((instancePtr->width == masterPtr->width)
		|| (validBox.width == masterPtr->width))) {
	    if (validBox.y > 0) {
		memset((VOID *) newError, 0, (size_t)
			(validBox.y * masterPtr->width * 3 * sizeof(schar)));
	    }
	    h = validBox.y + validBox.height;
	    if (h < masterPtr->height) {
		memset((VOID *) (newError + h * masterPtr->width * 3), 0,
			(size_t) ((masterPtr->height - h)
			    * masterPtr->width * 3 * sizeof(schar)));
	    }
	} else {
	    memset((VOID *) newError, 0, (size_t)
		    (masterPtr->height * masterPtr->width * 3 * sizeof(schar)));
	}

	if (instancePtr->error != NULL) {

	    /*
	     * Copy the common area over to the new array
	     * and free the old array.
	     */

	    if (masterPtr->width == instancePtr->width) {

		offset = validBox.y * masterPtr->width * 3;
		memcpy((VOID *) (newError + offset),
			(VOID *) (instancePtr->error + offset),
			(size_t) (validBox.height
			* masterPtr->width * 3 * sizeof(schar)));

	    } else if (validBox.width > 0 && validBox.height > 0) {

		errDestPtr = newError
			+ (validBox.y * masterPtr->width + validBox.x) * 3;
		errSrcPtr = instancePtr->error
			+ (validBox.y * instancePtr->width + validBox.x) * 3;
		for (h = validBox.height; h > 0; --h) {
		    memcpy((VOID *) errDestPtr, (VOID *) errSrcPtr,
			    validBox.width * 3 * sizeof(schar));
		    errDestPtr += masterPtr->width * 3;
		    errSrcPtr += instancePtr->width * 3;
		}
	    }
	    ckfree((char *) instancePtr->error);
	}

	instancePtr->error = newError;
    }

    instancePtr->width = masterPtr->width;
    instancePtr->height = masterPtr->height;
}

/*
 *----------------------------------------------------------------------
 *
 * IsValidPalette --
 *
 *	This procedure is called to check whether a value given for
 *	the -palette option is valid for a particular instance
 * 	of a photo image.
 *
 * Results:
 *	A boolean value: 1 if the palette is acceptable, 0 otherwise.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
IsValidPalette(instancePtr, palette)
    PhotoInstance *instancePtr;		/* Instance to which the palette
					 * specification is to be applied. */
    char *palette;			/* Palette specification string. */
{
    int nRed, nGreen, nBlue, mono, numColors;
    char *endp;

    /*
     * First parse the specification: it must be of the form
     * %d or %d/%d/%d.
     */

    nRed = strtol(palette, &endp, 10);
    if ((endp == palette) || ((*endp != 0) && (*endp != '/'))
	    || (nRed < 2) || (nRed > 256)) {
	return 0;
    }

    if (*endp == 0) {
	mono = 1;
	nGreen = nBlue = nRed;
    } else {
	palette = endp + 1;
	nGreen = strtol(palette, &endp, 10);
	if ((endp == palette) || (*endp != '/') || (nGreen < 2)
		|| (nGreen > 256)) {
	    return 0;
	}
	palette = endp + 1;
	nBlue = strtol(palette, &endp, 10);
	if ((endp == palette) || (*endp != 0) || (nBlue < 2)
		|| (nBlue > 256)) {
	    return 0;
	}
	mono = 0;
    }

    switch (instancePtr->visualInfo.class) {
	case DirectColor:
	case TrueColor:
	    if ((nRed > (1 << CountBits(instancePtr->visualInfo.red_mask)))
		    || (nGreen > (1
			<< CountBits(instancePtr->visualInfo.green_mask)))
		    || (nBlue > (1
			<< CountBits(instancePtr->visualInfo.blue_mask)))) {
		return 0;
	    }
	    break;
	case PseudoColor:
	case StaticColor:
	    numColors = nRed;
	    if (!mono) {
		numColors *= nGreen*nBlue;
	    }
	    if (numColors > (1 << instancePtr->visualInfo.depth)) {
		return 0;
	    }
	    break;
	case GrayScale:
	case StaticGray:
	    if (!mono || (nRed > (1 << instancePtr->visualInfo.depth))) {
		return 0;
	    }
	    break;
    }

    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * CountBits --
 *
 *	This procedure counts how many bits are set to 1 in `mask'.
 *
 * Results:
 *	The integer number of bits.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
CountBits(mask)
    pixel mask;			/* Value to count the 1 bits in. */
{
    int n;

    for( n = 0; mask != 0; mask &= mask - 1 )
	n++;
    return n;
}

/*
 *----------------------------------------------------------------------
 *
 * GetColorTable --
 *
 *	This procedure is called to allocate a table of colormap
 *	information for an instance of a photo image.  Only one such
 *	table is allocated for all photo instances using the same
 *	display, colormap, palette and gamma values, so that the
 *	application need only request a set of colors from the X
 *	server once for all such photo widgets.  This procedure
 *	maintains a hash table to find previously-allocated
 *	ColorTables.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A new ColorTable may be allocated and placed in the hash
 *	table, and have colors allocated for it.
 *
 *----------------------------------------------------------------------
 */

static void
GetColorTable(instancePtr)
    PhotoInstance *instancePtr;		/* Instance needing a color table. */
{
    ColorTable *colorPtr;
    Tcl_HashEntry *entry;
    ColorTableId id;
    int isNew;

    /*
     * Look for an existing ColorTable in the hash table.
     */

    memset((VOID *) &id, 0, sizeof(id));
    id.display = instancePtr->display;
    id.colormap = instancePtr->colormap;
    id.palette = instancePtr->palette;
    id.gamma = instancePtr->gamma;
    if (!imgPhotoColorHashInitialized) {
	Tcl_InitHashTable(&imgPhotoColorHash, N_COLOR_HASH);
	imgPhotoColorHashInitialized = 1;
    }
    entry = Tcl_CreateHashEntry(&imgPhotoColorHash, (char *) &id, &isNew);

    if (!isNew) {
	/*
	 * Re-use the existing entry.
	 */

	colorPtr = (ColorTable *) Tcl_GetHashValue(entry);

    } else {
	/*
	 * No color table currently available; need to make one.
	 */

	colorPtr = (ColorTable *) ckalloc(sizeof(ColorTable));

	/*
	 * The following line of code should not normally be needed due
	 * to the assignment in the following line.  However, it compensates
	 * for bugs in some compilers (HP, for example) where
	 * sizeof(ColorTable) is 24 but the assignment only copies 20 bytes,
	 * leaving 4 bytes uninitialized;  these cause problems when using
	 * the id for lookups in imgPhotoColorHash, and can result in
	 * core dumps.
	 */

	memset((VOID *) &colorPtr->id, 0, sizeof(ColorTableId));
	colorPtr->id = id;
	Tk_PreserveColormap(colorPtr->id.display, colorPtr->id.colormap);
	colorPtr->flags = 0;
	colorPtr->refCount = 0;
	colorPtr->liveRefCount = 0;
	colorPtr->numColors = 0;
	colorPtr->visualInfo = instancePtr->visualInfo;
	colorPtr->pixelMap = NULL;
	Tcl_SetHashValue(entry, colorPtr);
    }

    colorPtr->refCount++;
    colorPtr->liveRefCount++;
    instancePtr->colorTablePtr = colorPtr;
    if (colorPtr->flags & DISPOSE_PENDING) {
	Tcl_CancelIdleCall(DisposeColorTable, (ClientData) colorPtr);
	colorPtr->flags &= ~DISPOSE_PENDING;
    }

    /*
     * Allocate colors for this color table if necessary.
     */

    if ((colorPtr->numColors == 0)
	    && ((colorPtr->flags & BLACK_AND_WHITE) == 0)) {
	AllocateColors(colorPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * FreeColorTable --
 *
 *	This procedure is called when an instance ceases using a
 *	color table.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If no other instances are using this color table, a when-idle
 *	handler is registered to free up the color table and the colors
 *	allocated for it.
 *
 *----------------------------------------------------------------------
 */

static void
FreeColorTable(colorPtr)
    ColorTable *colorPtr;	/* Pointer to the color table which is
				 * no longer required by an instance. */
{
    colorPtr->refCount--;
    if (colorPtr->refCount > 0) {
	return;
    }
    if ((colorPtr->flags & DISPOSE_PENDING) == 0) {
	Tcl_DoWhenIdle(DisposeColorTable, (ClientData) colorPtr);
	colorPtr->flags |= DISPOSE_PENDING;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * AllocateColors --
 *
 *	This procedure allocates the colors required by a color table,
 *	and sets up the fields in the color table data structure which
 *	are used in dithering.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Colors are allocated from the X server.  Fields in the
 *	color table data structure are updated.
 *
 *----------------------------------------------------------------------
 */

static void
AllocateColors(colorPtr)
    ColorTable *colorPtr;	/* Pointer to the color table requiring
				 * colors to be allocated. */
{
    int i, r, g, b, rMult, mono;
    int numColors, nRed, nGreen, nBlue;
    double fr, fg, fb, igam;
    XColor *colors;
    unsigned long *pixels;

    /* 16-bit intensity value for i/n of full intensity. */
#   define CFRAC(i, n)	((i) * 65535 / (n))

    /* As for CFRAC, but apply exponent of g. */
#   define CGFRAC(i, n, g)	((int)(65535 * pow((double)(i) / (n), (g))))

    /*
     * First parse the palette specification to get the required number of
     * shades of each primary.
     */

    mono = sscanf(colorPtr->id.palette, "%d/%d/%d", &nRed, &nGreen, &nBlue)
	    <= 1;
    igam = 1.0 / colorPtr->id.gamma;

    /*
     * Each time around this loop, we reduce the number of colors we're
     * trying to allocate until we succeed in allocating all of the colors
     * we need.
     */

    for (;;) {
	/*
	 * If we are using 1 bit/pixel, we don't need to allocate
	 * any colors (we just use the foreground and background
	 * colors in the GC).
	 */

	if (mono && (nRed <= 2)) {
	    colorPtr->flags |= BLACK_AND_WHITE;
	    return;
	}

	/*
	 * Calculate the RGB coordinates of the colors we want to
	 * allocate and store them in *colors.
	 */

	if ((colorPtr->visualInfo.class == DirectColor)
	    || (colorPtr->visualInfo.class == TrueColor)) {

	    /*
	     * Direct/True Color: allocate shades of red, green, blue
	     * independently.
	     */

	    if (mono) {
		numColors = nGreen = nBlue = nRed;
	    } else {
		numColors = MAX(MAX(nRed, nGreen), nBlue);
	    }
	    colors = (XColor *) ckalloc(numColors * sizeof(XColor));

	    for (i = 0; i < numColors; ++i) {
		if (igam == 1.0) {
		    colors[i].red = CFRAC(i, nRed - 1);
		    colors[i].green = CFRAC(i, nGreen - 1);
		    colors[i].blue = CFRAC(i, nBlue - 1);
		} else {
		    colors[i].red = CGFRAC(i, nRed - 1, igam);
		    colors[i].green = CGFRAC(i, nGreen - 1, igam);
		    colors[i].blue = CGFRAC(i, nBlue - 1, igam);
		}
	    }
	} else {
	    /*
	     * PseudoColor, StaticColor, GrayScale or StaticGray visual:
	     * we have to allocate each color in the color cube separately.
	     */

	    numColors = (mono) ? nRed: (nRed * nGreen * nBlue);
	    colors = (XColor *) ckalloc(numColors * sizeof(XColor));

	    if (!mono) {
		/*
		 * Color display using a PseudoColor or StaticColor visual.
		 */

		i = 0;
		for (r = 0; r < nRed; ++r) {
		    for (g = 0; g < nGreen; ++g) {
			for (b = 0; b < nBlue; ++b) {
			    if (igam == 1.0) {
				colors[i].red = CFRAC(r, nRed - 1);
				colors[i].green = CFRAC(g, nGreen - 1);
				colors[i].blue = CFRAC(b, nBlue - 1);
			    } else {
				colors[i].red = CGFRAC(r, nRed - 1, igam);
				colors[i].green = CGFRAC(g, nGreen - 1, igam);
				colors[i].blue = CGFRAC(b, nBlue - 1, igam);
			    }
			    i++;
			}
		    }
		}
	    } else {
		/*
		 * Monochrome display - allocate the shades of grey we want.
		 */

		for (i = 0; i < numColors; ++i) {
		    if (igam == 1.0) {
			r = CFRAC(i, numColors - 1);
		    } else {
			r = CGFRAC(i, numColors - 1, igam);
		    }
		    colors[i].red = colors[i].green = colors[i].blue = r;
		}
	    }
	}

	/*
	 * Now try to allocate the colors we've calculated.
	 */

	pixels = (unsigned long *) ckalloc(numColors * sizeof(unsigned long));
	for (i = 0; i < numColors; ++i) {
	    if (!XAllocColor(colorPtr->id.display, colorPtr->id.colormap,
		    &colors[i])) {

		/*
		 * Can't get all the colors we want in the default colormap;
		 * first try freeing colors from other unused color tables.
		 */

		if (!ReclaimColors(&colorPtr->id, numColors - i)
			|| !XAllocColor(colorPtr->id.display,
			colorPtr->id.colormap, &colors[i])) {
		    /*
		     * Still can't allocate the color.
		     */
		    break;
		}
	    }
	    pixels[i] = colors[i].pixel;
	}

	/*
	 * If we didn't get all of the colors, reduce the
	 * resolution of the color cube, free the ones we got,
	 * and try again.
	 */

	if (i >= numColors) {
	    break;
	}
	XFreeColors(colorPtr->id.display, colorPtr->id.colormap, pixels, i, 0);
	ckfree((char *) colors);
	ckfree((char *) pixels);

	if (!mono) {
	    if ((nRed == 2) && (nGreen == 2) && (nBlue == 2)) {
		/*
		 * Fall back to 1-bit monochrome display.
		 */

		mono = 1;
	    } else {
		/*
		 * Reduce the number of shades of each primary to about
		 * 3/4 of the previous value.  This should reduce the
		 * total number of colors required to about half the
		 * previous value for PseudoColor displays.
		 */

		nRed = (nRed * 3 + 2) / 4;
		nGreen = (nGreen * 3 + 2) / 4;
		nBlue = (nBlue * 3 + 2) / 4;
	    }
	} else {
	    /*
	     * Reduce the number of shades of gray to about 1/2.
	     */

	    nRed = nRed / 2;
	}
    }
    
    /*
     * We have allocated all of the necessary colors:
     * fill in various fields of the ColorTable record.
     */

    if (!mono) {
	colorPtr->flags |= COLOR_WINDOW;

	/*
	 * The following is a hairy hack.  We only want to index into
	 * the pixelMap on colormap displays.  However, if the display
	 * is on Windows, then we actually want to store the index not
	 * the value since we will be passing the color table into the
	 * TkPutImage call.
	 */
	
#ifndef __WIN32__
	if ((colorPtr->visualInfo.class != DirectColor)
		&& (colorPtr->visualInfo.class != TrueColor)) {
	    colorPtr->flags |= MAP_COLORS;
	}
#endif /* __WIN32__ */
    }

    colorPtr->numColors = numColors;
    colorPtr->pixelMap = pixels;

    /*
     * Set up quantization tables for dithering.
     */
    rMult = nGreen * nBlue;
    for (i = 0; i < 256; ++i) {
	r = (i * (nRed - 1) + 127) / 255;
	if (mono) {
	    fr = (double) colors[r].red / 65535.0;
	    if (colorPtr->id.gamma != 1.0 ) {
		fr = pow(fr, colorPtr->id.gamma);
	    }
	    colorPtr->colorQuant[0][i] = (int)(fr * 255.99);
	    colorPtr->redValues[i] = colors[r].pixel;
	} else {
	    g = (i * (nGreen - 1) + 127) / 255;
	    b = (i * (nBlue - 1) + 127) / 255;
	    if ((colorPtr->visualInfo.class == DirectColor)
		    || (colorPtr->visualInfo.class == TrueColor)) {
		colorPtr->redValues[i] = colors[r].pixel
		    & colorPtr->visualInfo.red_mask;
		colorPtr->greenValues[i] = colors[g].pixel
		    & colorPtr->visualInfo.green_mask;
		colorPtr->blueValues[i] = colors[b].pixel
		    & colorPtr->visualInfo.blue_mask;
	    } else {
		r *= rMult;
		g *= nBlue;
		colorPtr->redValues[i] = r;
		colorPtr->greenValues[i] = g;
		colorPtr->blueValues[i] = b;
	    }
	    fr = (double) colors[r].red / 65535.0;
	    fg = (double) colors[g].green / 65535.0;
	    fb = (double) colors[b].blue / 65535.0;
	    if (colorPtr->id.gamma != 1.0) {
		fr = pow(fr, colorPtr->id.gamma);
		fg = pow(fg, colorPtr->id.gamma);
		fb = pow(fb, colorPtr->id.gamma);
	    }
	    colorPtr->colorQuant[0][i] = (int)(fr * 255.99);
	    colorPtr->colorQuant[1][i] = (int)(fg * 255.99);
	    colorPtr->colorQuant[2][i] = (int)(fb * 255.99);
	}
    }

    ckfree((char *) colors);
}

/*
 *----------------------------------------------------------------------
 *
 * DisposeColorTable --
 *
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The colors in the argument color table are freed, as is the
 *	color table structure itself.  The color table is removed
 *	from the hash table which is used to locate color tables.
 *
 *----------------------------------------------------------------------
 */

static void
DisposeColorTable(clientData)
    ClientData clientData;	/* Pointer to the ColorTable whose
				 * colors are to be released. */
{
    ColorTable *colorPtr;
    Tcl_HashEntry *entry;

    colorPtr = (ColorTable *) clientData;
    if (colorPtr->pixelMap != NULL) {
	if (colorPtr->numColors > 0) {
	    XFreeColors(colorPtr->id.display, colorPtr->id.colormap,
		    colorPtr->pixelMap, colorPtr->numColors, 0);
	    Tk_FreeColormap(colorPtr->id.display, colorPtr->id.colormap);
	}
	ckfree((char *) colorPtr->pixelMap);
    }

    entry = Tcl_FindHashEntry(&imgPhotoColorHash, (char *) &colorPtr->id);
    if (entry == NULL) {
	panic("DisposeColorTable couldn't find hash entry");
    }
    Tcl_DeleteHashEntry(entry);

    ckfree((char *) colorPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * ReclaimColors --
 *
 *	This procedure is called to try to free up colors in the
 *	colormap used by a color table.  It looks for other color
 *	tables with the same colormap and with a zero live reference
 *	count, and frees their colors.  It only does so if there is
 *	the possibility of freeing up at least `numColors' colors.
 *
 * Results:
 *	The return value is TRUE if any colors were freed, FALSE
 *	otherwise.
 *
 * Side effects:
 *	ColorTables which are not currently in use may lose their
 *	color allocations.
 *
 *---------------------------------------------------------------------- */

static int
ReclaimColors(id, numColors)
    ColorTableId *id;		/* Pointer to information identifying
				 * the color table which needs more colors. */
    int numColors;		/* Number of colors required. */
{
    Tcl_HashSearch srch;
    Tcl_HashEntry *entry;
    ColorTable *colorPtr;
    int nAvail;

    /*
     * First scan through the color hash table to get an
     * upper bound on how many colors we might be able to free.
     */

    nAvail = 0;
    entry = Tcl_FirstHashEntry(&imgPhotoColorHash, &srch);
    while (entry != NULL) {
	colorPtr = (ColorTable *) Tcl_GetHashValue(entry);
	if ((colorPtr->id.display == id->display)
	    && (colorPtr->id.colormap == id->colormap)
	    && (colorPtr->liveRefCount == 0 )&& (colorPtr->numColors != 0)
	    && ((colorPtr->id.palette != id->palette)
		|| (colorPtr->id.gamma != id->gamma))) {

	    /*
	     * We could take this guy's colors off him.
	     */

	    nAvail += colorPtr->numColors;
	}
	entry = Tcl_NextHashEntry(&srch);
    }

    /*
     * nAvail is an (over)estimate of the number of colors we could free.
     */

    if (nAvail < numColors) {
	return 0;
    }

    /*
     * Scan through a second time freeing colors.
     */

    entry = Tcl_FirstHashEntry(&imgPhotoColorHash, &srch);
    while ((entry != NULL) && (numColors > 0)) {
	colorPtr = (ColorTable *) Tcl_GetHashValue(entry);
	if ((colorPtr->id.display == id->display)
		&& (colorPtr->id.colormap == id->colormap)
		&& (colorPtr->liveRefCount == 0) && (colorPtr->numColors != 0)
		&& ((colorPtr->id.palette != id->palette)
		    || (colorPtr->id.gamma != id->gamma))) {

	    /*
	     * Free the colors that this ColorTable has.
	     */

	    XFreeColors(colorPtr->id.display, colorPtr->id.colormap,
		    colorPtr->pixelMap, colorPtr->numColors, 0);
	    numColors -= colorPtr->numColors;
	    colorPtr->numColors = 0;
	    ckfree((char *) colorPtr->pixelMap);
	    colorPtr->pixelMap = NULL;
	}

	entry = Tcl_NextHashEntry(&srch);
    }
    return 1;			/* we freed some colors */
}

/*
 *----------------------------------------------------------------------
 *
 * DisposeInstance --
 *
 *	This procedure is called to finally free up an instance
 *	of a photo image which is no longer required.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The instance data structure and the resources it references
 *	are freed.
 *
 *----------------------------------------------------------------------
 */

static void
DisposeInstance(clientData)
    ClientData clientData;	/* Pointer to the instance whose resources
				 * are to be released. */
{
    PhotoInstance *instancePtr = (PhotoInstance *) clientData;
    PhotoInstance *prevPtr;

    if (instancePtr->pixels != None) {
	Tk_FreePixmap(instancePtr->display, instancePtr->pixels);
    }
    if (instancePtr->gc != None) {
	Tk_FreeGC(instancePtr->display, instancePtr->gc);
    }
    if (instancePtr->imagePtr != NULL) {
	XFree((char *) instancePtr->imagePtr);
    }
    if (instancePtr->error != NULL) {
	ckfree((char *) instancePtr->error);
    }
    if (instancePtr->colorTablePtr != NULL) {
	FreeColorTable(instancePtr->colorTablePtr);
    }

    if (instancePtr->masterPtr->instancePtr == instancePtr) {
	instancePtr->masterPtr->instancePtr = instancePtr->nextPtr;
    } else {
	for (prevPtr = instancePtr->masterPtr->instancePtr;
		prevPtr->nextPtr != instancePtr; prevPtr = prevPtr->nextPtr) {
	    /* Empty loop body */
	}
	prevPtr->nextPtr = instancePtr->nextPtr;
    }
    Tk_FreeColormap(instancePtr->display, instancePtr->colormap);
    ckfree((char *) instancePtr);
}

/*
 *----------------------------------------------------------------------
 *
 * MatchFileFormat --
 *
 *	This procedure is called to find a photo image file format
 *	handler which can parse the image data in the given file.
 *	If a user-specified format string is provided, only handlers
 *	whose names match a prefix of the format string are tried.
 *
 * Results:
 *	A standard TCL return value.  If the return value is TCL_OK, a
 *	pointer to the image format record is returned in
 *	*imageFormatPtr, and the width and height of the image are
 *	returned in *widthPtr and *heightPtr.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
MatchFileFormat(interp, f, fileName, formatString, imageFormatPtr,
	widthPtr, heightPtr)
    Tcl_Interp *interp;		/* Interpreter to use for reporting errors. */
    FILE *f;			/* The image file, open for reading. */
    char *fileName;		/* The name of the image file. */
    char *formatString;		/* User-specified format string, or NULL. */
    Tk_PhotoImageFormat **imageFormatPtr;
				/* A pointer to the photo image format
				 * record is returned here. */
    int *widthPtr, *heightPtr;	/* The dimensions of the image are
				 * returned here. */
{
    int matched;
    Tk_PhotoImageFormat *formatPtr;

    /*
     * Scan through the table of file format handlers to find
     * one which can handle the image.
     */

    matched = 0;
    for (formatPtr = formatList; formatPtr != NULL;
	 formatPtr = formatPtr->nextPtr) {
	if (formatString != NULL) {
	    if (strncasecmp(formatString, formatPtr->name,
		    strlen(formatPtr->name)) != 0) {
		continue;
	    }
	    matched = 1;
	    if (formatPtr->fileMatchProc == NULL) {
		Tcl_AppendResult(interp, "-file option isn't supported for ",
			formatString, " images", (char *) NULL);
		return TCL_ERROR;
	    }
	}
	if (formatPtr->fileMatchProc != NULL) {
	    fseek(f, 0L, SEEK_SET);
	    if ((*formatPtr->fileMatchProc)(f, fileName, formatString,
		    widthPtr, heightPtr)) {
		if (*widthPtr < 1) {
		    *widthPtr = 1;
		}
		if (*heightPtr < 1) {
		    *heightPtr = 1;
		}
		break;
	    }
	}
    }

    if (formatPtr == NULL) {
	if ((formatString != NULL) && !matched) {
	    Tcl_AppendResult(interp, "image file format \"", formatString,
		    "\" is not supported", (char *) NULL);
	} else {
	    Tcl_AppendResult(interp,
		    "couldn't recognize data in image file \"",
		    fileName, "\"", (char *) NULL);
	}
	return TCL_ERROR;
    }

    *imageFormatPtr = formatPtr;
    fseek(f, 0L, SEEK_SET);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * MatchStringFormat --
 *
 *	This procedure is called to find a photo image file format
 *	handler which can parse the image data in the given string.
 *	If a user-specified format string is provided, only handlers
 *	whose names match a prefix of the format string are tried.
 *
 * Results:
 *	A standard TCL return value.  If the return value is TCL_OK, a
 *	pointer to the image format record is returned in
 *	*imageFormatPtr, and the width and height of the image are
 *	returned in *widthPtr and *heightPtr.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
MatchStringFormat(interp, string, formatString, imageFormatPtr,
	widthPtr, heightPtr)
    Tcl_Interp *interp;		/* Interpreter to use for reporting errors. */
    char *string;		/* String containing the image data. */
    char *formatString;		/* User-specified format string, or NULL. */
    Tk_PhotoImageFormat **imageFormatPtr;
				/* A pointer to the photo image format
				 * record is returned here. */
    int *widthPtr, *heightPtr;	/* The dimensions of the image are
				 * returned here. */
{
    int matched;
    Tk_PhotoImageFormat *formatPtr;

    /*
     * Scan through the table of file format handlers to find
     * one which can handle the image.
     */

    matched = 0;
    for (formatPtr = formatList; formatPtr != NULL;
	    formatPtr = formatPtr->nextPtr) {
	if (formatString != NULL) {
	    if (strncasecmp(formatString, formatPtr->name,
		    strlen(formatPtr->name)) != 0) {
		continue;
	    }
	    matched = 1;
	    if (formatPtr->stringMatchProc == NULL) {
		Tcl_AppendResult(interp, "-data option isn't supported for ",
			formatString, " images", (char *) NULL);
		return TCL_ERROR;
	    }
	}
	if ((formatPtr->stringMatchProc != NULL)
		&& (*formatPtr->stringMatchProc)(string, formatString,
		widthPtr, heightPtr)) {
	    break;
	}
    }

    if (formatPtr == NULL) {
	if ((formatString != NULL) && !matched) {
	    Tcl_AppendResult(interp, "image format \"", formatString,
		    "\" is not supported", (char *) NULL);
	} else {
	    Tcl_AppendResult(interp, "couldn't recognize image data",
		    (char *) NULL);
	}
	return TCL_ERROR;
    }

    *imageFormatPtr = formatPtr;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_FindPhoto --
 *
 *	This procedure is called to get an opaque handle (actually a
 *	PhotoMaster *) for a given image, which can be used in
 *	subsequent calls to Tk_PhotoPutBlock, etc.  The `name'
 *	parameter is the name of the image.
 *
 * Results:
 *	The handle for the photo image, or NULL if there is no
 *	photo image with the name given.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tk_PhotoHandle
Tk_FindPhoto(imageName)
    char *imageName;		/* Name of the desired photo image. */
{
    Tcl_HashEntry *entry;

    if (!imgPhotoHashInitialized) {
	return NULL;
    }
    entry = Tcl_FindHashEntry(&imgPhotoHash, imageName);
    if (entry == NULL) {
	return NULL;
    }
    return (Tk_PhotoHandle) Tcl_GetHashValue(entry);
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_PhotoPutBlock --
 *
 *	This procedure is called to put image data into a photo image.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The image data is stored.  The image may be expanded.
 *	The Tk image code is informed that the image has changed.
 *
 *---------------------------------------------------------------------- */

void
Tk_PhotoPutBlock(handle, blockPtr, x, y, width, height)
    Tk_PhotoHandle handle;	/* Opaque handle for the photo image
				 * to be updated. */
    register Tk_PhotoImageBlock *blockPtr;
				/* Pointer to a structure describing the
				 * pixel data to be copied into the image. */
    int x, y;			/* Coordinates of the top-left pixel to
				 * be updated in the image. */
    int width, height;		/* Dimensions of the area of the image
				 * to be updated. */
{
    register PhotoMaster *masterPtr;
    int xEnd, yEnd;
    int greenOffset, blueOffset;
    int wLeft, hLeft;
    int wCopy, hCopy;
    unsigned char *srcPtr, *srcLinePtr;
    unsigned char *destPtr, *destLinePtr;
    int pitch;
    XRectangle rect;

    masterPtr = (PhotoMaster *) handle;

    if ((masterPtr->userWidth != 0) && ((x + width) > masterPtr->userWidth)) {
	width = masterPtr->userWidth - x;
    }
    if ((masterPtr->userHeight != 0)
	    && ((y + height) > masterPtr->userHeight)) {
	height = masterPtr->userHeight - y;
    }
    if ((width <= 0) || (height <= 0))
	return;

    xEnd = x + width;
    yEnd = y + height;
    if ((xEnd > masterPtr->width) || (yEnd > masterPtr->height)) {
	ImgPhotoSetSize(masterPtr, MAX(xEnd, masterPtr->width),
		MAX(yEnd, masterPtr->height));
    }

    if ((y < masterPtr->ditherY) || ((y == masterPtr->ditherY)
	    && (x < masterPtr->ditherX))) {
	/*
	 * The dithering isn't correct past the start of this block.
	 */
	masterPtr->ditherX = x;
	masterPtr->ditherY = y;
    }

    /*
     * If this image block could have different red, green and blue
     * components, mark it as a color image.
     */

    greenOffset = blockPtr->offset[1] - blockPtr->offset[0];
    blueOffset = blockPtr->offset[2] - blockPtr->offset[0];
    if ((greenOffset != 0) || (blueOffset != 0)) {
	masterPtr->flags |= COLOR_IMAGE;
    }

    /*
     * Copy the data into our local 24-bit/pixel array.
     * If we can do it with a single memcpy, we do.
     */

    destLinePtr = masterPtr->pix24 + (y * masterPtr->width + x) * 3;
    pitch = masterPtr->width * 3;

    if ((blockPtr->pixelSize == 3) && (greenOffset == 1) && (blueOffset == 2)
	    && (width <= blockPtr->width) && (height <= blockPtr->height)
	    && ((height == 1) || ((x == 0) && (width == masterPtr->width)
		&& (blockPtr->pitch == pitch)))) {
	memcpy((VOID *) destLinePtr,
		(VOID *) (blockPtr->pixelPtr + blockPtr->offset[0]),
		(size_t) (height * width * 3));
    } else {
	for (hLeft = height; hLeft > 0;) {
	    srcLinePtr = blockPtr->pixelPtr + blockPtr->offset[0];
	    hCopy = MIN(hLeft, blockPtr->height);
	    hLeft -= hCopy;
	    for (; hCopy > 0; --hCopy) {
		destPtr = destLinePtr;
		for (wLeft = width; wLeft > 0;) {
		    wCopy = MIN(wLeft, blockPtr->width);
		    wLeft -= wCopy;
		    srcPtr = srcLinePtr;
		    for (; wCopy > 0; --wCopy) {
			*destPtr++ = srcPtr[0];
			*destPtr++ = srcPtr[greenOffset];
			*destPtr++ = srcPtr[blueOffset];
			srcPtr += blockPtr->pixelSize;
		    }
		}
		srcLinePtr += blockPtr->pitch;
		destLinePtr += pitch;
	    }
	}
    }

    /*
     * Add this new block to the region which specifies which data is valid.
     */

    rect.x = x;
    rect.y = y;
    rect.width = width;
    rect.height = height;
    TkUnionRectWithRegion(&rect, masterPtr->validRegion,
	    masterPtr->validRegion);

    /*
     * Update each instance.
     */

    Dither(masterPtr, x, y, width, height);

    /*
     * Tell the core image code that this image has changed.
     */

    Tk_ImageChanged(masterPtr->tkMaster, x, y, width, height, masterPtr->width,
	    masterPtr->height);
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_PhotoPutZoomedBlock --
 *
 *	This procedure is called to put image data into a photo image,
 *	with possible subsampling and/or zooming of the pixels.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The image data is stored.  The image may be expanded.
 *	The Tk image code is informed that the image has changed.
 *
 *----------------------------------------------------------------------
 */

void
Tk_PhotoPutZoomedBlock(handle, blockPtr, x, y, width, height, zoomX, zoomY,
	subsampleX, subsampleY)
    Tk_PhotoHandle handle;	/* Opaque handle for the photo image
				 * to be updated. */
    register Tk_PhotoImageBlock *blockPtr;
				/* Pointer to a structure describing the
				 * pixel data to be copied into the image. */
    int x, y;			/* Coordinates of the top-left pixel to
				 * be updated in the image. */
    int width, height;		/* Dimensions of the area of the image
				 * to be updated. */
    int zoomX, zoomY;		/* Zoom factors for the X and Y axes. */
    int subsampleX, subsampleY;	/* Subsampling factors for the X and Y axes. */
{
    register PhotoMaster *masterPtr;
    int xEnd, yEnd;
    int greenOffset, blueOffset;
    int wLeft, hLeft;
    int wCopy, hCopy;
    int blockWid, blockHt;
    unsigned char *srcPtr, *srcLinePtr, *srcOrigPtr;
    unsigned char *destPtr, *destLinePtr;
    int pitch;
    int xRepeat, yRepeat;
    int blockXSkip, blockYSkip;
    XRectangle rect;

    if ((zoomX == 1) && (zoomY == 1) && (subsampleX == 1)
	    && (subsampleY == 1)) {
	Tk_PhotoPutBlock(handle, blockPtr, x, y, width, height);
	return;
    }

    masterPtr = (PhotoMaster *) handle;

    if ((zoomX <= 0) || (zoomY <= 0))
	return;
    if ((masterPtr->userWidth != 0) && ((x + width) > masterPtr->userWidth)) {
	width = masterPtr->userWidth - x;
    }
    if ((masterPtr->userHeight != 0)
	    && ((y + height) > masterPtr->userHeight)) {
	height = masterPtr->userHeight - y;
    }
    if ((width <= 0) || (height <= 0))
	return;

    xEnd = x + width;
    yEnd = y + height;
    if ((xEnd > masterPtr->width) || (yEnd > masterPtr->height)) {
	ImgPhotoSetSize(masterPtr, MAX(xEnd, masterPtr->width),
		MAX(yEnd, masterPtr->height));
    }

    if ((y < masterPtr->ditherY) || ((y == masterPtr->ditherY)
	   && (x < masterPtr->ditherX))) {
	/*
	 * The dithering isn't correct past the start of this block.
	 */

	masterPtr->ditherX = x;
	masterPtr->ditherY = y;
    }

    /*
     * If this image block could have different red, green and blue
     * components, mark it as a color image.
     */

    greenOffset = blockPtr->offset[1] - blockPtr->offset[0];
    blueOffset = blockPtr->offset[2] - blockPtr->offset[0];
    if ((greenOffset != 0) || (blueOffset != 0)) {
	masterPtr->flags |= COLOR_IMAGE;
    }

    /*
     * Work out what area the pixel data in the block expands to after
     * subsampling and zooming.
     */

    blockXSkip = subsampleX * blockPtr->pixelSize;
    blockYSkip = subsampleY * blockPtr->pitch;
    if (subsampleX > 0)
	blockWid = ((blockPtr->width + subsampleX - 1) / subsampleX) * zoomX;
    else if (subsampleX == 0)
	blockWid = width;
    else
	blockWid = ((blockPtr->width - subsampleX - 1) / -subsampleX) * zoomX;
    if (subsampleY > 0)
	blockHt = ((blockPtr->height + subsampleY - 1) / subsampleY) * zoomY;
    else if (subsampleY == 0)
	blockHt = height;
    else
	blockHt = ((blockPtr->height - subsampleY - 1) / -subsampleY) * zoomY;

    /*
     * Copy the data into our local 24-bit/pixel array.
     */

    destLinePtr = masterPtr->pix24 + (y * masterPtr->width + x) * 3;
    srcOrigPtr = blockPtr->pixelPtr + blockPtr->offset[0];
    if (subsampleX < 0) {
	srcOrigPtr += (blockPtr->width - 1) * blockPtr->pixelSize;
    }
    if (subsampleY < 0) {
	srcOrigPtr += (blockPtr->height - 1) * blockPtr->pitch;
    }

    pitch = masterPtr->width * 3;
    for (hLeft = height; hLeft > 0; ) {
	hCopy = MIN(hLeft, blockHt);
	hLeft -= hCopy;
	yRepeat = zoomY;
	srcLinePtr = srcOrigPtr;
	for (; hCopy > 0; --hCopy) {
	    destPtr = destLinePtr;
	    for (wLeft = width; wLeft > 0;) {
		wCopy = MIN(wLeft, blockWid);
		wLeft -= wCopy;
		srcPtr = srcLinePtr;
		for (; wCopy > 0; wCopy -= zoomX) {
		    for (xRepeat = MIN(wCopy, zoomX); xRepeat > 0; xRepeat--) {
			*destPtr++ = srcPtr[0];
			*destPtr++ = srcPtr[greenOffset];
			*destPtr++ = srcPtr[blueOffset];
		    }
		    srcPtr += blockXSkip;
		}
	    }
	    destLinePtr += pitch;
	    yRepeat--;
	    if (yRepeat <= 0) {
		srcLinePtr += blockYSkip;
		yRepeat = zoomY;
	    }
	}
    }

    /*
     * Add this new block to the region that specifies which data is valid.
     */

    rect.x = x;
    rect.y = y;
    rect.width = width;
    rect.height = height;
    TkUnionRectWithRegion(&rect, masterPtr->validRegion,
	    masterPtr->validRegion);

    /*
     * Update each instance.
     */

    Dither(masterPtr, x, y, width, height);

    /*
     * Tell the core image code that this image has changed.
     */

    Tk_ImageChanged(masterPtr->tkMaster, x, y, width, height, masterPtr->width,
	    masterPtr->height);
}

/*
 *----------------------------------------------------------------------
 *
 * Dither --
 *
 *	This procedure is called to update an area of each instance's
 *	pixmap by dithering the corresponding area of the image master.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The pixmap of each instance of this image gets updated.
 *	The fields in *masterPtr indicating which area of the image
 *	is correctly dithered get updated.
 *
 *----------------------------------------------------------------------
 */

static void
Dither(masterPtr, x, y, width, height)
    PhotoMaster *masterPtr;	/* Image master whose instances are
				 * to be updated. */
    int x, y;			/* Coordinates of the top-left pixel
				 * in the area to be dithered. */
    int width, height;		/* Dimensions of the area to be dithered. */
{
    PhotoInstance *instancePtr;

    if ((width <= 0) || (height <= 0)) {
	return;
    }

    for (instancePtr = masterPtr->instancePtr; instancePtr != NULL;
	    instancePtr = instancePtr->nextPtr) {
	DitherInstance(instancePtr, x, y, width, height);
    }

    /*
     * Work out whether this block will be correctly dithered
     * and whether it will extend the correctly dithered region.
     */

    if (((y < masterPtr->ditherY)
	    || ((y == masterPtr->ditherY) && (x <= masterPtr->ditherX)))
	    && ((y + height) > (masterPtr->ditherY))) {

	/*
	 * This block starts inside (or immediately after) the correctly
	 * dithered region, so the first scan line at least will be right.
	 * Furthermore this block extends into scanline masterPtr->ditherY.
	 */

	if ((x == 0) && (width == masterPtr->width)) {
	    /*
	     * We are doing the full width, therefore the dithering
	     * will be correct to the end.
	     */

	    masterPtr->ditherX = 0;
	    masterPtr->ditherY = y + height;
	} else {
	    /*
	     * We are doing partial scanlines, therefore the
	     * correctly-dithered region will be extended by
	     * at most one scan line.
	     */

	    if (x <= masterPtr->ditherX) {
		masterPtr->ditherX = x + width;
		if (masterPtr->ditherX >= masterPtr->width) {
		    masterPtr->ditherX = 0;
		    masterPtr->ditherY++;
		}
	    }
	}
    }

}    

/*
 *----------------------------------------------------------------------
 *
 * DitherInstance --
 *
 *	This procedure is called to update an area of an instance's
 *	pixmap by dithering the corresponding area of the master.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The instance's pixmap gets updated.
 *
 *----------------------------------------------------------------------
 */

static void
DitherInstance(instancePtr, xStart, yStart, width, height)
    PhotoInstance *instancePtr;	/* The instance to be updated. */
    int xStart, yStart;		/* Coordinates of the top-left pixel in the
				 * block to be dithered. */
    int width, height;		/* Dimensions of the block to be dithered. */
{
    PhotoMaster *masterPtr;
    ColorTable *colorPtr;
    XImage *imagePtr;
    int nLines, bigEndian;
    int i, c, x, y;
    int xEnd, yEnd;
    int bitsPerPixel, bytesPerLine, lineLength;
    unsigned char *srcLinePtr, *srcPtr;
    schar *errLinePtr, *errPtr;
    unsigned char *destBytePtr, *dstLinePtr;
    pixel *destLongPtr;
    pixel firstBit, word, mask;
    int col[3];
    int doDithering = 1;

    colorPtr = instancePtr->colorTablePtr;
    masterPtr = instancePtr->masterPtr;

    /*
     * Turn dithering off in certain cases where it is not
     * needed (TrueColor, DirectColor with many colors).
     */

    if ((colorPtr->visualInfo.class == DirectColor)
	    || (colorPtr->visualInfo.class == TrueColor)) {
	int nRed, nGreen, nBlue, result;

	result = sscanf(colorPtr->id.palette, "%d/%d/%d", &nRed,
		&nGreen, &nBlue);
	if ((nRed >= 256)
		&& ((result == 1) || ((nGreen >= 256) && (nBlue >= 256)))) {
	    doDithering = 0;
	}
    }

    /*
     * First work out how many lines to do at a time,
     * then how many bytes we'll need for pixel storage,
     * and allocate it.
     */

    nLines = (MAX_PIXELS + width - 1) / width;
    if (nLines < 1) {
	nLines = 1;
    }
    if (nLines > height ) {
	nLines = height;
    }

    imagePtr = instancePtr->imagePtr;
    if (imagePtr == NULL) {
	return;			/* we must be really tight on memory */
    }
    bitsPerPixel = imagePtr->bits_per_pixel;
    bytesPerLine = ((bitsPerPixel * width + 31) >> 3) & ~3;
    imagePtr->width = width;
    imagePtr->height = nLines;
    imagePtr->bytes_per_line = bytesPerLine;
    imagePtr->data = (char *) ckalloc((unsigned) (imagePtr->bytes_per_line * nLines));
    bigEndian = imagePtr->bitmap_bit_order == MSBFirst;
    firstBit = bigEndian? (1 << (imagePtr->bitmap_unit - 1)): 1;

    lineLength = masterPtr->width * 3;
    srcLinePtr = masterPtr->pix24 + yStart * lineLength + xStart * 3;
    errLinePtr = instancePtr->error + yStart * lineLength + xStart * 3;
    xEnd = xStart + width;

    /*
     * Loop over the image, doing at most nLines lines before
     * updating the screen image.
     */

    for (; height > 0; height -= nLines) {
	if (nLines > height) {
	    nLines = height;
	}
	dstLinePtr = (unsigned char *) imagePtr->data;
	yEnd = yStart + nLines;
	for (y = yStart; y < yEnd; ++y) {
	    srcPtr = srcLinePtr;
	    errPtr = errLinePtr;
	    destBytePtr = dstLinePtr;
	    destLongPtr = (pixel *) dstLinePtr;
	    if (colorPtr->flags & COLOR_WINDOW) {
		/*
		 * Color window.  We dither the three components
		 * independently, using Floyd-Steinberg dithering,
		 * which propagates errors from the quantization of
		 * pixels to the pixels below and to the right.
		 */

		for (x = xStart; x < xEnd; ++x) {
		    if (doDithering) {
			for (i = 0; i < 3; ++i) {
			    /*
			     * Compute the error propagated into this pixel
			     * for this component.
			     * If e[x,y] is the array of quantization error
			     * values, we compute
			     *     7/16 * e[x-1,y] + 1/16 * e[x-1,y-1]
			     *   + 5/16 * e[x,y-1] + 3/16 * e[x+1,y-1]
			     * and round it to an integer.
			     *
			     * The expression ((c + 2056) >> 4) - 128
			     * computes round(c / 16), and works correctly on
			     * machines without a sign-extending right shift.
			     */
			    
			    c = (x > 0) ? errPtr[-3] * 7: 0;
			    if (y > 0) {
				if (x > 0) {
				    c += errPtr[-lineLength-3];
				}
				c += errPtr[-lineLength] * 5;
				if ((x + 1) < masterPtr->width) {
				    c += errPtr[-lineLength+3] * 3;
				}
			    }
			    
			    /*
			     * Add the propagated error to the value of this
			     * component, quantize it, and store the
			     * quantization error.
			     */
			    
			    c = ((c + 2056) >> 4) - 128 + *srcPtr++;
			    if (c < 0) {
				c = 0;
			    } else if (c > 255) {
				c = 255;
			    }
			    col[i] = colorPtr->colorQuant[i][c];
			    *errPtr++ = c - col[i];
			}
		    } else {
			/* 
			 * Output is virtually continuous in this case,
			 * so don't bother dithering.
			 */

			col[0] = *srcPtr++;
			col[1] = *srcPtr++;
			col[2] = *srcPtr++;
		    }

		    /*
		     * Translate the quantized component values into
		     * an X pixel value, and store it in the image.
		     */

		    i = colorPtr->redValues[col[0]]
			    + colorPtr->greenValues[col[1]]
			    + colorPtr->blueValues[col[2]];
		    if (colorPtr->flags & MAP_COLORS) {
			i = colorPtr->pixelMap[i];
		    }
		    switch (bitsPerPixel) {
			case NBBY:
			    *destBytePtr++ = i;
			    break;
			case NBBY * sizeof(pixel):
			    *destLongPtr++ = i;
			    break;
			default:
			    XPutPixel(imagePtr, x - xStart, y - yStart,
				    (unsigned) i);
		    }
		}

	    } else if (bitsPerPixel > 1) {
		/*
		 * Multibit monochrome window.  The operation here is similar
		 * to the color window case above, except that there is only
		 * one component.  If the master image is in color, use the
		 * luminance computed as
		 *	0.344 * red + 0.5 * green + 0.156 * blue.
		 */

		for (x = xStart; x < xEnd; ++x) {
		    c = (x > 0) ? errPtr[-1] * 7: 0;
		    if (y > 0) {
			if (x > 0)  {
			    c += errPtr[-lineLength-1];
			}
			c += errPtr[-lineLength] * 5;
			if (x + 1 < masterPtr->width) {
			    c += errPtr[-lineLength+1] * 3;
			}
		    }
		    c = ((c + 2056) >> 4) - 128;

		    if ((masterPtr->flags & COLOR_IMAGE) == 0) {
			c += srcPtr[0];
		    } else {
			c += (unsigned)(srcPtr[0] * 11 + srcPtr[1] * 16
					+ srcPtr[2] * 5 + 16) >> 5;
		    }
		    srcPtr += 3;

		    if (c < 0) {
			c = 0;
		    } else if (c > 255) {
			c = 255;
		    }
		    i = colorPtr->colorQuant[0][c];
		    *errPtr++ = c - i;
		    i = colorPtr->redValues[i];
		    switch (bitsPerPixel) {
			case NBBY:
			    *destBytePtr++ = i;
			    break;
			case NBBY * sizeof(pixel):
			    *destLongPtr++ = i;
			    break;
			default:
			    XPutPixel(imagePtr, x - xStart, y - yStart,
				    (unsigned) i);
		    }
		}
	    } else {
		/*
		 * 1-bit monochrome window.  This is similar to the
		 * multibit monochrome case above, except that the
		 * quantization is simpler (we only have black = 0
		 * and white = 255), and we produce an XY-Bitmap.
		 */

		word = 0;
		mask = firstBit;
		for (x = xStart; x < xEnd; ++x) {
		    /*
		     * If we have accumulated a whole word, store it
		     * in the image and start a new word.
		     */

		    if (mask == 0) {
			*destLongPtr++ = word;
			mask = firstBit;
			word = 0;
		    }

		    c = (x > 0) ? errPtr[-1] * 7: 0;
		    if (y > 0) {
			if (x > 0) {
			    c += errPtr[-lineLength-1];
			}
			c += errPtr[-lineLength] * 5;
			if (x + 1 < masterPtr->width) {
			    c += errPtr[-lineLength+1] * 3;
			}
		    }
		    c = ((c + 2056) >> 4) - 128;

		    if ((masterPtr->flags & COLOR_IMAGE) == 0) {
			c += srcPtr[0];
		    } else {
			c += (unsigned)(srcPtr[0] * 11 + srcPtr[1] * 16
					+ srcPtr[2] * 5 + 16) >> 5;
		    }
		    srcPtr += 3;

		    if (c < 0) {
			c = 0;
		    } else if (c > 255) {
			c = 255;
		    }
		    if (c >= 128) {
			word |= mask;
			*errPtr++ = c - 255;
		    } else {
			*errPtr++ = c;
		    }
		    mask = bigEndian? (mask >> 1): (mask << 1);
		}
		*destLongPtr = word;
	    }
	    srcLinePtr += lineLength;
	    errLinePtr += lineLength;
	    dstLinePtr += bytesPerLine;
	}

	/*
	 * Update the pixmap for this instance with the block of
	 * pixels that we have just computed.
	 */

	TkPutImage(colorPtr->pixelMap, colorPtr->numColors,
		instancePtr->display, instancePtr->pixels,
		instancePtr->gc, imagePtr, 0, 0, xStart, yStart,
		(unsigned) width, (unsigned) nLines);
	yStart = yEnd;
	
    }

    ckfree(imagePtr->data);
    imagePtr->data = NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_PhotoBlank --
 *
 *	This procedure is called to clear an entire photo image.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The valid region for the image is set to the null region.
 *	The generic image code is notified that the image has changed.
 *
 *----------------------------------------------------------------------
 */

void
Tk_PhotoBlank(handle)
    Tk_PhotoHandle handle;	/* Handle for the image to be blanked. */
{
    PhotoMaster *masterPtr;
    PhotoInstance *instancePtr;

    masterPtr = (PhotoMaster *) handle;
    masterPtr->ditherX = masterPtr->ditherY = 0;
    masterPtr->flags = 0;

    /*
     * The image has valid data nowhere.
     */

    if (masterPtr->validRegion != NULL) {
	TkDestroyRegion(masterPtr->validRegion);
    }
    masterPtr->validRegion = TkCreateRegion();

    /*
     * Clear out the 24-bit pixel storage array.
     * Clear out the dithering error arrays for each instance.
     */

    memset((VOID *) masterPtr->pix24, 0,
	    (size_t) (masterPtr->width * masterPtr->height));
    for (instancePtr = masterPtr->instancePtr; instancePtr != NULL;
	    instancePtr = instancePtr->nextPtr) {
	memset((VOID *) instancePtr->error, 0,
		(size_t) (masterPtr->width * masterPtr->height
		    * sizeof(schar)));
    }

    /*
     * Tell the core image code that this image has changed.
     */

    Tk_ImageChanged(masterPtr->tkMaster, 0, 0, masterPtr->width,
	    masterPtr->height, masterPtr->width, masterPtr->height);
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_PhotoExpand --
 *
 *	This procedure is called to request that a photo image be
 *	expanded if necessary to be at least `width' pixels wide and
 *	`height' pixels high.  If the user has declared a definite
 *	image size (using the -width and -height configuration
 *	options) then this call has no effect.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The size of the photo image may change; if so the generic
 *	image code is informed.
 *
 *----------------------------------------------------------------------
 */

void
Tk_PhotoExpand(handle, width, height)
    Tk_PhotoHandle handle;	/* Handle for the image to be expanded. */
    int width, height;		/* Desired minimum dimensions of the image. */
{
    PhotoMaster *masterPtr;

    masterPtr = (PhotoMaster *) handle;

    if (width <= masterPtr->width) {
	width = masterPtr->width;
    }
    if (height <= masterPtr->height) {
	height = masterPtr->height;
    }
    if ((width != masterPtr->width) || (height != masterPtr->height)) {
	ImgPhotoSetSize(masterPtr, MAX(width, masterPtr->width),
		MAX(height, masterPtr->height));
	Tk_ImageChanged(masterPtr->tkMaster, 0, 0, 0, 0, masterPtr->width,
		masterPtr->height);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_PhotoGetSize --
 *
 *	This procedure is called to obtain the current size of a photo
 *	image.
 *
 * Results:
 *	The image's width and height are returned in *widthp
 *	and *heightp.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
Tk_PhotoGetSize(handle, widthPtr, heightPtr)
    Tk_PhotoHandle handle;	/* Handle for the image whose dimensions
				 * are requested. */
    int *widthPtr, *heightPtr;	/* The dimensions of the image are returned
				 * here. */
{
    PhotoMaster *masterPtr;

    masterPtr = (PhotoMaster *) handle;
    *widthPtr = masterPtr->width;
    *heightPtr = masterPtr->height;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_PhotoSetSize --
 *
 *	This procedure is called to set size of a photo image.
 *	This call is equivalent to using the -width and -height
 *	configuration options.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The size of the image may change; if so the generic
 *	image code is informed.
 *
 *----------------------------------------------------------------------
 */

void
Tk_PhotoSetSize(handle, width, height)
    Tk_PhotoHandle handle;	/* Handle for the image whose size is to
				 * be set. */
    int width, height;		/* New dimensions for the image. */
{
    PhotoMaster *masterPtr;

    masterPtr = (PhotoMaster *) handle;

    masterPtr->userWidth = width;
    masterPtr->userHeight = height;
    ImgPhotoSetSize(masterPtr, ((width > 0) ? width: masterPtr->width),
	    ((height > 0) ? height: masterPtr->height));
    Tk_ImageChanged(masterPtr->tkMaster, 0, 0, 0, 0,
	    masterPtr->width, masterPtr->height);
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_PhotoGetImage --
 *
 *	This procedure is called to obtain image data from a photo
 *	image.  This procedure fills in the Tk_PhotoImageBlock structure
 *	pointed to by `blockPtr' with details of the address and
 *	layout of the image data in memory.
 *
 * Results:
 *	TRUE (1) indicating that image data is available,
 *	for backwards compatibility with the old photo widget.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tk_PhotoGetImage(handle, blockPtr)
    Tk_PhotoHandle handle;	/* Handle for the photo image from which
				 * image data is desired. */
    Tk_PhotoImageBlock *blockPtr;
				/* Information about the address and layout
				 * of the image data is returned here. */
{
    PhotoMaster *masterPtr;

    masterPtr = (PhotoMaster *) handle;
    blockPtr->pixelPtr = masterPtr->pix24;
    blockPtr->width = masterPtr->width;
    blockPtr->height = masterPtr->height;
    blockPtr->pitch = masterPtr->width * 3;
    blockPtr->pixelSize = 3;
    blockPtr->offset[0] = 0;
    blockPtr->offset[1] = 1;
    blockPtr->offset[2] = 2;
    return 1;
}
