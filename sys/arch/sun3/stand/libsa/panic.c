/*	$OpenBSD: panic.c,v 1.4 2001/07/04 08:33:51 niklas Exp $	*/


#include <stdarg.h>
#include "stand.h"

extern __dead void abort();

__dead void
panic(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vprintf(fmt, ap);
	printf("\n");
	va_end(ap);
	abort();
}
