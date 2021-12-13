/*      $OpenBSD: macros.h,v 1.2 2021/12/13 16:56:48 deraadt Exp $       */
/* Public domain - Moritz Buhl */

#define __FBSDID(a)
#define rounddown(x, y)	(((x)/(y))*(y))
#define fpequal(a, b)	fpequal_cs(a, b, 1)
#define hexdump(...)

#define nitems(_a)      (sizeof((_a)) / sizeof((_a)[0]))
