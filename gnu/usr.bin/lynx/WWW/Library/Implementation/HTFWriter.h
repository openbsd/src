/*                                                                     File Writer for libwww
                                      C FILE WRITER

   It is useful to have both FWriter and Writer for environments in which fdopen() doesn't
   exist for example.

 */
#ifndef HTFWRITE_H
#define HTFWRITE_H

#include <HTStream.h>
#include <HTFormat.h>

#ifdef __cplusplus
extern "C" {
#endif
    extern HTStream *HTFWriter_new(FILE *fp);

    extern HTStream *HTSaveAndExecute(HTPresentation *pres,
				      HTParentAnchor *anchor,	/* Not used */
				      HTStream *sink);

    extern HTStream *HTSaveLocally(HTPresentation *pres,
				   HTParentAnchor *anchor,	/* Not used */
				   HTStream *sink);

#ifdef __cplusplus
}
#endif
#endif				/* HTFWRITE_H */
