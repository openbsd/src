#ifndef HTSAVETOFILE_H
#define HTSAVETOFILE_H

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
