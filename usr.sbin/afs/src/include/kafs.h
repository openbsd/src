/*
 * Copyright (c) 1995 - 2002 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* $arla: kafs.h,v 1.49 2003/01/09 16:46:56 lha Exp $ */

#ifndef __KAFS_H
#define __KAFS_H

/* XXX must include krb5.h or krb.h */

/* sys/ioctl.h must be included manually before kafs.h */

/*
 */
#define AFSCALL_PIOCTL 20
#define AFSCALL_SETPAG 21

#ifndef _VICEIOCTL
#define _VICEIOCTL(id)  ((unsigned int ) _IOW('V', id, struct ViceIoctl))
#endif /* _VICEIOCTL */

#ifndef _ARLAIOCTL
#define _ARLAIOCTL(id)  ((unsigned int ) _IOW('A', id, struct ViceIoctl))
#endif /* _ARLAIOCTL */

#ifndef _AFSCOMMONIOCTL
#define _AFSCOMMONIOCTL(id)  ((unsigned int ) _IOW('C', id, struct ViceIoctl))
#endif /* _AFSCOMMONIOCTL */

/* 
 * this is for operating systems that support a 32-bit API while being
 * 64-bit (such as solaris 7 in 64 bit mode).  The ioctls will get
 * assigned different numbers in 32bits mode and we need to support
 * both.
 */

#ifdef NEED_VICEIOCTL32

#ifndef _VICEIOCTL32
#define _VICEIOCTL32(id)  ((unsigned int ) _IOW('V', id, struct ViceIoctl32))
#endif /* _VICEIOCTL32 */

#ifndef _ARLAIOCTL32
#define _ARLAIOCTL32(id)  ((unsigned int ) _IOW('A', id, struct ViceIoctl32))
#endif /* _ARLAIOCTL32 */

#ifndef _AFSCOMMONIOCTL32
#define _AFSCOMMONIOCTL32(id)  ((unsigned int ) _IOW('C', id, struct ViceIoctl32))
#endif /* _AFSCOMMONIOCTL32 */

#ifndef _VICEIOCTL64
#define _VICEIOCTL64(id)  ((unsigned int ) _IOW('V', id, struct ViceIoctl64))
#endif /* _VICEIOCTL64 */

#ifndef _ARLAIOCTL64
#define _ARLAIOCTL64(id)  ((unsigned int ) _IOW('A', id, struct ViceIoctl64))
#endif /* _ARLAIOCTL64 */

#ifndef _AFSCOMMONIOCTL64
#define _AFSCOMMONIOCTL64(id)  ((unsigned int ) _IOW('C', id, struct ViceIoctl64))
#endif /* _AFSCOMMONIOCTL64 */

#endif /* NEED_VICEIOCTL32 */

#include <nnpfs/nnpfs_pioctl.h>

/*
 * ioctls
 */

#define VIOCCLOSEWAIT_32	_VICEIOCTL32(1)
#define VIOCABORT_32		_VICEIOCTL32(2)
#define VIOIGETCELL_32		_VICEIOCTL32(3)

/*
 * pioctls
 */

#define AIOC_STATISTICS         _ARLAIOCTL(1)   /* arla: fetch statistics */
#define AIOC_PTSNAMETOID        _ARLAIOCTL(2)   /* arla: pts name to id */
#define AIOC_GETCACHEPARAMS	_ARLAIOCTL(3)	/* arla: get cache params */
#define AIOC_GETPREFETCH	_ARLAIOCTL(4)	/* arla: get prefetch value */
#define AIOC_SETPREFETCH	_ARLAIOCTL(5)	/* arla: set prefetch value */
#define AFSCOMMONIOC_GKK5SETTOK	_ARLAIOCTL(6)	/* gk k5 set token */

#define AFSCOMMONIOC_NEWALIAS	_AFSCOMMONIOCTL(1) /* common: ... */
#define AFSCOMMONIOC_LISTALIAS	_AFSCOMMONIOCTL(2) /* common: ... */

#ifdef NEED_VICEIOCTL32

/* and now for the 32-bit versions */

#define VIOCSETAL_32			_VICEIOCTL32(1)
#define VIOCGETAL_32			_VICEIOCTL32(2)
#define VIOCSETTOK_32			_VICEIOCTL32(3)
#define VIOCGETVOLSTAT_32		_VICEIOCTL32(4)
#define VIOCSETVOLSTAT_32		_VICEIOCTL32(5)
#define VIOCFLUSH_32			_VICEIOCTL32(6)
#define VIOCSTAT_32			_VICEIOCTL32(7)
#define VIOCGETTOK_32			_VICEIOCTL32(8)
#define VIOCUNLOG_32			_VICEIOCTL32(9)
#define VIOCCKSERV_32			_VICEIOCTL32(10)
#define VIOCCKBACK_32			_VICEIOCTL32(11)
#define VIOCCKCONN_32			_VICEIOCTL32(12)
#define VIOCGETTIME_32			_VICEIOCTL32(13)
#define VIOCWHEREIS_32			_VICEIOCTL32(14)
#define VIOCPREFETCH_32			_VICEIOCTL32(15)
#define VIOCNOP_32			_VICEIOCTL32(16)
#define VIOCENGROUP_32			_VICEIOCTL32(17)
#define VIOCDISGROUP_32			_VICEIOCTL32(18)
#define VIOCLISTGROUPS_32		_VICEIOCTL32(19)
#define VIOCACCESS_32			_VICEIOCTL32(20)
#define VIOCUNPAG_32			_VICEIOCTL32(21)
#define VIOCGETFID_32			_VICEIOCTL32(22)
#define VIOCWAITFOREVER_32		_VICEIOCTL32(23)
#define VIOCSETCACHESIZE_32		_VICEIOCTL32(24)
#define VIOCFLUSHCB_32			_VICEIOCTL32(25)
#define VIOCNEWCELL_32			_VICEIOCTL32(26)
#define VIOCGETCELL_32			_VICEIOCTL32(27)
#define VIOC_AFS_DELETE_MT_PT_32	_VICEIOCTL32(28)
#define VIOC_AFS_STAT_MT_PT_32		_VICEIOCTL32(29)
#define VIOC_FILE_CELL_NAME_32		_VICEIOCTL32(30)
#define VIOC_GET_WS_CELL_32		_VICEIOCTL32(31)
#define VIOC_AFS_MARINER_HOST_32	_VICEIOCTL32(32)
#define VIOC_GET_PRIMARY_CELL_32	_VICEIOCTL32(33)
#define VIOC_VENUSLOG_32		_VICEIOCTL32(34)
#define VIOC_GETCELLSTATUS_32		_VICEIOCTL32(35)
#define VIOC_SETCELLSTATUS_32		_VICEIOCTL32(36)
#define VIOC_FLUSHVOLUME_32		_VICEIOCTL32(37)
#define VIOC_AFS_SYSNAME_32		_VICEIOCTL32(38)
#define VIOC_EXPORTAFS_32		_VICEIOCTL32(39)
#define VIOCGETCACHEPARAMS_32		_VICEIOCTL32(40)
#define VIOCCONNECTMODE_32	        _VICEIOCTL32(41)
#define VIOCGETVCXSTATUS_32		_VICEIOCTL32(41)
#define VIOC_SETSPREFS_3233		_VICEIOCTL32(42)
#define VIOC_GETSPREFS_32		_VICEIOCTL32(43)
#define VIOC_GAG_32			_VICEIOCTL32(44)
#define VIOC_TWIDDLE_32			_VICEIOCTL32(45)
#define VIOC_SETSPREFS_32		_VICEIOCTL32(46)
#define VIOC_STORBEHIND_32		_VICEIOCTL32(47)
#define VIOC_GCPAGS_32			_VICEIOCTL32(48)
#define VIOC_GETINITPARAMS_32      	_VICEIOCTL32(49)
#define VIOC_GETCPREFS_32          	_VICEIOCTL32(50)
#define VIOC_SETCPREFS_32          	_VICEIOCTL32(51)
#define VIOC_FLUSHMOUNT_32         	_VICEIOCTL32(52)
#define VIOC_RXSTATPROC_32         	_VICEIOCTL32(53)
#define VIOC_RXSTATPEER_32         	_VICEIOCTL32(54)

#define VIOC_GETRXKCRYPT_32		_VICEIOCTL32(55) /* 48 in some implementations */
#define VIOC_SETRXKCRYPT_32		_VICEIOCTL32(56) /* with cryptosupport in afs */
#define VIOC_FPRIOSTATUS_32		_VICEIOCTL32(57)

#define VIOC_FHGET_32			_VICEIOCTL32(58)
#define VIOC_FHOPEN_32			_VICEIOCTL32(59)

#define VIOC_NNPFSDEBUG_32		_VICEIOCTL32(60)
#define VIOC_ARLADEBUG_32		_VICEIOCTL32(61)

#define VIOC_AVIATOR_32			_VICEIOCTL32(62)

#define VIOC_NNPFSDEBUG_PRINT_32		_VICEIOCTL32(63)

#define VIOC_CALCULATE_CACHE_32		_VICEIOCTL32(64)

#define VIOC_BREAKCALLBACK_32		_VICEIOCTL32(65)
#define VIOC_PREFETCHTAPE_32		_VICEIOCTL32(66)
#define VIOC_RESIDENCY_CMD_32		_VICEIOCTL32(67)

/* and now for the 64-bit versions */

#define VIOCSETAL_64			_VICEIOCTL64(1)
#define VIOCGETAL_64			_VICEIOCTL64(2)
#define VIOCSETTOK_64			_VICEIOCTL64(3)
#define VIOCGETVOLSTAT_64		_VICEIOCTL64(4)
#define VIOCSETVOLSTAT_64		_VICEIOCTL64(5)
#define VIOCFLUSH_64			_VICEIOCTL64(6)
#define VIOCSTAT_64			_VICEIOCTL64(7)
#define VIOCGETTOK_64			_VICEIOCTL64(8)
#define VIOCUNLOG_64			_VICEIOCTL64(9)
#define VIOCCKSERV_64			_VICEIOCTL64(10)
#define VIOCCKBACK_64			_VICEIOCTL64(11)
#define VIOCCKCONN_64			_VICEIOCTL64(12)
#define VIOCGETTIME_64			_VICEIOCTL64(13)
#define VIOCWHEREIS_64			_VICEIOCTL64(14)
#define VIOCPREFETCH_64			_VICEIOCTL64(15)
#define VIOCNOP_64			_VICEIOCTL64(16)
#define VIOCENGROUP_64			_VICEIOCTL64(17)
#define VIOCDISGROUP_64			_VICEIOCTL64(18)
#define VIOCLISTGROUPS_64		_VICEIOCTL64(19)
#define VIOCACCESS_64			_VICEIOCTL64(20)
#define VIOCUNPAG_64			_VICEIOCTL64(21)
#define VIOCGETFID_64			_VICEIOCTL64(22)
#define VIOCWAITFOREVER_64		_VICEIOCTL64(23)
#define VIOCSETCACHESIZE_64		_VICEIOCTL64(24)
#define VIOCFLUSHCB_64			_VICEIOCTL64(25)
#define VIOCNEWCELL_64			_VICEIOCTL64(26)
#define VIOCGETCELL_64			_VICEIOCTL64(27)
#define VIOC_AFS_DELETE_MT_PT_64	_VICEIOCTL64(28)
#define VIOC_AFS_STAT_MT_PT_64		_VICEIOCTL64(29)
#define VIOC_FILE_CELL_NAME_64		_VICEIOCTL64(30)
#define VIOC_GET_WS_CELL_64		_VICEIOCTL64(31)
#define VIOC_AFS_MARINER_HOST_64	_VICEIOCTL64(32)
#define VIOC_GET_PRIMARY_CELL_64	_VICEIOCTL64(33)
#define VIOC_VENUSLOG_64		_VICEIOCTL64(34)
#define VIOC_GETCELLSTATUS_64		_VICEIOCTL64(35)
#define VIOC_SETCELLSTATUS_64		_VICEIOCTL64(36)
#define VIOC_FLUSHVOLUME_64		_VICEIOCTL64(37)
#define VIOC_AFS_SYSNAME_64		_VICEIOCTL64(38)
#define VIOC_EXPORTAFS_64		_VICEIOCTL64(39)
#define VIOCGETCACHEPARAMS_64		_VICEIOCTL64(40)
#define VIOCCONNECTMODE_64	        _VICEIOCTL64(41)
#define VIOCGETVCXSTATUS_64		_VICEIOCTL64(41)
#define VIOC_SETSPREFS_6433		_VICEIOCTL64(42)
#define VIOC_GETSPREFS_64		_VICEIOCTL64(43)
#define VIOC_GAG_64			_VICEIOCTL64(44)
#define VIOC_TWIDDLE_64			_VICEIOCTL64(45)
#define VIOC_SETSPREFS_64		_VICEIOCTL64(46)
#define VIOC_STORBEHIND_64		_VICEIOCTL64(47)
#define VIOC_GCPAGS_64			_VICEIOCTL64(48)
#define VIOC_GETINITPARAMS_64      	_VICEIOCTL64(49)
#define VIOC_GETCPREFS_64          	_VICEIOCTL64(50)
#define VIOC_SETCPREFS_64          	_VICEIOCTL64(51)
#define VIOC_FLUSHMOUNT_64         	_VICEIOCTL64(52)
#define VIOC_RXSTATPROC_64         	_VICEIOCTL64(53)
#define VIOC_RXSTATPEER_64         	_VICEIOCTL64(54)

#define VIOC_GETRXKCRYPT_64		_VICEIOCTL64(55) /* 48 in some implementations */
#define VIOC_SETRXKCRYPT_64		_VICEIOCTL64(56) /* with cryptosupport in afs */
#define VIOC_FPRIOSTATUS_64		_VICEIOCTL64(57)

#define VIOC_FHGET_64			_VICEIOCTL64(58)
#define VIOC_FHOPEN_64			_VICEIOCTL64(59)

#define VIOC_NNPFSDEBUG_64		_VICEIOCTL64(60)
#define VIOC_ARLADEBUG_64		_VICEIOCTL64(61)

#define VIOC_AVIATOR_64			_VICEIOCTL64(62)

#define VIOC_NNPFSDEBUG_PRINT_64		_VICEIOCTL64(63)

#define VIOC_CALCULATE_CACHE_64		_VICEIOCTL64(64)

#define VIOC_BREAKCALLBACK_64		_VICEIOCTL64(65)

#define VIOC_PREFETCHTAPE_64		_VICEIOCTL64(66)
#define VIOC_RESIDENCY_CMD_64		_VICEIOCTL64(67)

/*
 * Arla implementationspecific IOCTL'S
 */

#define AIOC_STATISTICS_32	_ARLAIOCTL32(1) /* arla: fetch statistics */
#define AIOC_STATISTICS_64	_ARLAIOCTL64(1) /* arla: fetch statistics */
#define AIOC_PTSNAMETOID_32     _ARLAIOCTL32(2) /* arla: pts name to id */
#define AIOC_PTSNAMETOID_64     _ARLAIOCTL64(2) /* arla: pts name to id */
#define AIOC_GETCACHEPARAMS_32	_ARLAIOCTL32(3) /* arla: get cache param */
#define AIOC_GETCACHEPARAMS_64	_ARLAIOCTL64(3) /* arla: get cache param */

#define AFSCOMMONIOC_NEWALIAS_32	_AFSCOMMONIOCTL32(1) /* common: ... */
#define AFSCOMMONIOC_NEWALIAS_64	_AFSCOMMONIOCTL64(1) /* common: ... */
#define AFSCOMMONIOC_LISTALIAS_32	_AFSCOMMONIOCTL32(2) /* common: ... */
#define AFSCOMMONIOC_LISTALIAS_64	_AFSCOMMONIOCTL64(2) /* common: ... */

#endif /* NEED_VICEIOCTL32 */

/*
 * GETCELLSTATUS flags
 */

#define CELLSTATUS_PRIMARY	0x01 /* this is the `primary' cell */
#define CELLSTATUS_SETUID	0x02 /* setuid honored for this cell */
#define CELLSTATUS_OBSOLETE_VL	0x04 /* uses obsolete VL servers */

/*
 * VIOCCONNECTMODE arguments
 */

#define CONNMODE_PROBE 0
#define CONNMODE_CONN 1
#define CONNMODE_FETCH 2
#define CONNMODE_DISCONN 3
#define CONNMODE_PARCONNECTED 4
#define CONNMODE_CONN_WITHCALLBACKS 5

/*
 * The struct for VIOC_FPRIOSTATUS
 */

#define FPRIO_MAX 100
#define FPRIO_MIN 0
#define FPRIO_DEFAULT FPRIO_MAX

#define FPRIO_GET 0
#define FPRIO_SET 1
#define FPRIO_GETMAX 2
#define FPRIO_SETMAX 3

/*
 * Flags for VIOCCKSERV
 */

#define CKSERV_DONTPING     1
#define CKSERV_FSONLY       2

#define CKSERV_MAXSERVERS   16 /* limitation of VIOCCKSERV number of 
				  returned servers */

/* 
 *  for AIOC_STATISTICS
 */

#define STATISTICS_OPCODE_LIST 0
#define STATISTICS_OPCODE_GETENTRY 1

#define STATISTICS_REQTYPE_FETCHSTATUS 1
#define STATISTICS_REQTYPE_FETCHDATA 2
#define STATISTICS_REQTYPE_BULKSTATUS 3
#define STATISTICS_REQTYPE_STOREDATA 4
#define STATISTICS_REQTYPE_STORESTATUS 5

/* 
 *  for AIOC_GETCACHEPARAMS
 */

#define GETCACHEPARAMS_OPCODE_HIGHBYTES		1
#define GETCACHEPARAMS_OPCODE_USEDBYTES		2
#define GETCACHEPARAMS_OPCODE_LOWBYTES		3
#define GETCACHEPARAMS_OPCODE_HIGHVNODES	4
#define GETCACHEPARAMS_OPCODE_USEDVNODES	5
#define GETCACHEPARAMS_OPCODE_LOWVNODES		6


struct ViceIoctl32 {
  uint32_t in, out;		/* really caddr_t in 32 bits */
  short in_size;
  short out_size;
};

#if NEED_VICEIOCTL32
struct ViceIoctl64 {
#ifdef HAVE_UINT64_T
  uint64_t in, out;		/* really caddr_t in 64 bits */
#else
  caddr_t in, out;
#endif
  short in_size;
  short out_size;
};
#endif /* NEED_VICEIOCTL32 */

#ifndef __P
#define __P(x) x
#endif

/* Use k_hasafs() to probe if the machine supports AFS syscalls.
   The other functions will generate a SIGSYS if AFS is not supported */

int k_hasafs __P((void));

int krb_afslog __P((const char *cell, const char *realm));
int krb_afslog_uid __P((const char *cell, const char *realm, uid_t uid));

int k_pioctl __P((char *a_path,
		  int o_opcode,
		  struct ViceIoctl *a_paramsP,
		  int a_followSymlinks));
int k_unlog __P((void));
int k_setpag __P((void));
int k_afs_cell_of_file __P((const char *path, char *cell, int len));

/* XXX */
#ifdef KFAILURE
#define KRB_H_INCLUDED
#endif

#ifdef KRB5_RECVAUTH_IGNORE_VERSION
#define KRB5_H_INCLUDED
#endif

#ifdef KRB_H_INCLUDED
int kafs_settoken __P((const char*, uid_t, CREDENTIALS*));
#endif

#ifdef KRB5_H_INCLUDED
krb5_error_code krb5_afslog_uid __P((krb5_context, krb5_ccache,
				     const char*, krb5_const_realm, uid_t));
krb5_error_code krb5_afslog __P((krb5_context, krb5_ccache, 
				 const char*, krb5_const_realm));
#endif


#define _PATH_VICE		"/usr/vice/etc/"
#define _PATH_THISCELL 		_PATH_VICE "ThisCell"
#define _PATH_CELLSERVDB 	_PATH_VICE "CellServDB"
#define _PATH_THESECELLS	_PATH_VICE "TheseCells"

#define _PATH_ARLA_VICE		"/usr/arla/etc/"
#define _PATH_ARLA_THISCELL	_PATH_ARLA_VICE "ThisCell"
#define _PATH_ARLA_CELLSERVDB 	_PATH_ARLA_VICE "CellServDB"
#define _PATH_ARLA_THESECELLS	_PATH_ARLA_VICE "TheseCells"

#endif /* __KAFS_H */
