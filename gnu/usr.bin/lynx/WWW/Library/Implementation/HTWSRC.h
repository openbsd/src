/*                                                             A parser for WAIS source files
                                 WAIS SOURCE FILE PARSER

   This converter returns a stream object into which a WAIS source file can be
   written.  The result is put via a structured stream into whatever format was
   required for the output stream.

   See also:  HTWAIS protocol interface module

 */
#ifndef HTWSRC_H
#define HTWSRC_H

#include <HTFormat.h>

#ifdef __cplusplus
extern "C" {
#endif
    extern char from_hex(char c);

    extern HTStream *HTWSRCConvert(HTPresentation *pres,
				   HTParentAnchor *anchor,
				   HTStream *sink);

/*

Escaping Strings

   HTDeSlash takes out the invlaid characters in a URL path ELEMENT by
   converting them into hex-escaped characters.  HTEnSlash does the reverse.

   Each returns a pointer to a newly allocated string which must eventually be
   freed by the caller.

 */
    extern char *HTDeSlash(const char *str);

    extern char *HTEnSlash(const char *str);

#ifdef __cplusplus
}
#endif
#endif				/* HTWSRC_H */
