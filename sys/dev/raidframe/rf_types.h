/*	$OpenBSD: rf_types.h,v 1.1 1999/01/11 14:29:54 niklas Exp $	*/
/*	$NetBSD: rf_types.h,v 1.2 1998/11/16 04:14:10 mycroft Exp $	*/
/*
 * rf_types.h
 */
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Jim Zelenka
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */
/***********************************************************
 *
 * rf_types.h -- standard types for RAIDframe
 *
 ***********************************************************/
/*
 * :  
 * Log: rf_types.h,v 
 * Revision 1.35  1996/08/09 18:48:29  jimz
 * correct mips definition
 *
 * Revision 1.34  1996/08/07  22:50:14  jimz
 * monkey with linux includes to get a good compile
 *
 * Revision 1.33  1996/08/07  21:09:28  jimz
 * add SGI mips stuff (note: 64-bit stuff may be wrong, I didn't have
 * a machine to test on)
 *
 * Revision 1.32  1996/08/06  22:24:27  jimz
 * add LINUX_I386
 *
 * Revision 1.31  1996/07/31  16:30:12  jimz
 * move in RF_LONGSHIFT
 *
 * Revision 1.30  1996/07/30  04:51:58  jimz
 * ultrix port
 *
 * Revision 1.29  1996/07/29  16:37:34  jimz
 * define DEC_OSF for osf/1 kernel
 *
 * Revision 1.28  1996/07/28  20:31:39  jimz
 * i386netbsd port
 * true/false fixup
 *
 * Revision 1.27  1996/07/27  23:36:08  jimz
 * Solaris port of simulator
 *
 * Revision 1.26  1996/07/27  18:40:24  jimz
 * cleanup sweep
 *
 * Revision 1.25  1996/07/22  19:52:16  jimz
 * switched node params to RF_DagParam_t, a union of
 * a 64-bit int and a void *, for better portability
 * attempted hpux port, but failed partway through for
 * lack of a single C compiler capable of compiling all
 * source files
 *
 * Revision 1.24  1996/07/18  22:57:14  jimz
 * port simulator to AIX
 *
 * Revision 1.23  1996/07/15  17:22:18  jimz
 * nit-pick code cleanup
 * resolve stdlib problems on DEC OSF
 *
 * Revision 1.22  1996/06/11  18:11:57  jimz
 * add ThreadGroup
 *
 * Revision 1.21  1996/06/11  10:58:47  jimz
 * add RF_ReconDoneProc_t
 *
 * Revision 1.20  1996/06/10  14:18:58  jimz
 * move user, throughput stats into per-array structure
 *
 * Revision 1.19  1996/06/10  11:55:47  jimz
 * Straightened out some per-array/not-per-array distinctions, fixed
 * a couple bugs related to confusion. Added shutdown lists. Removed
 * layout shutdown function (now subsumed by shutdown lists).
 *
 * Revision 1.18  1996/06/07  21:33:04  jimz
 * begin using consistent types for sector numbers,
 * stripe numbers, row+col numbers, recon unit numbers
 *
 * Revision 1.17  1996/06/05  19:38:32  jimz
 * fixed up disk queueing types config
 * added sstf disk queueing
 * fixed exit bug on diskthreads (ref-ing bad mem)
 *
 * Revision 1.16  1996/06/05  18:06:02  jimz
 * Major code cleanup. The Great Renaming is now done.
 * Better modularity. Better typing. Fixed a bunch of
 * synchronization bugs. Made a lot of global stuff
 * per-desc or per-array. Removed dead code.
 *
 * Revision 1.15  1996/06/03  23:28:26  jimz
 * more bugfixes
 * check in tree to sync for IPDS runs with current bugfixes
 * there still may be a problem with threads in the script test
 * getting I/Os stuck- not trivially reproducible (runs ~50 times
 * in a row without getting stuck)
 *
 * Revision 1.14  1996/06/02  17:31:48  jimz
 * Moved a lot of global stuff into array structure, where it belongs.
 * Fixed up paritylogging, pss modules in this manner. Some general
 * code cleanup. Removed lots of dead code, some dead files.
 *
 * Revision 1.13  1996/05/31  22:26:54  jimz
 * fix a lot of mapping problems, memory allocation problems
 * found some weird lock issues, fixed 'em
 * more code cleanup
 *
 * Revision 1.12  1996/05/30  23:22:16  jimz
 * bugfixes of serialization, timing problems
 * more cleanup
 *
 * Revision 1.11  1996/05/30  11:29:41  jimz
 * Numerous bug fixes. Stripe lock release code disagreed with the taking code
 * about when stripes should be locked (I made it consistent: no parity, no lock)
 * There was a lot of extra serialization of I/Os which I've removed- a lot of
 * it was to calculate values for the cache code, which is no longer with us.
 * More types, function, macro cleanup. Added code to properly quiesce the array
 * on shutdown. Made a lot of stuff array-specific which was (bogusly) general
 * before. Fixed memory allocation, freeing bugs.
 *
 * Revision 1.10  1996/05/27  18:56:37  jimz
 * more code cleanup
 * better typing
 * compiles in all 3 environments
 *
 * Revision 1.9  1996/05/24  22:17:04  jimz
 * continue code + namespace cleanup
 * typed a bunch of flags
 *
 * Revision 1.8  1996/05/24  04:28:55  jimz
 * release cleanup ckpt
 *
 * Revision 1.7  1996/05/24  01:59:45  jimz
 * another checkpoint in code cleanup for release
 * time to sync kernel tree
 *
 * Revision 1.6  1996/05/23  21:46:35  jimz
 * checkpoint in code cleanup (release prep)
 * lots of types, function names have been fixed
 *
 * Revision 1.5  1996/05/23  00:33:23  jimz
 * code cleanup: move all debug decls to rf_options.c, all extern
 * debug decls to rf_options.h, all debug vars preceded by rf_
 *
 * Revision 1.4  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.3  1996/05/10  16:22:46  jimz
 * RF_offset -> RF_Offset
 * add RF_SectorCount
 *
 * Revision 1.2  1996/05/02  14:58:50  jimz
 * switch to _t for non-base-integral types
 *
 * Revision 1.1  1995/12/14  18:36:51  jimz
 * Initial revision
 *
 */

#ifndef _RF__RF_TYPES_H_
#define _RF__RF_TYPES_H_


#ifdef _KERNEL
#define KERNEL
#endif

#include "rf_archs.h"

#ifndef KERNEL
#ifdef LINUX
#include <stdlib.h>
#include <sys/types.h>
#endif /* LINUX */
#include <fcntl.h>
#include <stdio.h>

#ifdef __osf__
/*
 * The following monkeying is to get around some problems with
 * conflicting definitions in /usr/include/random.h and /usr/include/stdlib.h
 * on Digital Unix. They
 * (1) define the same symbols
 * (2) differently than one another
 * (3) also differently from the DU libc sources
 * This loses, bad.
 */
#include <standards.h>
#include <cma.h>
#ifdef _OSF_SOURCE
#undef _OSF_SOURCE
#define _RF_SPANKME
#endif /* _OSF_SOURCE */
#endif /* __osf__ */
#include <stdlib.h>
#ifdef __osf__
#ifdef _RF_SPANKME
#undef _RF_SPANKME
#define _OSF_SOURCE
#endif /* _RF_SPANKME */
#endif /* __osf__ */

#include <string.h>
#include <unistd.h>
#endif /* !KERNEL */
#include <sys/errno.h>
#include <sys/types.h>

#ifdef AIX
#include <sys/stream.h>
#endif /* AIX */

#if defined(hpux) || defined(__hpux)
/*
 * Yeah, we get one of hpux or __hpux, but not both. This is because
 * HP didn't really want to provide an ANSI C compiler. Apparantly, they
 * don't like standards. This explains a lot about their API. You might
 * try using gcc, but you'll discover that it's sufficiently buggy that
 * it can't even compile the core library.
 *
 * Hatred update: c89, the one thing which could both handle prototypes,
 * and compile /usr/include/sys/timeout.h, can't do 64-bit ints.
 *
 * Note: the hpux port is incomplete. Why? Well, because I can't find
 * a working C compiler. I've tried cc (both with and without -Ae),
 * c89, and gcc, all with and without -D_HPUX_SOURCE. Sod it.
 *
 * -Jim Zelenka, 22 July 1996
 */
#ifndef hpux
#define hpux
#endif /* !hpux */
#include <sys/hpibio.h>
#endif /* hpux || __hpux*/

#ifdef sun
#ifndef KERNEL
#include <errno.h>
#endif /* !KERNEL */
#endif /* sun */

#if defined(OSF) && defined(__alpha) && defined(KERNEL)
#ifndef DEC_OSF
#define DEC_OSF
#endif /* !DEC_OSF */
#endif /* OSF && __alpha && KERNEL */

#if (defined(__NetBSD__) || defined(__OpenBSD__)) && defined(KERNEL)
#include <sys/uio.h>
#include <sys/param.h>
#include <sys/lock.h>

/* XXX not sure about these... */
/* #define PZERO 0 */ /* actually defined in <sys/param.h> */
#define MS_LOCK_SIMPLE 1

#define TRUE 1  /* XXX why isn't this done somewhere already!! */

#endif /* (__NetBSD__ || __OpenBSD__) && KERNEL */

/*
 * First, define system-dependent types and constants.
 *
 * If the machine is big-endian, RF_BIG_ENDIAN should be 1.
 * Otherwise, it should be 0.
 *
 * The various integer types should be self-explanatory; we
 * use these elsewhere to avoid size confusion.
 *
 * LONGSHIFT is lg(sizeof(long)) (that is, log base two of sizeof(long)
 *
 */

#if defined(__NetBSD__) || defined(__OpenBSD__)

#include <sys/types.h>
#include <machine/endian.h>
#include <machine/limits.h>

#if BYTE_ORDER == BIG_ENDIAN
#define RF_IS_BIG_ENDIAN    1
#elif BYTE_ORDER == LITTLE_ENDIAN
#define RF_IS_BIG_ENDIAN    0
#else
#error byte order not defined
#endif
typedef int8_t              RF_int8;
typedef u_int8_t            RF_uint8;
typedef int16_t             RF_int16;
typedef u_int16_t           RF_uint16;
typedef int32_t             RF_int32;
typedef u_int32_t           RF_uint32;
typedef int64_t             RF_int64;
typedef u_int64_t           RF_uint64;
#if LONG_BIT == 32
#define RF_LONGSHIFT        2
#elif LONG_BIT == 64
#define RF_LONGSHIFT        3
#else
#error word size not defined
#endif

#else /* __NetBSD__ || __OpenBSD__ */

#ifdef __alpha
#define RF_IS_BIG_ENDIAN    0
typedef signed char         RF_int8;
typedef unsigned char       RF_uint8;
typedef short               RF_int16;
typedef unsigned short      RF_uint16;
typedef int                 RF_int32;
typedef unsigned int        RF_uint32;
typedef long                RF_int64;
typedef unsigned long       RF_uint64;
#define RF_LONGSHIFT        3
#endif /* __alpha */

#ifdef _IBMR2
#define RF_IS_BIG_ENDIAN    1
typedef signed char         RF_int8;
typedef unsigned char       RF_uint8;
typedef short               RF_int16;
typedef unsigned short      RF_uint16;
typedef int                 RF_int32;
typedef unsigned int        RF_uint32;
typedef long long           RF_int64;
typedef unsigned long long  RF_uint64;
#define RF_LONGSHIFT        2
#endif /* _IBMR2 */

#ifdef hpux
#define RF_IS_BIG_ENDIAN    1
typedef signed char         RF_int8;
typedef unsigned char       RF_uint8;
typedef short               RF_int16;
typedef unsigned short      RF_uint16;
typedef int                 RF_int32;
typedef unsigned int        RF_uint32;
typedef long long           RF_int64;
typedef unsigned long long  RF_uint64;
#define RF_LONGSHIFT        2
#endif /* hpux */

#ifdef sun
#define RF_IS_BIG_ENDIAN    1
typedef char                RF_int8;
typedef unsigned char       RF_uint8;
typedef short               RF_int16;
typedef unsigned short      RF_uint16;
typedef int                 RF_int32;
typedef unsigned int        RF_uint32;
typedef long long           RF_int64;
typedef unsigned long long  RF_uint64;
#define RF_LONGSHIFT        2
#endif /* sun */

#if defined(NETBSD_I386) || defined(NETBSD_I386) || defined(LINUX_I386)
#define RF_IS_BIG_ENDIAN    0
typedef char                RF_int8;
typedef unsigned char       RF_uint8;
typedef short               RF_int16;
typedef unsigned short      RF_uint16;
typedef int                 RF_int32;
typedef unsigned int        RF_uint32;
typedef long long           RF_int64;
typedef unsigned long long  RF_uint64;
#define RF_LONGSHIFT        2
#endif /* NETBSD_I386 || OPENBSD_I386 || LINUX_I386 */

#if defined(mips) && !defined(SGI) && !defined(__NetBSD__) && !defined(__OpenBSD__)
#define RF_IS_BIG_ENDIAN    0
typedef char                RF_int8;
typedef unsigned char       RF_uint8;
typedef short               RF_int16;
typedef unsigned short      RF_uint16;
typedef int                 RF_int32;
typedef unsigned int        RF_uint32;
typedef long long           RF_int64;
typedef unsigned long long  RF_uint64;
#define RF_LONGSHIFT        2
#endif /* mips && !SGI */

#ifdef SGI
#if _MIPS_SZLONG == 64
#define RF_IS_BIG_ENDIAN    1
typedef signed char         RF_int8;
typedef unsigned char       RF_uint8;
typedef short               RF_int16;
typedef unsigned short      RF_uint16;
typedef int                 RF_int32;
typedef unsigned int        RF_uint32;
typedef long                RF_int64;
typedef unsigned long       RF_uint64;
#define RF_LONGSHIFT        3
#endif /* _MIPS_SZLONG == 64 */
#if _MIPS_SZLONG == 32
#define RF_IS_BIG_ENDIAN    1
typedef char                RF_int8;
typedef unsigned char       RF_uint8;
typedef short               RF_int16;
typedef unsigned short      RF_uint16;
typedef int                 RF_int32;
typedef unsigned int        RF_uint32;
typedef long long           RF_int64;
typedef unsigned long long  RF_uint64;
#define RF_LONGSHIFT        2
#endif /* _MIPS_SZLONG == 32 */
#endif /* SGI */

#endif /* __NetBSD__ || __OpenBSD__ */

/*
 * These are just zero and non-zero. We don't use "TRUE"
 * and "FALSE" because there's too much nonsense trying
 * to get them defined exactly once on every platform, given
 * the different places they may be defined in system header
 * files.
 */
#define RF_TRUE  1
#define RF_FALSE 0

/*
 * Now, some generic types
 */
typedef RF_uint64    RF_IoCount_t;
typedef RF_uint64    RF_Offset_t;
typedef RF_uint32    RF_PSSFlags_t;
typedef RF_uint64    RF_SectorCount_t;
typedef RF_uint64    RF_StripeCount_t;
typedef RF_int64     RF_SectorNum_t; /* these are unsigned so we can set them to (-1) for "uninitialized" */
typedef RF_int64     RF_StripeNum_t;
typedef RF_int64     RF_RaidAddr_t;
typedef int          RF_RowCol_t; /* unsigned so it can be (-1) */
typedef RF_int64     RF_HeadSepLimit_t;
typedef RF_int64     RF_ReconUnitCount_t;
typedef int          RF_ReconUnitNum_t;

typedef char RF_ParityConfig_t;

typedef char RF_DiskQueueType_t[1024];
#define RF_DISK_QUEUE_TYPE_NONE ""

/* values for the 'type' field in a reconstruction buffer */
typedef int RF_RbufType_t;
#define RF_RBUF_TYPE_EXCLUSIVE   0    /* this buf assigned exclusively to one disk */
#define RF_RBUF_TYPE_FLOATING    1    /* this is a floating recon buf */
#define RF_RBUF_TYPE_FORCED      2    /* this rbuf was allocated to complete a forced recon */

typedef char RF_IoType_t;
#define RF_IO_TYPE_READ          'r'
#define RF_IO_TYPE_WRITE         'w'
#define RF_IO_TYPE_NOP           'n'
#define RF_IO_IS_R_OR_W(_type_) (((_type_) == RF_IO_TYPE_READ) \
                                || ((_type_) == RF_IO_TYPE_WRITE))

#ifdef SIMULATE
typedef double RF_TICS_t;
typedef int RF_Owner_t;
#endif /* SIMULATE */

typedef void (*RF_VoidFuncPtr)(void *,...);

typedef RF_uint32 RF_AccessStripeMapFlags_t;
typedef RF_uint32 RF_DiskQueueDataFlags_t;
typedef RF_uint32 RF_DiskQueueFlags_t;
typedef RF_uint32 RF_RaidAccessFlags_t;

#define RF_DISKQUEUE_DATA_FLAGS_NONE ((RF_DiskQueueDataFlags_t)0)

typedef struct RF_AccessStripeMap_s          RF_AccessStripeMap_t;
typedef struct RF_AccessStripeMapHeader_s    RF_AccessStripeMapHeader_t;
typedef struct RF_AllocListElem_s            RF_AllocListElem_t;
typedef struct RF_CallbackDesc_s             RF_CallbackDesc_t;
typedef struct RF_ChunkDesc_s                RF_ChunkDesc_t;
typedef struct RF_CommonLogData_s            RF_CommonLogData_t;
typedef struct RF_Config_s                   RF_Config_t;
typedef struct RF_CumulativeStats_s          RF_CumulativeStats_t;
typedef struct RF_DagHeader_s                RF_DagHeader_t;
typedef struct RF_DagList_s                  RF_DagList_t;
typedef struct RF_DagNode_s                  RF_DagNode_t;
typedef struct RF_DeclusteredConfigInfo_s    RF_DeclusteredConfigInfo_t;
typedef struct RF_DiskId_s                   RF_DiskId_t;
typedef struct RF_DiskMap_s                  RF_DiskMap_t;
typedef struct RF_DiskQueue_s                RF_DiskQueue_t;
typedef struct RF_DiskQueueData_s            RF_DiskQueueData_t;
typedef struct RF_DiskQueueSW_s              RF_DiskQueueSW_t;
typedef struct RF_Etimer_s                   RF_Etimer_t;
typedef struct RF_EventCreate_s              RF_EventCreate_t;
typedef struct RF_FreeList_s                 RF_FreeList_t;
typedef struct RF_LockReqDesc_s              RF_LockReqDesc_t;
typedef struct RF_LockTableEntry_s           RF_LockTableEntry_t;
typedef struct RF_MCPair_s                   RF_MCPair_t;
typedef struct RF_OwnerInfo_s                RF_OwnerInfo_t;
typedef struct RF_ParityLog_s                RF_ParityLog_t;
typedef struct RF_ParityLogAppendQueue_s     RF_ParityLogAppendQueue_t;
typedef struct RF_ParityLogData_s            RF_ParityLogData_t;
typedef struct RF_ParityLogDiskQueue_s       RF_ParityLogDiskQueue_t;
typedef struct RF_ParityLogQueue_s           RF_ParityLogQueue_t;
typedef struct RF_ParityLogRecord_s          RF_ParityLogRecord_t;
typedef struct RF_PerDiskReconCtrl_s         RF_PerDiskReconCtrl_t;
typedef struct RF_PSStatusHeader_s           RF_PSStatusHeader_t;
typedef struct RF_PhysDiskAddr_s             RF_PhysDiskAddr_t;
typedef struct RF_PropHeader_s               RF_PropHeader_t;
typedef struct RF_Raid_s                     RF_Raid_t;
typedef struct RF_RaidAccessDesc_s           RF_RaidAccessDesc_t;
typedef struct RF_RaidDisk_s                 RF_RaidDisk_t;
typedef struct RF_RaidLayout_s               RF_RaidLayout_t;
typedef struct RF_RaidReconDesc_s            RF_RaidReconDesc_t;
typedef struct RF_ReconBuffer_s              RF_ReconBuffer_t;
typedef struct RF_ReconConfig_s              RF_ReconConfig_t;
typedef struct RF_ReconCtrl_s                RF_ReconCtrl_t;
typedef struct RF_ReconDoneProc_s            RF_ReconDoneProc_t;
typedef struct RF_ReconEvent_s               RF_ReconEvent_t;
typedef struct RF_ReconMap_s                 RF_ReconMap_t;
typedef struct RF_ReconMapListElem_s         RF_ReconMapListElem_t;
typedef struct RF_ReconParityStripeStatus_s  RF_ReconParityStripeStatus_t;
typedef struct RF_RedFuncs_s                 RF_RedFuncs_t;
typedef struct RF_RegionBufferQueue_s        RF_RegionBufferQueue_t;
typedef struct RF_RegionInfo_s               RF_RegionInfo_t;
typedef struct RF_ShutdownList_s             RF_ShutdownList_t;
typedef struct RF_SpareTableEntry_s          RF_SpareTableEntry_t;
typedef struct RF_SparetWait_s               RF_SparetWait_t;
typedef struct RF_StripeLockDesc_s           RF_StripeLockDesc_t;
typedef struct RF_ThreadGroup_s              RF_ThreadGroup_t;
typedef struct RF_ThroughputStats_s          RF_ThroughputStats_t;

/*
 * Important assumptions regarding ordering of the states in this list
 * have been made!!!
 * Before disturbing this ordering, look at code in rf_states.c
 */
typedef enum RF_AccessState_e {
  /* original states */
  rf_QuiesceState,           /* handles queisence for reconstruction */
  rf_IncrAccessesCountState, /* count accesses in flight */
  rf_DecrAccessesCountState,
  rf_MapState,               /* map access to disk addresses */
  rf_LockState,              /* take stripe locks */
  rf_CreateDAGState,         /* create DAGs */
  rf_ExecuteDAGState,        /* execute DAGs */
  rf_ProcessDAGState,        /* DAGs are completing- check if correct, or if we need to retry */
  rf_CleanupState,           /* release stripe locks, clean up */
  rf_LastState               /* must be the last state */
} RF_AccessState_t;

#define RF_MAXROW    10 /* these are arbitrary and can be modified at will */
#define RF_MAXCOL    40
#define RF_MAXSPARE  10
#define RF_MAXDBGV   75 /* max number of debug variables */

union RF_GenericParam_u {
  void       *p;
  RF_uint64   v;
};
typedef union RF_GenericParam_u RF_DagParam_t;
typedef union RF_GenericParam_u RF_CBParam_t;

#endif /* _RF__RF_TYPES_H_ */
