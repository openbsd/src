/*	$Id: netbsd.h,v 1.1.1.1 1995/10/18 08:39:53 deraadt Exp $ */

/* ZMAGIC files never have the header in the text.  */
#define	N_HEADER_IN_TEXT(x)	0

/* ZMAGIC files start at address 0.  This does not apply to QMAGIC.  */
#define TEXT_START_ADDR 0

#define N_MAGIC(ex) \
    ( (((ex).a_info)&0xffff0000) ? (ntohl(((ex).a_info))&0xffff) : ((ex).a_info))
#define N_MACHTYPE(ex) \
    ( (((ex).a_info)&0xffff0000) ? ((ntohl(((ex).a_info))>>16)&0x03ff) : 0 )
# define N_FLAGS(ex) \
    ( (((ex).a_info)&0xffff0000) ? ((ntohl(((ex).a_info))>>26)&0x3f) : 0 )
#define N_SET_INFO(ex, mag,mid,flag) \
    ( (ex).a_info = htonl( (((flag)&0x3f)<<26) | (((mid)&0x03ff)<<16) | \
    (((mag)&0xffff)) ) )
#define N_SET_MAGIC(exec,magic) \
  ((exec).a_info = (((exec).a_info & ~0xffff) | ((magic) & 0xffff)))
#define N_SET_MACHTYPE(exec,machtype) \
  ((exec).a_info = \
   (((exec).a_info & ~(0x3ff<<16)) | (((machtype)&0xff) << 16)))
#define N_SET_FLAGS(exec, flags) \
  ((exec).a_info = ((exec).a_info & 0xffff) | (flags & 0xffff))

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "libaout.h"

#define N_GETMAGIC(ex) \
    ( (((ex).a_info)&0xffff0000) ? (ntohl(((ex).a_info))&0xffff) : ((ex).a_info))
#define N_GETMAGIC2(ex) \
    ( (((ex).a_info)&0xffff0000) ? (ntohl(((ex).a_info))&0xffff) : \
    (((ex).a_info) | 0x10000) )

#define N_TXTADDR(ex)	(N_GETMAGIC2(ex) == (ZMAGIC|0x10000) ? 0 : __LDPGSZ)
#define N_TXTOFF(ex) \
	( N_GETMAGIC2(ex)==ZMAGIC || N_GETMAGIC2(ex)==(QMAGIC|0x10000) ? \
	0 : (N_GETMAGIC2(ex)==(ZMAGIC|0x10000) ? __LDPGSZ : EXEC_BYTES_SIZE ))
#define N_ALIGN(ex,x) \
	(N_MAGIC(ex) == ZMAGIC || N_MAGIC(ex) == QMAGIC ? \
	 ((x) + __LDPGSZ - 1) & ~(__LDPGSZ - 1) : (x))
#define	N_DATADDR(ex) \
	(N_GETMAGIC(ex) == OMAGIC ? N_TXTADDR(ex) + (ex).a_text : \
		(N_TXTADDR(ex) + (ex).a_text + __LDPGSZ - 1) & ~(__LDPGSZ - 1))

/* Data segment offset. */
#define	N_DATOFF(ex) \
	N_ALIGN(ex, N_TXTOFF(ex) + (ex).a_text)

#define NO_SWAP_MAGIC	/* magic number already in correct endian format */

#include "aout-target.h"

