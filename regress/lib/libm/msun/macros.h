/*      $OpenBSD: macros.h,v 1.1 2021/10/22 18:00:22 mbuhl Exp $       */
/* Public domain - Moritz Buhl */

#define __FBSDID(a)
#define rounddown(x, y)	(((x)/(y))*(y))
#define fpequal(a, b)	fpequal_cs(a, b, 1)
#define hexdump(...)
