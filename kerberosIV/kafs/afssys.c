/*	$Id: afssys.c,v 1.2 1996/09/16 03:18:08 tholo Exp $	*/

#include <sys/types.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <setjmp.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <kerberosIV/krb.h>
#include <kerberosIV/kafs.h>

#include "afssysdefs.h"

#define AUTH_SUPERUSER "afs"

/*
 * Here only ASCII characters are relevant.
 */

#define IsAsciiUpper(c) ('A' <= (c) && (c) <= 'Z')

#define ToAsciiLower(c) ((c) - 'A' + 'a')

static void
folddown(a, b)
     char *a, *b;
{
  for (; *b; a++, b++)
    if (IsAsciiUpper(*b))
      *a = ToAsciiLower(*b);
    else
      *a = *b;
  *a = '\0';
}

#if !defined(linux)		/* won't work there -- no SIGSYS, no syscall */

int
k_afsklog(realm)
	char *realm;
{
  int k_errno;
  CREDENTIALS c;
  KTEXT_ST ticket;
  char username[256];
  char krealm[REALM_SZ];

  if (!k_hasafs())
    return KSUCCESS;

  if (realm == 0 || realm[0] == 0)
    {
      k_errno = krb_get_lrealm(krealm, 0);
      if (k_errno != KSUCCESS)
	return k_errno;
      realm = krealm;
    }

  k_errno = krb_get_cred(AUTH_SUPERUSER, "", realm, &c);
  if (k_errno != KSUCCESS)
    {
      k_errno = krb_mk_req(&ticket, AUTH_SUPERUSER, "", realm, 0);
      if (k_errno == KSUCCESS)
	k_errno = krb_get_cred(AUTH_SUPERUSER, "", realm, &c);
    }

  if (k_errno == KSUCCESS)
    {
      char cell[256];
      struct ViceIoctl parms;
      struct ClearToken ct;
      int32_t sizeof_x;
      char buf[2048], *t;

      folddown(cell, realm);

      /*
       * Build a struct ClearToken
       */
      ct.AuthHandle = c.kvno;
      bcopy((char *)c.session, ct.HandShakeKey, sizeof(c.session));
      ct.ViceId = getuid();	/* is this always valid? */
      ct.BeginTimestamp = 1 + c.issue_date;
      ct.EndTimestamp = krb_life_to_time(c.issue_date, c.lifetime);

      t = buf;
      /*
       * length of secret token followed by secret token
       */
      sizeof_x = c.ticket_st.length;
      bcopy((char *)&sizeof_x, t, sizeof(sizeof_x));
      t += sizeof(sizeof_x);
      bcopy((char *)c.ticket_st.dat, t, sizeof_x);
      t += sizeof_x;
      /*
       * length of clear token followed by clear token
       */
      sizeof_x = sizeof(ct);
      bcopy((char *)&sizeof_x, t, sizeof(sizeof_x));
      t += sizeof(sizeof_x);
      bcopy((char *)&ct, t, sizeof_x);
      t += sizeof_x;

      /*
       * do *not* mark as primary cell
       */
      sizeof_x = 0;
      bcopy((char *)&sizeof_x, t, sizeof(sizeof_x));
      t += sizeof(sizeof_x);
      /*
       * follow with cell name
       */
      sizeof_x = strlen(cell) + 1;
      bcopy(cell, t, sizeof_x);
      t += sizeof_x;

      /*
       * Build argument block
       */
      parms.in = buf;
      parms.in_size = t - buf;
      parms.out = 0;
      parms.out_size = 0;
      (void) k_pioctl(0, VIOCSETTOK, &parms, 0);
    }
  return k_errno;
}

#define NO_ENTRY_POINT		0
#define SINGLE_ENTRY_POINT	1
#define MULTIPLE_ENTRY_POINT	2
#define SINGLE_ENTRY_POINT2	3
#define AIX_ENTRY_POINTS	4
#define UNKNOWN_ENTRY_POINT	5
static int afs_entry_point = UNKNOWN_ENTRY_POINT;

int
k_pioctl(a_path, o_opcode, a_paramsP, a_followSymlinks)
	char *a_path;
	int o_opcode;
	struct ViceIoctl *a_paramsP;
	int a_followSymlinks;
{
#ifdef AFS_SYSCALL
  if (afs_entry_point == SINGLE_ENTRY_POINT)
    return syscall(AFS_SYSCALL, AFSCALL_PIOCTL,
		   a_path, o_opcode, a_paramsP, a_followSymlinks);
#endif

#ifdef AFS_PIOCTL
    if (afs_entry_point == MULTIPLE_ENTRY_POINT)
      return syscall(AFS_PIOCTL,
		     a_path, o_opcode, a_paramsP, a_followSymlinks);
#endif

#ifdef AFS_SYSCALL2
  if (afs_entry_point == SINGLE_ENTRY_POINT2)
    return syscall(AFS_SYSCALL2, AFSCALL_PIOCTL,
		   a_path, o_opcode, a_paramsP, a_followSymlinks);
#endif

#ifdef _AIX
  if (afs_entry_point == AIX_ENTRY_POINTS)
    return lpioctl(a_path, o_opcode, a_paramsP, a_followSymlinks);
#endif

  errno = ENOSYS;
  kill(getpid(), SIGSYS);	/* You loose! */
  return -1;
}

int
k_unlog()
{
  struct ViceIoctl parms;
  bzero((char *)&parms, sizeof(parms));
  return k_pioctl(0, VIOCUNLOG, &parms, 0);
}

int
k_setpag()
{
#ifdef AFS_SYSCALL
  if (afs_entry_point == SINGLE_ENTRY_POINT)
    return syscall(AFS_SYSCALL, AFSCALL_SETPAG);
#endif

#ifdef AFS_SETPAG
  if (afs_entry_point == MULTIPLE_ENTRY_POINT)
    return syscall(AFS_SETPAG);
#endif

#ifdef AFS_SYSCALL2
  if (afs_entry_point == SINGLE_ENTRY_POINT2)
    return syscall(AFS_SYSCALL2, AFSCALL_SETPAG);
#endif

#ifdef _AIX
  if (afs_entry_point == AIX_ENTRY_POINTS)
    return lsetpag();
#endif

  errno = ENOSYS;
  kill(getpid(), SIGSYS);	/* You loose! */
  return -1;
}
#endif /* defined(linux) */
static jmp_buf catch_SIGSYS;

static void
SIGSYS_handler()
{
  errno = 0;
  longjmp(catch_SIGSYS, 1);
}

int
k_hasafs()
{
  int saved_errno;
  void (*saved_func)();
  struct ViceIoctl parms;
  
#if defined(linux)
  return 0;
#else
  /*
   * Already checked presence of AFS syscalls?
   */
  if (afs_entry_point != UNKNOWN_ENTRY_POINT)
    return afs_entry_point != NO_ENTRY_POINT;

  /*
   * Probe kernel for AFS specific syscalls,
   * they (currently) come in two flavors.
   * If the syscall is absent we recive a SIGSYS.
   */
  afs_entry_point = NO_ENTRY_POINT;
  bzero(&parms, sizeof(parms));
  
  saved_errno = errno;
  saved_func = signal(SIGSYS, SIGSYS_handler);

#ifdef AFS_SYSCALL
  if (setjmp(catch_SIGSYS) == 0)
    {
      syscall(AFS_SYSCALL, AFSCALL_PIOCTL,
	      0, VIOCSETTOK, &parms, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
      if (errno == EINVAL)
	{
	  afs_entry_point = SINGLE_ENTRY_POINT;
	  goto done;
	}
    }
#endif /* AFS_SYSCALL */

#ifdef AFS_PIOCTL
  if (setjmp(catch_SIGSYS) == 0)
    {
      syscall(AFS_PIOCTL,
	      0, VIOCSETTOK, &parms, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
      if (errno == EINVAL)
	{
	  afs_entry_point = MULTIPLE_ENTRY_POINT;
	  goto done;
	}
    }
#endif /* AFS_PIOCTL */

#ifdef AFS_SYSCALL2
  if (setjmp(catch_SIGSYS) == 0)
    {
      syscall(AFS_SYSCALL2, AFSCALL_PIOCTL,
	      0, VIOCSETTOK, &parms, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
      if (errno == EINVAL)
	{
	  afs_entry_point = SINGLE_ENTRY_POINT2;
	  goto done;
	}
    }
#endif /* AFS_SYSCALL */

#ifdef _AIX
  if (setjmp(catch_SIGSYS) == 0)
    {
      lpioctl(0, 0, 0, 0);
      if (errno == EINVAL)
	{
	  afs_entry_point = AIX_ENTRY_POINTS;
	  goto done;
	}
    }
#endif

 done:
  (void) signal(SIGSYS, saved_func);
  errno = saved_errno;
  return afs_entry_point != NO_ENTRY_POINT;
#endif /* linux */
}
