/*	$Id: netbsd_sparc.c,v 1.1.1.1 1995/10/18 08:39:55 deraadt Exp $ */

#define TARGET_IS_BIG_ENDIAN_P
#define HOST_BIG_ENDIAN_P
#define	BYTES_IN_WORD	4
#define	ARCH	32

#define __LDPGSZ	8192
#define	PAGE_SIZE	__LDPGSZ
#define	SEGMENT_SIZE	__LDPGSZ
#define MID_SPARC	138
#define	DEFAULT_ARCH	bfd_arch_sparc

#define MACHTYPE_OK(mtype) ((mtype) == MID_SPARC || (mtype) == M_UNKNOWN)

#define MY(OP) CAT(netbsd_sparc_,OP)
/* This needs to start with a.out so GDB knows it is an a.out variant.  */
#define TARGETNAME "a.out-netbsd-sparc"

#include "netbsd.h"

