/*
 * tkImgGIF.c --
 *
 * A photo image file handler for GIF files. Reads 87a and 89a GIF files.
 * At present there is no write function.
 *
 * Derived from the giftoppm code found in the pbmplus package 
 * and tkImgFmtPPM.c in the tk4.0b2 distribution by -
 *
 * Reed Wade (wade@cs.utk.edu), University of Tennessee
 *
 * Copyright (c) 1995-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * This file also contains code from the giftoppm program, which is
 * copyrighted as follows:
 *
 * +-------------------------------------------------------------------+
 * | Copyright 1990, David Koblas.                                     |
 * |   Permission to use, copy, modify, and distribute this software   |
 * |   and its documentation for any purpose and without fee is hereby |
 * |   granted, provided that the above copyright notice appear in all |
 * |   copies and that both that copyright notice and this permission  |
 * |   notice appear in supporting documentation.  This software is    |
 * |   provided "as is" without express or implied warranty.           |
 * +-------------------------------------------------------------------+
 *
 * SCCS: @(#) tkImgGIF.c 1.7 96/04/09 17:11:46
 */

#include "tkInt.h"
#include "tkPort.h"

/*
 * The format record for the GIF file format:
 */

static int      FileMatchGIF _ANSI_ARGS_((FILE *f, char *fileName,
		    char *formatString, int *widthPtr, int *heightPtr));
static int      FileReadGIF  _ANSI_ARGS_((Tcl_Interp *interp,
		    FILE *f, char *fileName, char *formatString,
		    Tk_PhotoHandle imageHandle, int destX, int destY,
		    int width, int height, int srcX, int srcY));

Tk_PhotoImageFormat tkImgFmtGIF = {
	"GIF",			/* name */
	FileMatchGIF,   /* fileMatchProc */
	NULL,           /* stringMatchProc */
	FileReadGIF,    /* fileReadProc */
	NULL,           /* stringReadProc */
	NULL,           /* fileWriteProc */
	NULL,           /* stringWriteProc */
};

#define INTERLACE		0x40
#define LOCALCOLORMAP		0x80
#define BitSet(byte, bit)	(((byte) & (bit)) == (bit))
#define MAXCOLORMAPSIZE		256
#define CM_RED			0
#define CM_GREEN		1
#define CM_BLUE			2
#define MAX_LWZ_BITS		12
#define LM_to_uint(a,b)         (((b)<<8)|(a))
#define ReadOK(file,buffer,len)	(fread(buffer, len, 1, file) != 0)

/*
 * Prototypes for local procedures defined in this file:
 */

static int		DoExtension _ANSI_ARGS_((FILE *fd, int label,
			    int *transparent));
static int		GetCode _ANSI_ARGS_((FILE *fd, int code_size,
			    int flag));
static int		GetDataBlock _ANSI_ARGS_((FILE *fd,
			    unsigned char *buf));
static int		LWZReadByte _ANSI_ARGS_((FILE *fd, int flag,
			    int input_code_size));
static int		ReadColorMap _ANSI_ARGS_((FILE *fd, int number,
			    unsigned char buffer[3][MAXCOLORMAPSIZE]));
static int		ReadGIFHeader _ANSI_ARGS_((FILE *f, int *widthPtr,
			    int *heightPtr));
static int		ReadImage _ANSI_ARGS_((Tcl_Interp *interp,
			    char *imagePtr, FILE *fd, int len, int height,
			    unsigned char cmap[3][MAXCOLORMAPSIZE],
			    int interlace, int transparent));

/*
 *----------------------------------------------------------------------
 *
 * FileMatchGIF --
 *
 *  This procedure is invoked by the photo image type to see if
 *  a file contains image data in GIF format.
 *
 * Results:
 *  The return value is 1 if the first characters in file f look
 *  like GIF data, and 0 otherwise.
 *
 * Side effects:
 *  The access position in f may change.
 *
 *----------------------------------------------------------------------
 */

static int
FileMatchGIF(f, fileName, formatString, widthPtr, heightPtr)
    FILE *f;			/* The image file, open for reading. */
    char *fileName;		/* The name of the image file. */
    char *formatString;		/* User-specified format string, or NULL. */
    int *widthPtr, *heightPtr;	/* The dimensions of the image are
				 * returned here if the file is a valid
				 * raw GIF file. */
{
	return ReadGIFHeader(f, widthPtr, heightPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * FileReadGIF --
 *
 *	This procedure is called by the photo image type to read
 *	GIF format data from a file and write it into a given
 *	photo image.
 *
 * Results:
 *	A standard TCL completion code.  If TCL_ERROR is returned
 *	then an error message is left in interp->result.
 *
 * Side effects:
 *	The access position in file f is changed, and new data is
 *	added to the image given by imageHandle.
 *
 *----------------------------------------------------------------------
 */

static int
FileReadGIF(interp, f, fileName, formatString, imageHandle, destX, destY,
	width, height, srcX, srcY)
    Tcl_Interp *interp;		/* Interpreter to use for reporting errors. */
    FILE *f;			/* The image file, open for reading. */
    char *fileName;		/* The name of the image file. */
    char *formatString;		/* User-specified format string, or NULL. */
    Tk_PhotoHandle imageHandle;	/* The photo image to write into. */
    int destX, destY;		/* Coordinates of top-left pixel in
				 * photo image to be written to. */
    int width, height;		/* Dimensions of block of photo image to
				 * be written to. */
    int srcX, srcY;		/* Coordinates of top-left pixel to be used
				 * in image being read. */
{
    int fileWidth, fileHeight;
    int nBytes;
    Tk_PhotoImageBlock block;
    unsigned char buf[100];
    int bitPixel;
    unsigned int colorResolution;
    unsigned int background;
    unsigned int aspectRatio;
    unsigned char localColorMap[3][MAXCOLORMAPSIZE];
    unsigned char colorMap[3][MAXCOLORMAPSIZE];
    int useGlobalColormap;
    int transparent = -1;

    if (!ReadGIFHeader(f, &fileWidth, &fileHeight)) {
	Tcl_AppendResult(interp, "couldn't read GIF header from file \"",
		fileName, "\"", NULL);
	return TCL_ERROR;
    }
    if ((fileWidth <= 0) || (fileHeight <= 0)) {
	Tcl_AppendResult(interp, "GIF image file \"", fileName,
		"\" has dimension(s) <= 0", (char *) NULL);
	return TCL_ERROR;
    }

    if (fread(buf, 1, 3, f) != 3) {
	return TCL_OK;
    }
    bitPixel = 2<<(buf[0]&0x07);
    colorResolution = (((buf[0]&0x70)>>3)+1);
    background = buf[1];
    aspectRatio = buf[2];

    if (BitSet(buf[0], LOCALCOLORMAP)) {    /* Global Colormap */
	if (!ReadColorMap(f, bitPixel, colorMap)) {
	    Tcl_AppendResult(interp, "error reading color map",
		    (char *) NULL);
	    return TCL_ERROR;
	}
    }

    if ((srcX + width) > fileWidth) {
	width = fileWidth - srcX;
    }
    if ((srcY + height) > fileHeight) {
	height = fileHeight - srcY;
    }
    if ((width <= 0) || (height <= 0)
	    || (srcX >= fileWidth) || (srcY >= fileHeight)) {
	return TCL_OK;
    }

    Tk_PhotoExpand(imageHandle, destX + width, destY + height);

    block.width = fileWidth;
    block.height = fileHeight;
    block.pixelSize = 3;
    block.pitch = 3 * fileWidth;
    block.offset[0] = 0;
    block.offset[1] = 1;
    block.offset[2] = 2;
    nBytes = fileHeight * block.pitch;
    block.pixelPtr = (unsigned char *) ckalloc((unsigned) nBytes);

    while (1) {
	if (fread(buf, 1, 1, f) != 1) {
	    /*
	     * Premature end of image.  We should really notify
	     * the user, but for now just show garbage.
	     */

	    break;
	}

	if (buf[0] == ';') {
	    /*
	     * GIF terminator.
	     */

	    break;
	}

	if (buf[0] == '!') {
	    /*
	     * This is a GIF extension.
	     */

	    if (fread(buf, 1, 1, f) != 1) {
		interp->result =
			"error reading extension function code in GIF image";
		goto error;
	    }
	    if (DoExtension(f, buf[0], &transparent) < 0) {
		interp->result = "error reading extension in GIF image";
		goto error;
	    }
	    continue;
	}

	if (buf[0] != ',') {
	    /*
	     * Not a valid start character; ignore it.
	     */
	    continue;
	}

	if (fread(buf, 1, 9, f) != 9) {
	    interp->result = "couldn't read left/top/width/height in GIF image";
	    goto error;
	}

	useGlobalColormap = ! BitSet(buf[8], LOCALCOLORMAP);

	bitPixel = 1<<((buf[8]&0x07)+1);

	if (!useGlobalColormap) {
	    if (!ReadColorMap(f, bitPixel, localColorMap)) {
		    Tcl_AppendResult(interp, "error reading color map", 
			    (char *) NULL);
		    goto error;
	    }
	    if (ReadImage(interp, (char *) block.pixelPtr, f, fileWidth,
		    fileHeight, localColorMap, BitSet(buf[8], INTERLACE),
		    transparent) != TCL_OK) {
		goto error;
	    }
	} else {
	    if (ReadImage(interp, (char *) block.pixelPtr, f, fileWidth,
		    fileHeight, colorMap, BitSet(buf[8], INTERLACE),
		    transparent) != TCL_OK) {
		goto error;
	    }
	}

    }

    Tk_PhotoPutBlock(imageHandle, &block, destX, destY, fileWidth, fileHeight);
    ckfree((char *) block.pixelPtr);
    return TCL_OK;

    error:
    ckfree((char *) block.pixelPtr);
    return TCL_ERROR;

}

/*
 *----------------------------------------------------------------------
 *
 * ReadGIFHeader --
 *
 *	This procedure reads the GIF header from the beginning of a
 *	GIF file and returns the dimensions of the image.
 *
 * Results:
 *	The return value is 1 if file "f" appears to start with
 *	a valid GIF header, 0 otherwise.  If the header is valid,
 *	then *widthPtr and *heightPtr are modified to hold the
 *	dimensions of the image.
 *
 * Side effects:
 *	The access position in f advances.
 *
 *----------------------------------------------------------------------
 */

static int
ReadGIFHeader(f, widthPtr, heightPtr)
    FILE *f;			/* Image file to read the header from */
    int *widthPtr, *heightPtr;	/* The dimensions of the image are
				 * returned here. */
{
    unsigned char buf[7];

    if ((fread(buf, 1, 6, f) != 6)
	    || ((strncmp("GIF87a", (char *) buf, 6) != 0)
	    && (strncmp("GIF89a", (char *) buf, 6) != 0))) {
	return 0;
    }

    if (fread(buf, 1, 4, f) != 4) {
	return 0;
    }

    *widthPtr = LM_to_uint(buf[0],buf[1]);
    *heightPtr = LM_to_uint(buf[2],buf[3]);
    return 1;
}

/*
 *-----------------------------------------------------------------
 * The code below is copied from the giftoppm program and modified
 * just slightly.
 *-----------------------------------------------------------------
 */

static int
ReadColorMap(fd,number,buffer)
FILE        *fd;
int     number;
unsigned char   buffer[3][MAXCOLORMAPSIZE];
{
	int     i;
	unsigned char   rgb[3];

	for (i = 0; i < number; ++i) {
		if (! ReadOK(fd, rgb, sizeof(rgb)))
			return 0;

		buffer[CM_RED][i] = rgb[0] ;
		buffer[CM_GREEN][i] = rgb[1] ;
		buffer[CM_BLUE][i] = rgb[2] ;
	}
	return 1;
}



static int
DoExtension(fd, label, transparent)
FILE    *fd;
int label;
int	*transparent;
{
	static unsigned char buf[256];
	int count = 0;

	switch (label) {
		case 0x01:      /* Plain Text Extension */
			break;

		case 0xff:      /* Application Extension */
			break;

		case 0xfe:      /* Comment Extension */
			do {
				count = GetDataBlock(fd, (unsigned char*) buf);
			} while (count > 0);
			return count;

		case 0xf9:      /* Graphic Control Extension */
			count = GetDataBlock(fd, (unsigned char*) buf);
			if (count < 0) {
				return 1;
			}
			if ((buf[0] & 0x1) != 0) {
				*transparent = buf[3];
			}

			do {
			    count = GetDataBlock(fd, (unsigned char*) buf);
			} while (count > 0);
			return count;
	}

	do {
	    count = GetDataBlock(fd, (unsigned char*) buf);
	} while (count > 0);
	return count;
}

static int ZeroDataBlock = 0;

static int
GetDataBlock(fd, buf)
FILE        *fd;
unsigned char   *buf;
{
	unsigned char   count;

	if (! ReadOK(fd,&count,1)) {
		return -1;
	}

	ZeroDataBlock = count == 0;

	if ((count != 0) && (! ReadOK(fd, buf, count))) {
		return -1;
	}

	return count;
}


static int
ReadImage(interp, imagePtr, fd, len, height, cmap, interlace, transparent)
Tcl_Interp *interp;
char 	*imagePtr;
FILE    *fd;
int len, height;
unsigned char   cmap[3][MAXCOLORMAPSIZE];
int interlace;
int transparent;
{
	unsigned char   c;
	int     v;
	int     xpos = 0, ypos = 0, pass = 0;
	char 	*colStr;


	/*
	 *  Initialize the Compression routines
	 */
	if (! ReadOK(fd,&c,1))  {
	    Tcl_AppendResult(interp, "error reading GIF image: ",
		    Tcl_PosixError(interp), (char *) NULL);
	    return TCL_ERROR;
	}

	if (LWZReadByte(fd, 1, c) < 0) {
	    interp->result = "format error in GIF image";
	    return TCL_ERROR;
	}

	if (transparent!=-1 && 
		(colStr = Tcl_GetVar(interp, "TRANSPARENT_GIF_COLOR", 0L))) {
		XColor *colorPtr;
		colorPtr = Tk_GetColor(interp, Tk_MainWindow(interp), 
							  Tk_GetUid(colStr));
		if (colorPtr) {
/*
			printf("color is %d %d %d\n", 
					colorPtr->red >> 8, 
					colorPtr->green >> 8, 
					colorPtr->blue >> 8);
*/
			cmap[CM_RED][transparent] = colorPtr->red >> 8;
			cmap[CM_GREEN][transparent] = colorPtr->green >> 8;
			cmap[CM_BLUE][transparent] = colorPtr->blue >> 8;
			Tk_FreeColor(colorPtr);
		}
	}

	while ((v = LWZReadByte(fd,0,c)) >= 0 ) {

		imagePtr[ (xpos*3)  +  (ypos *len*3)] = cmap[CM_RED][v];
		imagePtr[ (xpos*3)  +  (ypos *len*3) +1] = cmap[CM_GREEN][v];
		imagePtr[ (xpos*3)  +  (ypos *len*3) +2] = cmap[CM_BLUE][v];

		++xpos;
		if (xpos == len) {
			xpos = 0;
			if (interlace) {
				switch (pass) {
					case 0:
					case 1:
						ypos += 8; break;
					case 2:
						ypos += 4; break;
					case 3:
						ypos += 2; break;
				}

				if (ypos >= height) {
					++pass;
					switch (pass) {
						case 1:
							ypos = 4; break;
						case 2:
							ypos = 2; break;
						case 3:
							ypos = 1; break;
						default:
							return TCL_OK;
					}
				}
			} else {
				++ypos;
			}
		}
		if (ypos >= height)
			break;
	}
	return TCL_OK;
}

static int
LWZReadByte(fd, flag, input_code_size)
FILE    *fd;
int flag;
int input_code_size;
{
	static int  fresh = 0;
	int     code, incode;
	static int  code_size, set_code_size;
	static int  max_code, max_code_size;
	static int  firstcode, oldcode;
	static int  clear_code, end_code;
	static int  table[2][(1<< MAX_LWZ_BITS)];
	static int  stack[(1<<(MAX_LWZ_BITS))*2], *sp;
	register int    i;


	if (flag) {

		set_code_size = input_code_size;
		code_size = set_code_size+1;
		clear_code = 1 << set_code_size ;
		end_code = clear_code + 1;
		max_code_size = 2*clear_code;
		max_code = clear_code+2;

		GetCode(fd, 0, 1);

		fresh = 1;

		for (i = 0; i < clear_code; ++i) {
			table[0][i] = 0;
			table[1][i] = i;
		}
		for (; i < (1<<MAX_LWZ_BITS); ++i) {
			table[0][i] = table[1][0] = 0;
		}

		sp = stack;

		return 0;

	} else if (fresh) {

		fresh = 0;
		do {
			firstcode = oldcode = GetCode(fd, code_size, 0);
		} while (firstcode == clear_code);
		return firstcode;
	}

	if (sp > stack)
		return *--sp;

	while ((code = GetCode(fd, code_size, 0)) >= 0) {
		if (code == clear_code) {
			for (i = 0; i < clear_code; ++i) {
				table[0][i] = 0;
				table[1][i] = i;
			}

			for (; i < (1<<MAX_LWZ_BITS); ++i) {
				table[0][i] = table[1][i] = 0;
			}

			code_size = set_code_size+1;
			max_code_size = 2*clear_code;
			max_code = clear_code+2;
			sp = stack;
			firstcode = oldcode = GetCode(fd, code_size, 0);
			return firstcode;

	} else if (code == end_code) {
		int     count;
		unsigned char   buf[260];

		if (ZeroDataBlock)
			return -2;

		while ((count = GetDataBlock(fd, buf)) > 0)
			;

		if (count != 0)
			return -2;
	}

	incode = code;

	if (code >= max_code) {
		*sp++ = firstcode;
		code = oldcode;
	}

	while (code >= clear_code) {
		*sp++ = table[1][code];
		if (code == table[0][code]) {
			return -2;

			/*
			 * Used to be this instead, Steve Ball suggested
			 * the change to just return.

			printf("circular table entry BIG ERROR\n");
			*/
		}
		code = table[0][code];
	}

	*sp++ = firstcode = table[1][code];

	if ((code = max_code) <(1<<MAX_LWZ_BITS)) {

		table[0][code] = oldcode;
		table[1][code] = firstcode;
		++max_code;
		if ((max_code>=max_code_size) && (max_code_size < (1<<MAX_LWZ_BITS))) {
			max_code_size *= 2;
			++code_size;
		}
	}

	oldcode = incode;

	if (sp > stack)
		return *--sp;
	}
	return code;
}


static int
GetCode(fd, code_size, flag)
FILE    *fd;
int code_size;
int flag;
{
	static unsigned char    buf[280];
	static int      curbit, lastbit, done, last_byte;
	int         i, j, ret;
	unsigned char       count;

	if (flag) {
		curbit = 0;
		lastbit = 0;
		done = 0;
		return 0;
	}


	if ( (curbit+code_size) >= lastbit) {
		if (done) {
			/* ran off the end of my bits */
			return -1;
		}
		buf[0] = buf[last_byte-2];
		buf[1] = buf[last_byte-1];

		if ((count = GetDataBlock(fd, &buf[2])) == 0)
			done = 1;

		last_byte = 2 + count;
		curbit = (curbit - lastbit) + 16;
		lastbit = (2+count)*8 ;
	}

	ret = 0;
	for (i = curbit, j = 0; j < code_size; ++i, ++j)
		ret |= ((buf[ i / 8 ] & (1 << (i % 8))) != 0) << j;


	curbit += code_size;

	return ret;
}
