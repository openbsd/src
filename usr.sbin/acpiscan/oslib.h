/* $OpenBSD: oslib.h,v 1.1 2006/11/03 19:33:56 marco Exp $ */
/*
 * Copyright (c) 2006 Jordan Hargrave <jordan@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef __oslib_h__
#define __oslib_h__

/*=========== ( DOS 16-bit ) ============================= */
#ifdef MSDOS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stddef.h>
#include <dos.h>
#include <time.h>

#define FAR far
#define PACKED

typedef unsigned __int64        uint64_t;
typedef unsigned long           uint32_t;
typedef unsigned short          uint16_t;
typedef unsigned char           uint8_t;

#define ZeroMem(a,c)            _fmemset((void FAR *)a,0,c)
#define CopyMem(a,b,c)     	_fmemcpy((void FAR *)(a),(void FAR *)b,c)
#endif

/*==== ( OpenBSD user mode ) ============================= */
#ifdef __OpenBSD__
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <fcntl.h>
#include <time.h>
#include <machine/pio.h>
#include <i386/sysarch.h>

#define ZeroMem(buf,len)        bzero(buf,len)
#define CopyMem(dest,src,len)   bcopy(src,dest,len)

#define cpu_to_be16 be16toh
#define cpu_to_be32 be32toh
#define cpu_to_be64 be64toh
#define be16_to_cpu htobe16
#define be32_to_cpu htobe32
#define be64_to_cpu htobe64
#endif

/*==== ( FreeBSD user mode ) ============================= */
#ifdef __FreeBSD__
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdint.h>
#include <fcntl.h>
#include <time.h>

#define ZeroMem(buf,len)        bzero(buf,len)
#define CopyMem(dest,src,len)   bcopy(src,dest,len)
#endif

/*==== ( SCO UNIXWARE user mode ) ============================= */
#ifdef SCO
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <sys/sysi86.h>
#include <sys/inline.h>
#include <inttypes.h>

#define ZeroMem(buf,len)        bzero(buf,len)
#define CopyMem(dest,src,len)   bcopy(src,dest,len)
#endif

/*==== ( Solaris user mode ) ============================= */
#ifdef __sun__
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <sys/sysi86.h>
//#include <sys/inline.h>

#define ZeroMem(buf,len)        bzero(buf,len)
#define CopyMem(dest,src,len)   bcopy(src,dest,len)

#endif

/*==== ( Linux kernel mode ) ============================= */
#if defined(linux) && defined(__KERNEL__)
#include <linux/version.h>
#include <linux/module.h>
#include <linux/config.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/stddef.h>
#include <linux/sched.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/miscdevice.h>

#endif

/*==== ( Linux user mode ) ============================= */
#if defined(linux) && !defined(__KERNEL__)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stddef.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <sys/io.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <asm/page.h>
#include <errno.h>
#include <endian.h>
#include <byteswap.h>

#endif

/*====================================================*
 * Common code
 *====================================================*/
#ifndef FAR
# define FAR
#endif
#ifndef PACKED
# define PACKED __attribute__((packed))
#endif

extern void os_dump(const void FAR *, int);

/* ----==== Memory I/O ====---- */
extern void     set_physmemfile(const char FAR *, uint64_t);
extern int      physmemcpy(void FAR *dest, uint32_t src, size_t len);
extern int      physmemcmp(const void FAR *dest, uint32_t src, size_t len);
extern uint32_t scanmem(uint32_t src, uint32_t end, int step, int sz, 
		const void FAR *mem);

/* ----==== Port I/O ====---- */
extern void       set_iopl(int);
extern uint8_t    os_inb(uint16_t);
extern uint16_t   os_inw(uint16_t);
extern uint32_t   os_inl(uint16_t);
extern void       os_outb(uint16_t, uint8_t);
extern void       os_outw(uint16_t, uint16_t);
extern void       os_outl(uint16_t, uint32_t);

/* ----==== PCI ====---- */
extern int pci_read_n(int, int, int, int, int, void FAR *);

#define cpu_to_le16(x) (x)
#define cpu_to_le32(x) (x)
#define cpu_to_le64(x) (x)

#define le16_to_cpu(x) (x)
#define le32_to_cpu(x) (x)
#define le64_to_cpu(x) (x)

/* AABBCCDD
 *   xchg dh, al = DDBBCCAA
 *   xchg dl, ah = DDCCBBAA
 */
#ifdef WATCOM
#pragma aux cpu_to_be16 = "xchg ah, al" parm [ax] value [ax];
#pragma aux cpu_to_be32 = \
			  "xchg  dh, al" \
"xchg  dl, ah" \
parm caller [dx ax] value [dx ax];
#endif

#endif

