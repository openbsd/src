/*                              ENCODING TO PRINTABLE CHARACTERS

   File module provides functions HTUU_encode() and HTUU_decode() which convert
   a buffer of bytes to/from RFC 1113 printable encoding format.  This
   technique is similar to the familiar Unix uuencode format in that it maps 6
   binary bits to one ASCII character (or more aptly, 3 binary bytes to 4 ASCII
   characters).  However, RFC 1113 does not use the same mapping to printable
   characters as uuencode.

 */

#ifndef HTUU_H
#define HTUU_H

#ifndef HTUTILS_H
#include <HTUtils.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif
    extern int HTUU_encode(unsigned char *bufin,
			   unsigned int nbytes,
			   char *bufcoded);

    extern int HTUU_decode(char *bufcoded,
			   unsigned char *bufplain,
			   int outbufsize);

#ifdef __cplusplus
}
#endif
#endif				/* HTUU_H */
