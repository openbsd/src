/*		Configuration-specific Initialialization	HTInit.c
**		----------------------------------------
*/

/*	Define a basic set of suffixes and presentations
**	------------------------------------------------
**
*/

#include "HTUtils.h"

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
#include "HTFWriter.h"

#include "LYLeaks.h"

PUBLIC void HTFormatInit NOARGS
{
#ifdef NeXT
    HTSetPresentation("application/postscript", "open %s", 1.0, 2.0, 0.0, 0);
    /* The following needs the GIF previewer -- you might not have it. */
    HTSetPresentation("image/gif", 		"open %s", 0.3, 2.0, 0.0, 0);
    HTSetPresentation("image/x-tiff", 		"open %s", 1.0, 2.0, 0.0, 0);
    HTSetPresentation("audio/basic", 		"open %s", 1.0, 2.0, 0.0, 0);
    HTSetPresentation("*", 			"open %s", 1.0, 0.0, 0.0, 0);
#else
    if (getenv("DISPLAY")) {	/* Must have X11 */
	HTSetPresentation("application/postscript", "ghostview %s",
							 1.0, 3.0, 0.0, 0);
	HTSetPresentation("image/gif", 		"xv %s", 1.0, 3.0, 0.0, 0);
	HTSetPresentation("image/x-tiff", 	"xv %s", 1.0, 3.0, 0.0, 0);
	HTSetPresentation("image/jpeg", 	"xv %s", 1.0, 3.0, 0.0, 0);
    }
#endif
    HTSetConversion("www/mime",			"*",		HTMIMEConvert,
    							1.0, 0.0, 0.0, 0);
    HTSetConversion("application/x-wais-source","*",		HTWSRCConvert,
    							1.0, 0.0, 0.0, 0);
    HTSetConversion("text/html",		"text/x-c",	HTMLToC,
    							0.5, 0.0, 0.0, 0);
    HTSetConversion("text/html",		"text/plain",	HTMLToPlain,
    							0.5, 0.0, 0.0, 0);
    HTSetConversion("text/html",		"www/present",	HTMLPresent,
    							1.0, 0.0, 0.0, 0);
    HTSetConversion("text/plain",		"text/html",	HTPlainToHTML,
    							1.0, 0.0, 0.0, 0);
    HTSetConversion("text/plain",		"www/present",	HTPlainPresent,
    							1.0, 0.0, 0.0, 0);
    HTSetConversion("application/octet-stream",	"www/present",	HTSaveLocally,
    							0.1, 0.0, 0.0, 0);
    HTSetConversion("www/unknown",		"www/present",	HTSaveLocally,
    							0.3, 0.0, 0.0, 0);
    HTSetConversion("www/source",		"www/present",	HTSaveLocally,
    							0.3, 0.0, 0.0, 0);
}



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

#ifndef NO_INIT
PUBLIC void HTFileInit NOARGS
{
    /*		Suffix     Contenet-Type	Content-Encoding  Quality			*/

    HTSetSuffix(".mime",   "www/mime",			"8bit",   1.0);	/* Internal -- MIME is	*/
                                                                        /* not recursive	*/
    HTSetSuffix(".bin",    "application/octet-stream",	"binary", 1.0); /* Uninterpreted binary	*/
    HTSetSuffix(".oda",    "application/oda",		"binary", 1.0);
    HTSetSuffix(".pdf",    "application/pdf",		"binary", 1.0);
    HTSetSuffix(".ai",     "application/postscript",	"8bit",   0.5);	/* Adobe Illustrator	*/
    HTSetSuffix(".PS",     "application/postscript",	"8bit",	  0.8);	/* PostScript		*/
    HTSetSuffix(".eps",    "application/postscript",	"8bit",   0.8);
    HTSetSuffix(".ps",     "application/postscript",	"8bit",   0.8);
    HTSetSuffix(".rtf",    "application/x-rtf",		"7bit",   1.0);	/* RTF			*/
    HTSetSuffix(".Z",      "application/x-compressed",	"binary", 1.0);	/* Compressed data	*/
    HTSetSuffix(".csh",    "application/x-csh",		"7bit",   0.5);	/* C-shell script	*/
    HTSetSuffix(".dvi",    "application/x-dvi",		"binary", 1.0);	/* TeX DVI		*/
    HTSetSuffix(".hdf",    "application/x-hdf",		"binary", 1.0);	/* NCSA HDF data file	*/
    HTSetSuffix(".latex",  "application/x-latex",	"8bit",   1.0);	/* LaTeX source		*/
    HTSetSuffix(".nc",     "application/x-netcdf",	"binary", 1.0);	/* Unidata netCDF data	*/
    HTSetSuffix(".cdf",    "application/x-netcdf",	"binary", 1.0);
    HTSetSuffix(".sh",     "application/x-sh",		"7bit",   0.5);	/* Shell-script		*/
    HTSetSuffix(".tcl",    "application/x-tcl",		"7bit",   0.5);	/* TCL-script		*/
    HTSetSuffix(".tex",    "application/x-tex",		"8bit",   1.0);	/* TeX source		*/
    HTSetSuffix(".texi",   "application/x-texinfo",	"7bit",   1.0);	/* Texinfo		*/
    HTSetSuffix(".texinfo","application/x-texinfo",	"7bit",   1.0);
    HTSetSuffix(".t",      "application/x-troff",	"7bit",   0.5);	/* Troff		*/
    HTSetSuffix(".roff",   "application/x-troff",	"7bit",   0.5);
    HTSetSuffix(".tr",     "application/x-troff",	"7bit",   0.5);
    HTSetSuffix(".man",    "application/x-troff-man",	"7bit",   0.5);	/* Troff with man macros*/
    HTSetSuffix(".me",     "application/x-troff-me",	"7bit",   0.5);	/* Troff with me macros	*/
    HTSetSuffix(".ms",     "application/x-troff-ms",	"7bit",   0.5);	/* Troff with ms macros	*/
    HTSetSuffix(".src",    "application/x-wais-source",	"7bit",   1.0);	/* WAIS source		*/
    HTSetSuffix(".zip",    "application/zip",		"binary", 1.0);	/* PKZIP		*/
    HTSetSuffix(".bcpio",  "application/x-bcpio",	"binary", 1.0);	/* Old binary CPIO	*/
    HTSetSuffix(".cpio",   "application/x-cpio",	"binary", 1.0);	/* POSIX CPIO		*/
    HTSetSuffix(".gtar",   "application/x-gtar",	"binary", 1.0);	/* Gnu tar		*/
    HTSetSuffix(".shar",   "application/x-shar",	"8bit",   1.0);	/* Shell archive	*/
    HTSetSuffix(".sv4cpio","application/x-sv4cpio",	"binary", 1.0);	/* SVR4 CPIO		*/
    HTSetSuffix(".sv4crc", "application/x-sv4crc",	"binary", 1.0);	/* SVR4 CPIO with CRC	*/
    HTSetSuffix(".tar",    "application/x-tar",		"binary", 1.0);	/* 4.3BSD tar		*/
    HTSetSuffix(".ustar",  "application/x-ustar",	"binary", 1.0);	/* POSIX tar		*/
    HTSetSuffix(".snd",    "audio/basic",		"binary", 1.0);	/* Audio		*/
    HTSetSuffix(".au",     "audio/basic",		"binary", 1.0);
    HTSetSuffix(".aiff",   "audio/x-aiff",		"binary", 1.0);
    HTSetSuffix(".aifc",   "audio/x-aiff",		"binary", 1.0);
    HTSetSuffix(".aif",    "audio/x-aiff",		"binary", 1.0);
    HTSetSuffix(".wav",    "audio/x-wav",		"binary", 1.0);	/* Windows+ WAVE format	*/
    HTSetSuffix(".gif",    "image/gif",			"binary", 1.0);	/* GIF			*/
    HTSetSuffix(".ief",    "image/ief",			"binary", 1.0);	/* Image Exchange fmt	*/
    HTSetSuffix(".jpg",    "image/jpeg",		"binary", 1.0);	/* JPEG			*/
    HTSetSuffix(".JPG",    "image/jpeg",		"binary", 1.0);
    HTSetSuffix(".JPE",    "image/jpeg",		"binary", 1.0);
    HTSetSuffix(".jpe",    "image/jpeg",		"binary", 1.0);
    HTSetSuffix(".JPEG",   "image/jpeg",		"binary", 1.0);
    HTSetSuffix(".jpeg",   "image/jpeg",		"binary", 1.0);
    HTSetSuffix(".tif",    "image/tiff",		"binary", 1.0);	/* TIFF			*/
    HTSetSuffix(".tiff",   "image/tiff",		"binary", 1.0);
    HTSetSuffix(".ras",    "image/cmu-raster",		"binary", 1.0);
    HTSetSuffix(".pnm",    "image/x-portable-anymap",	"binary", 1.0);	/* PBM Anymap format	*/
    HTSetSuffix(".pbm",    "image/x-portable-bitmap",	"binary", 1.0);	/* PBM Bitmap format	*/
    HTSetSuffix(".pgm",    "image/x-portable-graymap",	"binary", 1.0);	/* PBM Graymap format	*/
    HTSetSuffix(".ppm",    "image/x-portable-pixmap",	"binary", 1.0);	/* PBM Pixmap format	*/
    HTSetSuffix(".rgb",    "image/x-rgb",		"binary", 1.0);
    HTSetSuffix(".xbm",    "image/x-xbitmap",		"binary", 1.0);	/* X bitmap		*/
    HTSetSuffix(".xpm",    "image/x-xpixmap",		"binary", 1.0);	/* X pixmap format	*/
    HTSetSuffix(".xwd",    "image/x-xwindowdump",	"binary", 1.0);	/* X window dump (xwd)	*/
    HTSetSuffix(".html",   "text/html",			"8bit",   1.0);	/* HTML			*/
    HTSetSuffix(".c",      "text/plain",		"7bit",   0.5);	/* C source		*/
    HTSetSuffix(".h",      "text/plain",		"7bit",   0.5);	/* C headers		*/
    HTSetSuffix(".C",      "text/plain",		"7bit",   0.5);	/* C++ source		*/
    HTSetSuffix(".cc",     "text/plain",		"7bit",   0.5);	/* C++ source		*/
    HTSetSuffix(".hh",     "text/plain",		"7bit",   0.5);	/* C++ headers		*/
    HTSetSuffix(".m",      "text/plain",		"7bit",   0.5);	/* Objective-C source	*/
    HTSetSuffix(".f90",    "text/plain",		"7bit",   0.5);	/* Fortran 90 source	*/
    HTSetSuffix(".txt",    "text/plain",		"7bit",   0.5);	/* Plain text		*/
    HTSetSuffix(".rtx",    "text/richtext",		"7bit",   1.0);	/* MIME Richtext format	*/
    HTSetSuffix(".tsv",    "text/tab-separated-values",	"7bit",   1.0);	/* Tab-separated values	*/
    HTSetSuffix(".etx",    "text/x-setext",		"7bit",   0.9);	/* Struct Enchanced Txt	*/
    HTSetSuffix(".MPG",    "video/mpeg",		"binary", 1.0);	/* MPEG			*/
    HTSetSuffix(".mpg",    "video/mpeg",		"binary", 1.0);
    HTSetSuffix(".MPE",    "video/mpeg",		"binary", 1.0);
    HTSetSuffix(".mpe",    "video/mpeg",		"binary", 1.0);
    HTSetSuffix(".MPEG",   "video/mpeg",		"binary", 1.0);
    HTSetSuffix(".mpeg",   "video/mpeg",		"binary", 1.0);
    HTSetSuffix(".qt",     "video/quicktime",		"binary", 1.0);	/* QuickTime		*/
    HTSetSuffix(".mov",    "video/quicktime",		"binary", 1.0);
    HTSetSuffix(".avi",    "video/x-msvideo",		"binary", 1.0);	/* MS Video for Windows	*/
    HTSetSuffix(".movie",  "video/x-sgi-movie",		"binary", 1.0);	/* SGI "moviepalyer"	*/
    
    HTSetSuffix("*.*",     "application/octet-stream",	"binary", 0.1);
    HTSetSuffix("*",       "text/plain",		"7bit",   0.5);

}
#endif /* NO_INIT */

