/*							File access in libwww
**				FILE ACCESS
**
**  These are routines for local file access used by WWW browsers and servers.
**  Implemented by HTFile.c.
**
**  If the file is not a local file, then we pass it on to HTFTP in case it
**  can be reached by FTP.
*/
#ifndef HTFILE_H
#define HTFILE_H

#include "HTFormat.h"
#include "HTAccess.h"
#ifndef HTML_H
#include "HTML.h"               /* SCW */
#endif /* HTML_H */

/*
**  Controlling globals
**
**  These flags control how directories and files are represented as
**  hypertext, and are typically set by the application from command
**  line options, etc.
*/
extern int HTDirAccess;         /* Directory access level */

#define HT_DIR_FORBID           0       /* Altogether forbidden */
#define HT_DIR_SELECTIVE        1       /* If HT_DIR_ENABLE_FILE exists */
#define HT_DIR_OK               2       /* Any accesible directory */

#define HT_DIR_ENABLE_FILE      ".www_browsable" /* If exists, can browse */

extern int HTDirReadme;         /* Include readme files in listing? */
                                        /* Values: */
#define HT_DIR_README_NONE      0       /* No */
#define HT_DIR_README_TOP       1       /* Yes, first */
#define HT_DIR_README_BOTTOM    2       /* Yes, at the end */

#define HT_DIR_README_FILE              "README"

/*
**  Convert filenames between local and WWW formats
*/
extern char * HTLocalName PARAMS((CONST char * name));

/*
**  Make a WWW name from a full local path name
*/
extern char * WWW_nameOfFile PARAMS((const char * name));

/*
**  Generate the name of a cache file
*/
extern char * HTCacheFileName PARAMS((CONST char * name));

/*
**  Output directory titles
**
** This is (like the next one) used by HTFTP. It is common code to generate
** the title and heading 1 and the parent directory link for any anchor.
**
** changed to return TRUE if parent directory link was generated,
** FALSE otherwise - KW
*/
extern BOOL HTDirTitles PARAMS((
        HTStructured *  target,
        HTAnchor *      anchor,
	BOOL		tildeIsTop));

/*	Load a document.
**	----------------
*/
extern int HTLoadFile PARAMS((
	CONST char *		addr,
	HTParentAnchor *	anchor,
	HTFormat		format_out,
	HTStream *		sink));

/*
**  Output a directory entry
**
** This is used by HTFTP.c for example -- it is a common routine for
**  generating a linked directory entry.
*/
extern void HTDirEntry PARAMS((
        HTStructured *  target,         /* in which to put the linked text */
        CONST char *    tail,           /* last part of directory name */
        CONST char *    entry));        /* name of this entry */

/*
**  HTSetSuffix: Define the representation for a file suffix
**
**  This defines a mapping between local file suffixes and file content
**  types and encodings.
**
**  ON ENTRY,
**
**  suffix		includes the "." if that is important (normally, yes!)
**
**  representation	is MIME-style content-type
**
**  encoding		is MIME-style content-transfer-encoding
**			(8bit, 7bit, etc) or HTTP-style content-encoding
**			(gzip, compress etc.)
**
**  quality		an a priori judgement of the quality of such files
**			(0.0..1.0)
**
**  HTSetSuffix5 has one more parameter for a short description of the type
**  which is otherwise derived from the representation:
**
**  desc		is a short textual description, or NULL
**
**  Examples:   HTSetSuffix(".ps", "application/postscript", "8bit", 1.0);
**  Examples:   HTSetSuffix(".psz", "application/postscript", "gzip", 1.0);
**  A MIME type could also indicate a non-trivial encoding on its own
**  ("application/x-compressed-tar"), but in that case don't use enconding
**  to also indicate it but use "binary" etc.
*/
extern void HTSetSuffix5 PARAMS((
        CONST char *    suffix,
        CONST char *    representation,
        CONST char *    encoding,
        CONST char *    desc,
        float           quality));

#define HTSetSuffix(suff,rep,enc,q) HTSetSuffix5(suff, rep, enc, NULL, q)

/*
**  HTFileFormat: Get Representation and Encoding from file name.
**
**  ON EXIT,
**
**  return		The represntation it imagines the file is in.
**
**  *pEncoding		The encoding (binary, 7bit, etc). See HTSetSuffix.
*/
extern HTFormat HTFileFormat PARAMS((
	CONST char *		filename,
	HTAtom **		pEncoding,
	CONST char **		pDesc));

/*
**  HTCharsetFormat: Revise the file format in relation to the Lynx charset.
**
**  This checks the format associated with an anchor for
**  for an extended MIME Content-Type, and if a charset is
**  indicated, sets Lynx up for proper handling in relation
**  to the currently selected character set. - FM
*/
extern HTFormat HTCharsetFormat PARAMS((
	HTFormat		format,
	HTParentAnchor *	anchor,
	int			default_LYhndl));

/*
**  Determine file value from file name.
*/
extern float HTFileValue PARAMS((
	CONST char *	filename));

/*
**  Determine write access to a file.
**
**  ON EXIT,
**
**  return value	YES if file can be accessed and can be written to.
**
**  BUGS
**
**   Isn't there a quicker way?
*/
extern BOOL HTEditable PARAMS((CONST char * filename));

/*	Make a save stream.
**	-------------------
*/
extern HTStream * HTFileSaveStream PARAMS((
	HTParentAnchor *	anchor));

/*
** Determine a suitable suffix, given the representation.
**
**  ON ENTRY,
**
**  rep			is the atomized MIME style representation
**  enc			is an encoding (8bit, binary, gzip, compress,..)
**
**  ON EXIT,
**
**  returns		a pointer to a suitable suffix string if one has
**			been found, else NULL.
*/
extern CONST char * HTFileSuffix PARAMS((
                HTAtom* rep,
                CONST char* enc));

/*
**  The Protocols
*/
#ifdef GLOBALREF_IS_MACRO
extern GLOBALREF (HTProtocol,HTFTP);
extern GLOBALREF (HTProtocol,HTFile);
#else
GLOBALREF HTProtocol HTFTP, HTFile;
#endif /* GLOBALREF_IS_MACRO */
#endif /* HTFILE_H */

/* end of HTFile */
