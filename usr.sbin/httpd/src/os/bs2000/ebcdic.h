#ifndef AP_EBCDIC_H
#define AP_EBCDIC_H  "$Id: ebcdic.h,v 1.3 2000/03/19 11:17:32 beck Exp $"

#include <sys/types.h>

extern const unsigned char os_toascii[256];
extern const unsigned char os_toebcdic[256];
void *ebcdic2ascii(void *dest, const void *srce, size_t count);
void *ascii2ebcdic(void *dest, const void *srce, size_t count);

#endif /*AP_EBCDIC_H*/
