
#include <stdarg.h>
#include "stand.h"

__dead void
panic(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    printf(fmt, ap);
    printf("\n");
    va_end(ap);
    _exit ();
}

