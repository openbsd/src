#ifndef HTSAVETOFILE_H
#define HTSAVETOFILE_H

#ifndef HTUTILS_H
#include <HTUtils.h>
#endif

#include <HTStream.h>
#include <HTFormat.h>

#ifdef __cplusplus
extern "C" {
#endif
    extern HTStream *HTSaveToFile(HTPresentation *pres,
				  HTParentAnchor *anchor,
				  HTStream *sink);

    extern HTStream *HTDumpToStdout(HTPresentation *pres,
				    HTParentAnchor *anchor,
				    HTStream *sink);

    extern HTStream *HTCompressed(HTPresentation *pres,
				  HTParentAnchor *anchor,
				  HTStream *sink);

#ifdef __cplusplus
}
#endif
#endif				/* HTSAVETOFILE_H */
