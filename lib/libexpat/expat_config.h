/* $OpenBSD: expat_config.h,v 1.2 2015/05/23 00:53:25 jsg Exp $ */

/* quick and dirty conf for OpenBSD */

#define HAVE_MEMMOVE 1
#define XML_CONTEXT_BYTES 1024
#define XML_DTD 1
#define XML_NS 1

#include <endian.h>
#if BYTE_ORDER == LITTLE_ENDIAN
#define BYTEORDER 1234
#elif BYTE_ORDER == BIG_ENDIAN
#define BYTEORDER 4321
#else
#error "unknown byte order"
#endif
