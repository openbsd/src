#ifndef HTSAVETOFILE_H
#define HTSAVETOFILE_H

#ifndef HTUTILS_H
#include <HTUtils.h>
#endif

#include <HTStream.h>
#include <HTFormat.h>

extern HTStream * HTSaveToFile PARAMS((
        HTPresentation *        pres,
        HTParentAnchor *        anchor,
        HTStream *              sink));

extern HTStream * HTDumpToStdout PARAMS((
        HTPresentation *        pres,
        HTParentAnchor *        anchor,
        HTStream *              sink));

extern HTStream * HTCompressed PARAMS((
        HTPresentation *        pres,
        HTParentAnchor *        anchor,
        HTStream *              sink));

#endif /* HTSAVETOFILE_H */
