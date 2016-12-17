/* $OpenBSD: endian.h,v 1.1 2016/12/17 23:38:33 patrick Exp $ */

#ifndef _MACHINE_ENDIAN_H_
#define _MACHINE_ENDIAN_H_

#define __swap32md(x) __statement({                                     \
        __uint32_t __swap32md_x; 	                                 \
                                                                        \
        __asm ("rev %w0, %w1" : "=r" (__swap32md_x) : "r"(x));          \
        __swap32md_x;                                                   \
})

#define __swap64md(x) __statement({                                     \
        __uint64_t __swap64md_x;					\
                                                                        \
        __asm ("rev %x0, %x1" : "=r" (__swap64md_x) : "r"(x));          \
        __swap64md_x;                                                   \
})

#define __swap16md(x) __statement({                                     \
        __uint16_t __swap16md_x;					\
                                                                        \
        __asm ("rev16 %w0, %w1" : "=r" (__swap16md_x) : "r"(x));        \
        __swap16md_x;                                                   \
})

/* Tell sys/endian.h we have MD variants of the swap macros.  */
#define __HAVE_MD_SWAP


#define _BYTE_ORDER _LITTLE_ENDIAN
#define	__STRICT_ALIGNMENT

#ifndef __FROM_SYS__ENDIAN
#include <sys/endian.h>
#endif
#endif /* _MACHINE_ENDIAN_H_ */
