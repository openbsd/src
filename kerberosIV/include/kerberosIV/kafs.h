/*	$OpenBSD: kafs.h,v 1.8 1998/09/15 18:21:01 art Exp $	*/
/*	$KTH: kafs.h,v 1.28 1998/04/26 18:20:09 joda Exp $	*/

/*
 * Copyright (c) 1995, 1996, 1997 Kungliga Tekniska Högskolan
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
 *      Högskolan and its contributors.
 * 
 * 4. Neither the name of the Institute nor the names of its contributors
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

/*
 * ioctls
 */

#define VIOCCLOSEWAIT		_VICEIOCTL(1)
#define VIOCABORT		_VICEIOCTL(2)
#define VIOIGETCELL		_VICEIOCTL(3)

/*
 * pioctls
 */

#define VIOCSETAL               _VICEIOCTL(1)
#define VIOCGETAL               _VICEIOCTL(2)
#define VIOCSETTOK              _VICEIOCTL(3)
#define VIOCGETVOLSTAT          _VICEIOCTL(4)
#define VIOCSETVOLSTAT          _VICEIOCTL(5)
#define VIOCFLUSH               _VICEIOCTL(6)
#define VIOCSTAT		_VICEIOCTL(7)
#define VIOCGETTOK              _VICEIOCTL(8)
#define VIOCUNLOG               _VICEIOCTL(9)
#define VIOCCKSERV              _VICEIOCTL(10)
#define VIOCCKBACK              _VICEIOCTL(11)
#define VIOCCKCONN              _VICEIOCTL(12)
#define VIOCGETTIME		_VICEIOCTL(13)
#define VIOCWHEREIS             _VICEIOCTL(14)
#define VIOCPREFETCH		_VICEIOCTL(15)
#define VIOCNOP			_VICEIOCTL(16)
#define VIOCENGROUP		_VICEIOCTL(17)
#define VIOCDISGROUP		_VICEIOCTL(18)
#define VIOCLISTGROUPS		_VICEIOCTL(19)
#define VIOCACCESS              _VICEIOCTL(20)
#define VIOCUNPAG               _VICEIOCTL(21)
#define VIOCGETFID              _VICEIOCTL(22)
#define VIOCWAITFOREVER		_VICEIOCTL(23)
#define VIOCSETCACHESIZE        _VICEIOCTL(24)
#define VIOCFLUSHCB             _VICEIOCTL(25)
#define VIOCNEWCELL             _VICEIOCTL(26)
#define VIOCGETCELL             _VICEIOCTL(27)
#define VIOC_AFS_DELETE_MT_PT   _VICEIOCTL(28)
#define VIOC_AFS_STAT_MT_PT     _VICEIOCTL(29)
#define VIOC_FILE_CELL_NAME     _VICEIOCTL(30)
#define VIOC_GET_WS_CELL        _VICEIOCTL(31)
#define VIOC_AFS_MARINER_HOST   _VICEIOCTL(32)
#define VIOC_GET_PRIMARY_CELL   _VICEIOCTL(33)
#define VIOC_VENUSLOG           _VICEIOCTL(34)
#define VIOC_GETCELLSTATUS      _VICEIOCTL(35)
#define VIOC_SETCELLSTATUS      _VICEIOCTL(36)
#define VIOC_FLUSHVOLUME        _VICEIOCTL(37)
#define VIOC_AFS_SYSNAME        _VICEIOCTL(38)
#define VIOC_EXPORTAFS          _VICEIOCTL(39)
#define VIOCGETCACHEPARAMS      _VICEIOCTL(40)
#define VIOCCONNECTMODE	        _VICEIOCTL(41)
#define VIOCGETVCXSTATUS	_VICEIOCTL(41)
#define VIOC_SETSPREFS33	_VICEIOCTL(42)
#define VIOC_GETSPREFS		_VICEIOCTL(43)
#define VIOC_GAG		_VICEIOCTL(44)
#define VIOC_TWIDDLE		_VICEIOCTL(45)
#define VIOC_SETSPREFS		_VICEIOCTL(46)
#define VIOC_STORBEHIND		_VICEIOCTL(47)
#define VIOC_GETRXKCRYPT	_VICEIOCTL(48)
#define VIOC_SETRXKCRYPT	_VICEIOCTL(49)
#define VIOC_FPRIOSTATUS	_VICEIOCTL(50)


/*
 * VIOCCONNECTMODE arguments
 */

#define CONNMODE_PROBE 0
#define CONNMODE_CONN 1
#define CONNMODE_FETCH 2
#define CONNMODE_DISCONN 3

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

struct vioc_fprio {
    int16_t cmd;
    int16_t prio;
    int32_t Cell;
    int32_t Volume;
    int32_t Vnode;
    int32_t Unique;
};

#if !defined(HAVE_STRUCT_VICEIOCTL_IN) || !defined(__KERNEL__)
struct ViceIoctl {
  caddr_t in, out;
  short in_size;
  short out_size;
};
#endif

struct ClearToken {
  int32_t AuthHandle;
  char HandShakeKey[8];
  int32_t ViceId;
  int32_t BeginTimestamp;
  int32_t EndTimestamp;
};

/* Use k_hasafs() to probe if the machine supports AFS syscalls.
   The other functions will generate a SIGSYS if AFS is not supported */

int k_hasafs __P((void));

int krb_afslog __P((const char *cell, const char *realm));
int krb_afslog_uid __P((const char *cell, const char *realm, uid_t uid));
/* compat */
#define k_afsklog krb_afslog
#define k_afsklog_uid krb_afslog_uid

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

#define _PATH_ARLA_VICE		"/etc/afs/"
#define _PATH_ARLA_THISCELL	_PATH_ARLA_VICE "ThisCell"
#define _PATH_ARLA_CELLSERVDB 	_PATH_ARLA_VICE "CellServDB"
#define _PATH_ARLA_THESECELLS	_PATH_ARLA_VICE "TheseCells"

extern int _kafs_debug;

#endif /* __KAFS_H */
