/*                                                                     File Writer for libwww
                                      C FILE WRITER

   It is useful to have both FWriter and Writer for environments in which fdopen() doesn't
   exist for example.

 */
#ifndef HTFWRITE_H
#define HTFWRITE_H

#include <HTStream.h>
#include <HTFormat.h>

extern HTStream * HTFWriter_new PARAMS((FILE * fp));

extern HTStream * HTSaveAndExecute PARAMS((
        HTPresentation *        pres,
        HTParentAnchor *        anchor, /* Not used */
        HTStream *              sink));

extern HTStream * HTSaveLocally PARAMS((
        HTPresentation *        pres,
        HTParentAnchor *        anchor, /* Not used */
        HTStream *              sink));

#endif
/*

   end */
