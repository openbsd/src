/*                                            HTFormat: The format manager in the WWW Library
                            MANAGE DIFFERENT DOCUMENT FORMATS

   Here we describe the functions of the HTFormat module which handles conversion between
   different data representations.  (In MIME parlance, a representation is known as a
   content-type.  In WWW the term "format" is often used as it is shorter).

   This module is implemented by HTFormat.c.  This hypertext document is used to generate
   the HTFormat.h include file.  Part of the WWW library.

Preamble

 */
#ifndef HTFORMAT_H
#define HTFORMAT_H

#include <HTStream.h>
#include <HTAtom.h>
#include <HTList.h>

/*

The HTFormat type

   We use the HTAtom object for holding representations.  This allows faster manipulation
   (comparison and copying) that if we stayed with strings.

 */
typedef HTAtom * HTFormat;

/*

   These macros (which used to be constants) define some basic internally referenced
   representations.  The www/xxx ones are of course not MIME standard.

   www/source  is an output format which leaves the input untouched. It is useful for
   diagnostics, and for users who want to see the original, whatever it is.

 */
                        /* Internal ones */
#define WWW_SOURCE HTAtom_for("www/source")     /* Whatever it was originally*/

/*

   www/present represents the user's perception of the document.  If you convert to
   www/present, you present the material to the user.

 */
#define WWW_PRESENT HTAtom_for("www/present")   /* The user's perception */

/*

   The message/rfc822 format means a MIME message or a plain text message with no MIME
   header.  This is what is returned by an HTTP server.

 */
#define WWW_MIME HTAtom_for("www/mime")         /* A MIME message */

/*

   www/print is like www/present except it represents a printed copy.

 */
#define WWW_PRINT HTAtom_for("www/print")       /* A printed copy */

/*

   www/unknown is a really unknown type.  Some default action is appropriate.

 */
#define WWW_UNKNOWN     HTAtom_for("www/unknown")

#ifdef DIRED_SUPPORT
/*
   www/dired signals directory edit mode.
*/
#define WWW_DIRED      HTAtom_for("www/dired")
#endif

/*

   These are regular MIME types.  HTML is assumed to be added by the W3 code.
   application/octet-stream was mistakenly application/binary in earlier libwww versions
   (pre 2.11).

 */
#define WWW_PLAINTEXT   HTAtom_for("text/plain")
#define WWW_POSTSCRIPT  HTAtom_for("application/postscript")
#define WWW_RICHTEXT    HTAtom_for("application/rtf")
#define WWW_AUDIO       HTAtom_for("audio/basic")
#define WWW_HTML        HTAtom_for("text/html")
#define WWW_BINARY      HTAtom_for("application/octet-stream")

/*

   We must include the following file after defining HTFormat, to which it makes
   reference.

The HTEncoding type

 */
typedef HTAtom* HTEncoding;

/*

   The following are values for the MIME types:

 */
#define WWW_ENC_7BIT            HTAtom_for("7bit")
#define WWW_ENC_8BIT            HTAtom_for("8bit")
#define WWW_ENC_BINARY          HTAtom_for("binary")

/*

   We also add

 */
#define WWW_ENC_COMPRESS        HTAtom_for("compress")

/*
   Does a string designate a real encoding, or is it just
   a "dummy" as for example 7bit, 8bit, and binary?
  */
#define IsUnityEncStr(senc) \
        ((senc)==NULL || *(senc)=='\0' || !strcmp(senc,"identity") ||\
        !strcmp(senc,"8bit") || !strcmp(senc,"binary") || !strcmp(senc,"7bit"))

#define IsUnityEnc(enc) \
        ((enc)==NULL || (enc)==HTAtom_for("identity") ||\
        (enc)==WWW_ENC_8BIT || (enc)==WWW_ENC_BINARY || (enc)==WWW_ENC_7BIT)


#include <HTAnchor.h>

/*

The HTPresentation and HTConverter types

   This HTPresentation structure represents a possible conversion algorithm from one
   format to annother.  It includes a pointer to a conversion routine.  The conversion
   routine returns a stream to which data should be fed. See also HTStreamStack which
   scans the list of registered converters and calls one.  See the initialisation module
   for a list of conversion routines.

 */
typedef struct _HTPresentation HTPresentation;

typedef HTStream * HTConverter PARAMS((
        HTPresentation *        pres,
        HTParentAnchor *        anchor,
        HTStream *              sink));

struct _HTPresentation {
        HTAtom	*	rep;            /* representation name atmoized */
        HTAtom	*	rep_out;        /* resulting representation */
        HTConverter *	converter;  /* The routine to gen the stream stack */
        char *		command;        /* MIME-format string */
        float		quality;        /* Between 0 (bad) and 1 (good) */
        float		secs;
        float		secs_per_byte;
	long int	maxbytes;
};

/*

   The list of presentations is kept by this module.  It is also scanned by modules which
   want to know the set of formats supported. for example.

 */
extern HTList * HTPresentations;

/*

   The default presentation is used when no other is appriporate

 */
extern  HTPresentation* default_presentation;

/*

HTSetPresentation: Register a system command to present a format

  ON ENTRY,

  rep                     is the MIME - style format name

  command                 is the MAILCAP - style command template

  quality                 A degradation faction 0..1.0

  secs                    A limit on the time user will wait (0.0 for infinity)
  secs_per_byte

  maxbytes                A limit on the length acceptable as input (0 infinite)

 */
extern void HTSetPresentation PARAMS((
        CONST char *	representation,
        CONST char *	command,
        float		quality,
        float		secs,
        float		secs_per_byte,
	long int	maxbytes
));


/*

HTSetConversion:   Register a converstion routine

  ON ENTRY,

  rep_in                  is the content-type input

  rep_out                 is the resulting content-type

  converter               is the routine to make the stream to do it

 */

extern void HTSetConversion PARAMS((
        CONST char *    rep_in,
        CONST char *    rep_out,
        HTConverter *   converter,
        float           quality,
        float           secs,
        float           secs_per_byte,
	long int	maxbytes
));


/*

HTStreamStack:   Create a stack of streams

   This is the routine which actually sets up the conversion.  It currently checks only for
   direct conversions, but multi-stage conversions are forseen.  It takes a stream into
   which the output should be sent in the final format, builds the conversion stack, and
   returns a stream into which the data in the input format should be fed.  The anchor is
   passed because hypertxet objects load information into the anchor object which
   represents them.

 */
extern HTStream * HTStreamStack PARAMS((
        HTFormat                format_in,
        HTFormat                format_out,
        HTStream*               stream_out,
        HTParentAnchor*         anchor));

/*
HTReorderPresentation: put presentation near head of list

    Look up a presentation (exact match only) and, if found, reorder
    it to the start of the HTPresentations list. - kw
    */

extern void HTReorderPresentation PARAMS((
        HTFormat                format_in,
        HTFormat                format_out));

/*

HTStackValue: Find the cost of a filter stack

   Must return the cost of the same stack which HTStreamStack would set up.

  ON ENTRY,

  format_in               The fomat of the data to be converted

  format_out              The format required

  initial_value           The intrinsic "value" of the data before conversion on a scale
                         from 0 to 1

  length                  The number of bytes expected in the input format

 */
extern float HTStackValue PARAMS((
        HTFormat                format_in,
        HTFormat                rep_out,
        float                   initial_value,
        long int                length));

#define NO_VALUE_FOUND  -1e20           /* returned if none found */

/*	Display the page while transfer in progress
**	-------------------------------------------
**
**   Repaint the page only when necessary.
**   This is a traverse call for HText_pageDispaly() - it works!.
**
*/
extern void HTDisplayPartial NOPARAMS;

extern void HTFinishDisplayPartial NOPARAMS;

/*

HTCopy:  Copy a socket to a stream

   This is used by the protocol engines to send data down a stream, typically one which
   has been generated by HTStreamStack.

 */
extern int HTCopy PARAMS((
	HTParentAnchor *	anchor,
        int                     file_number,
	void*			handle,
        HTStream*               sink));


/*

HTFileCopy:  Copy a file to a stream

   This is used by the protocol engines to send data down a stream, typically one which
   has been generated by HTStreamStack.  It is currently called by HTParseFile

 */
extern int HTFileCopy PARAMS((
        FILE*                   fp,
        HTStream*               sink));


#ifdef SOURCE_CACHE
#include <HTChunk.h>
/*

HTMemCopy:  Copy a memory chunk to a stream

   This is used by the protocol engines to send data down a stream, typically one which
   has been generated by HTStreamStack.  It is currently called by HTParseMem

 */
extern int HTMemCopy PARAMS((
	HTChunk *		chunk,
	HTStream*		sink));
#endif


/*

HTCopyNoCR: Copy a socket to a stream, stripping CR characters.

   It is slower than HTCopy .

 */

extern void HTCopyNoCR PARAMS((
	HTParentAnchor *	anchor,
        int                     file_number,
        HTStream*               sink));


/*

Clear input buffer and set file number

   This routine and the one below provide simple character input from sockets. (They are
   left over from the older architecure and may not be used very much.)  The existence of
   a common routine and buffer saves memory space in small implementations.

 */
extern void HTInitInput PARAMS((int file_number));

/*

Get next character from buffer

 */
extern int interrupted_in_htgetcharacter;
extern int HTGetCharacter NOPARAMS;


/*

HTParseSocket: Parse a socket given its format

   This routine is called by protocol modules to load an object.  uses HTStreamStack and
   the copy routines above.  Returns HT_LOADED if succesful, <0 if not.

 */
extern int HTParseSocket PARAMS((
        HTFormat        format_in,
        HTFormat        format_out,
        HTParentAnchor  *anchor,
        int             file_number,
        HTStream*       sink));

/*

HTParseFile: Parse a File through a file pointer

   This routine is called by protocols modules to load an object.  uses
   HTStreamStack and HTFileCopy.  Returns HT_LOADED if successful, can also
   return HT_PARTIAL_CONTENT, HT_NO_DATA, or other <0 for failure.

 */
extern int HTParseFile PARAMS((
        HTFormat        format_in,
        HTFormat        format_out,
        HTParentAnchor  *anchor,
        FILE            *fp,
        HTStream*       sink));

#ifdef SOURCE_CACHE
/*

HTParseMem: Parse a document in memory

   This routine is called by protocols modules to load an object.  uses
   HTStreamStack and HTMemCopy.  Returns HT_LOADED if successful, can also
   return <0 for failure.

 */
extern int HTParseMem PARAMS((
	HTFormat	format_in,
	HTFormat	format_out,
	HTParentAnchor	*anchor,
	HTChunk*	chunk,
	HTStream*	sink));
#endif

#ifdef USE_ZLIB

#ifdef USE_ZLIB
#include <zlib.h>
#endif /* USE_ZLIB */
/*
HTParseGzFile: Parse a gzipped File through a file pointer

   This routine is called by protocols modules to load an object.  uses
   HTStreamStack and HTGzFileCopy.  Returns HT_LOADED if successful, can also
   return HT_PARTIAL_CONTENT, HT_NO_DATA, or other <0 for failure.
 */
extern int HTParseGzFile PARAMS((
        HTFormat        format_in,
        HTFormat        format_out,
        HTParentAnchor  *anchor,
        gzFile          gzfp,
        HTStream*       sink));

#endif /* USE_ZLIB */

/*

HTNetToText: Convert Net ASCII to local representation

   This is a filter stream suitable for taking text from a socket and passing it into a
   stream which expects text in the local C representation.  It does ASCII and newline
   conversion.  As usual, pass its output stream to it when creating it.

 */
extern HTStream *  HTNetToText PARAMS ((HTStream * sink));

/*

HTFormatInit: Set up default presentations and conversions

   These are defined in HTInit.c or HTSInit.c if these have been replaced. If you don't
   call this routine, and you don't define any presentations, then this routine will
   automatically be called the first time a conversion is needed. However, if you
   explicitly add some conversions (eg using HTLoadRules) then you may want also to
   explicitly call this to get the defaults as well.

 */
extern void HTFormatInit NOPARAMS;

/*

Epilogue

 */
extern BOOL HTOutputSource;     /* Flag: shortcut parser */
#endif

/*

   end */
