/*                                                                     File Writer for libwww
                                      C FILE WRITER
                                             
   It is useful to have both FWriter and Writer for environments in which fdopen() doesn't
   exist for example.
   
 */
#ifndef HTFWRITE_H
#define HTFWRITE_H

#ifndef HTUTILS_H
#include "HTUtils.h"
#endif /* HTUTILS_H */
#include "HTStream.h"
/*#include <stdio.h> included by HTUtils.h -- FM */
#include "HTFormat.h"

#ifdef SHORT_NAMES
#define HTFWriter_new   HTFWnew
#endif

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
