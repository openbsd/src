/*	$Id: kafs.h,v 1.1.1.1 1995/12/14 06:52:34 tholo Exp $	*/

#ifndef __KAFS_H
#define __KAFS_H

#define AFSCALL_PIOCTL 20
#define AFSCALL_SETPAG 21

#ifndef _VICEIOCTL
#if defined(__STDC__) || defined(sgi)
#define _VICEIOCTL(id)  ((unsigned int ) _IOW('V', id, struct ViceIoctl))
#else
#define _VICEIOCTL(id)  ((unsigned int ) _IOW(V, id, struct ViceIoctl))
#endif
#endif /* _VICEIOCTL */

#define VIOCSETTOK _VICEIOCTL(3)
#define VIOCUNLOG  _VICEIOCTL(9)

struct ViceIoctl {
  caddr_t in, out;
  short in_size;
  short out_size;
};

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

int k_afsklog __P((char *realm));
int k_pioctl __P((char *a_path,
		  int o_opcode,
		  struct ViceIoctl *a_paramsP,
		  int a_followSymlinks));
int k_unlog __P((void));
int k_setpag __P((void));

#endif /* __KAFS_H */
