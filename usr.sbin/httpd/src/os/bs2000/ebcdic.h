#ifndef AP_EBCDIC_H
#define AP_EBCDIC_H  "$Id: ebcdic.h,v 1.4 2000/12/15 22:18:27 beck Exp $"

#include <sys/types.h>

extern const unsigned char os_toascii[256];
extern const unsigned char os_toebcdic[256];
API_EXPORT(void *) ebcdic2ascii(void *dest, const void *srce, size_t count);
API_EXPORT(void *) ascii2ebcdic(void *dest, const void *srce, size_t count);

#endif /*AP_EBCDIC_H*/
