/*
**	$Id: netbsd.c,v 1.1.1.1 1995/10/18 08:43:18 deraadt Exp $
**
** netbsd.c		Low level kernel access functions for NetBSD
**
** This program is in the public domain and may be used freely by anyone
** who wants to. 
**
** Last update: 17 March 1993
**
** Please send bug fixes/bug reports to: Peter Eriksson <pen@lysator.liu.se>
*/

#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <limits.h>
#include <nlist.h>
#include <pwd.h>
#include <signal.h>
#include <syslog.h>

#include "kvm.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <sys/socketvar.h>

#define _KERNEL

#include <sys/file.h>

#undef _KERNEL
#include <sys/sysctl.h>

#include <fcntl.h>

#include <sys/user.h>

#include <sys/wait.h>

#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>

#include <netinet/in_systm.h>
#include <netinet/ip.h>

#include <netinet/in_pcb.h>

#include <netinet/tcp.h>
#include <netinet/ip_var.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>

#include <arpa/inet.h>

#include "identd.h"
#include "error.h"


extern void *calloc();
extern void *malloc();


struct nlist nl[] =
{
#define N_FILE 0
#define N_NFILE 1
#define N_TCBTABLE 2
      
  { "_filehead" },
  { "_nfiles" },
  { "_tcbtable" },
  { "" }
};

static kvm_t *kd;

static struct file *xfile;
static int nfile;

static struct inpcbtable tcbtable;
  

int k_open()
{
  char errbuf[_POSIX2_LINE_MAX];

  /*
  ** Open the kernel memory device
  */
  if ((kd = kvm_openfiles(path_unix, path_kmem, NULL, O_RDONLY, errbuf)) ==
      NULL)
    ERROR1("main: kvm_open: %s", errbuf);
  
  /*
  ** Extract offsets to the needed variables in the kernel
  */
  if (kvm_nlist(kd, nl) < 0)
    ERROR("main: kvm_nlist");

  return 0;
}


/*
** Get a piece of kernel memory with error handling.
** Returns 1 if call succeeded, else 0 (zero).
*/
static int getbuf(addr, buf, len, what)
  long addr;
  char *buf;
  int len;
  char *what;
{
  if (kvm_read(kd, addr, buf, len) < 0)
  {
    if (syslog_flag)
      syslog(LOG_ERR, "getbuf: kvm_read(%08x, %d) - %s : %m",
	     addr, len, what);

    return 0;
  }
  
  return 1;
}



/*
** Traverse the inpcb list until a match is found.
** Returns NULL if no match.
*/
static struct socket *
getlist(tcbtablep, ktcbtablep, faddr, fport, laddr, lport)
	struct inpcbtable *tcbtablep, *ktcbtablep;
	struct in_addr *faddr;
	int fport;
	struct in_addr *laddr;
	int lport;
{
	struct inpcb *kpcbp, pcb;

	if (!tcbtablep)
		return NULL;
 
	for (kpcbp = tcbtablep->inpt_queue.cqh_first;
	     kpcbp != (struct inpcb *)ktcbtablep;
	     kpcbp = pcb.inp_queue.cqe_next) {
		if (!getbuf((long) kpcbp, &pcb, sizeof(struct inpcb), "tcb"))
			break;
		if (pcb.inp_faddr.s_addr == faddr->s_addr &&
		    pcb.inp_laddr.s_addr == laddr->s_addr &&
		    pcb.inp_fport        == fport &&
		    pcb.inp_lport        == lport )
			return pcb.inp_socket;
	}
  return NULL;
}



/*
** Return the user number for the connection owner
*/
int k_getuid(faddr, fport, laddr, lport, uid)
  struct in_addr *faddr;
  int fport;
  struct in_addr *laddr;
  int lport;
  int *uid;
{
  long addr;
  struct socket *sockp;
  int i, mib[2];
  struct ucred ucb;
  
  /* -------------------- FILE DESCRIPTOR TABLE -------------------- */
  if (!getbuf(nl[N_NFILE].n_value, &nfile, sizeof(nfile), "nfile"))
    return -1;
  
  if (!getbuf(nl[N_FILE].n_value, &addr, sizeof(addr), "&file"))
    return -1;

  {
    size_t siz;
    int rv;

    mib[0] = CTL_KERN;
    mib[1] = KERN_FILE;
    if ((rv = sysctl(mib, 2, NULL, &siz, NULL, 0)) == -1)
    {
      ERROR1("k_getuid: sysctl 1 (%d)", rv);
      return -1;
    }
    xfile = malloc(siz);
    if (!xfile)
      ERROR1("k_getuid: malloc(%d)", siz);
    if ((rv = sysctl(mib, 2, xfile, &siz, NULL, 0)) == -1)
    {
      ERROR1("k_getuid: sysctl 2 (%d)", rv);
      return -1;
    }
    xfile = (struct file *)((char *)xfile + sizeof(filehead));
  }
  
  /* -------------------- TCP PCB LIST -------------------- */
  if (!getbuf(nl[N_TCBTABLE].n_value, &tcbtable, sizeof(tcbtable), "tcbtable"))
    return -1;
  
  sockp = getlist(&tcbtable, nl[N_TCBTABLE].n_value, faddr, fport, laddr,
      lport);
  
  if (!sockp)
    return -1;

  /*
  ** Locate the file descriptor that has the socket in question
  ** open so that we can get the 'ucred' information
  */
  for (i = 0; i < nfile; i++)
  {
    if (xfile[i].f_count == 0)
      continue;
    
    if (xfile[i].f_type == DTYPE_SOCKET &&
	(struct socket *) xfile[i].f_data == sockp)
    {
      if (!getbuf(xfile[i].f_cred, &ucb, sizeof(ucb), "ucb"))
	return -1;

      *uid = ucb.cr_uid;
      return 0;
    }
  }

  return -1;
}

