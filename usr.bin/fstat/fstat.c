/*	$OpenBSD: fstat.c,v 1.95 2018/09/16 02:44:06 millert Exp $	*/

/*
 * Copyright (c) 2009 Todd C. Miller <Todd.Miller@courtesan.com>
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

/*-
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/vnode.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/eventvar.h>
#include <sys/sysctl.h>
#include <sys/filedesc.h>
#define _KERNEL /* for DTYPE_* */
#include <sys/file.h>
#undef _KERNEL

#include <net/route.h>
#include <netinet/in.h>

#include <netdb.h>
#include <arpa/inet.h>

#include <sys/pipe.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <kvm.h>
#include <limits.h>
#include <nlist.h>
#include <pwd.h>
#include <search.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <err.h>

#include "fstat.h"

#define MAXIMUM(a, b)	(((a) > (b)) ? (a) : (b))

struct fileargs fileargs = SLIST_HEAD_INITIALIZER(fileargs);

int	fsflg;	/* show files on same filesystem as file(s) argument */
int	pflg;	/* show files open by a particular pid */
int	uflg;	/* show files open by a particular (effective) user */
int	checkfile; /* true if restricting to particular files or filesystems */
int	nflg;	/* (numerical) display f.s. and rdev as dev_t */
int	oflg;	/* display file offset */
int	sflg;	/* display file xfer/bytes counters */
int	vflg;	/* display errors in locating kernel data objects etc... */
int	cflg; 	/* fuser only */

int	fuser;	/* 1 if we are fuser, 0 if we are fstat */
int	signo;	/* signal to send (fuser only) */

kvm_t *kd;
uid_t uid;

void fstat_dofile(struct kinfo_file *);
void fstat_header(void);
void getinetproto(int);
__dead void usage(void);
int getfname(char *);
void kqueuetrans(struct kinfo_file *);
void pipetrans(struct kinfo_file *);
struct kinfo_file *splice_find(char, u_int64_t);
void splice_insert(char, u_int64_t, struct kinfo_file *);
void find_splices(struct kinfo_file *, int);
void print_inet_details(struct kinfo_file *);
void print_inet6_details(struct kinfo_file *);
void print_sock_details(struct kinfo_file *);
void socktrans(struct kinfo_file *);
void vtrans(struct kinfo_file *);
const char *inet6_addrstr(struct in6_addr *);
int signame_to_signum(char *);
void hide(void *p);

int hideroot;

void
hide(void *p)
{
	printf("%p", hideroot ? NULL : p);
}

int
main(int argc, char *argv[])
{
	struct passwd *passwd;
	struct kinfo_file *kf, *kflast;
	int arg, ch, what;
	char *memf, *nlistf, *optstr;
	char buf[_POSIX2_LINE_MAX];
	const char *errstr;
	int cnt, flags;

	hideroot = getuid();

	arg = -1;
	what = KERN_FILE_BYPID;
	nlistf = memf = NULL;
	oflg = 0;

	/* are we fstat(1) or fuser(1)? */
	if (strcmp(__progname, "fuser") == 0) {
		fuser = 1;
		optstr = "cfks:uM:N:";
	} else {
		fuser = 0;
		optstr = "fnop:su:vN:M:";
	}

	/* Keep passwd file open for faster lookups. */
	setpassent(1);

	/*
	 * fuser and fstat share three flags: -f, -s and -u.  In both cases
	 * -f is a boolean, but for -u fstat wants an argument while fuser
	 * does not and for -s fuser wants an argument whereas fstat does not.
	 */
	while ((ch = getopt(argc, argv, optstr)) != -1)
		switch ((char)ch) {
		case 'c':
			if (fsflg)
				usage();
			cflg = 1;
			break;
		case 'f':
			if (cflg)
				usage();
			fsflg = 1;
			break;
		case 'k':
			sflg = 1;
			signo = SIGKILL;
			break;
		case 'M':
			memf = optarg;
			break;
		case 'N':
			nlistf = optarg;
			break;
		case 'n':
			nflg = 1;
			break;
		case 'o':
			oflg = 1;
			break;
		case 'p':
			if (pflg++)
				usage();
			arg = strtonum(optarg, 0, INT_MAX, &errstr);
			if (errstr != NULL) {
				warnx("-p requires a process id, %s: %s",
					errstr, optarg);
				usage();
			}
			what = KERN_FILE_BYPID;
			break;
		case 's':
			sflg = 1;
			if (fuser) {
				signo = signame_to_signum(optarg);
				if (signo == -1) {
					warnx("invalid signal %s", optarg);
					usage();
				}
			}
			break;
		case 'u':
			if (uflg++)
				usage();
			if (!fuser) {
				uid_t uid;

				if (uid_from_user(optarg, &uid) == -1) {
					uid = strtonum(optarg, 0, UID_MAX,
					    &errstr);
					if (errstr != NULL) {
						errx(1, "%s: unknown uid",
						    optarg);
					}
				}
				arg = uid;
				what = KERN_FILE_BYUID;
			}
			break;
		case 'v':
			vflg = 1;
			break;
		default:
			usage();
		}

	/*
	 * get the uid, for oflg and sflg
	 */
	uid = getuid();

	/*
	 * Use sysctl unless inspecting an alternate kernel.
	 */
	if (nlistf == NULL || memf == NULL)
		flags = KVM_NO_FILES;
	else
		flags = O_RDONLY;

	if ((kd = kvm_openfiles(nlistf, memf, NULL, flags, buf)) == NULL)
		errx(1, "%s", buf);

	if (*(argv += optind)) {
		for (; *argv; ++argv) {
			if (getfname(*argv))
				checkfile = 1;
		}
		/* file(s) specified, but none accessible */
		if (!checkfile)
			exit(1);
	} else if (fuser)
		usage();

	if (!fuser && fsflg && !checkfile) {
		/* fstat -f with no files means use wd */
		if (getfname(".") == 0)
			exit(1);
		checkfile = 1;
	}

	if ((kf = kvm_getfiles(kd, what, arg, sizeof(*kf), &cnt)) == NULL)
		errx(1, "%s", kvm_geterr(kd));

	if (fuser) {
		/*
		 * fuser
		 *  uflg: need "getpw"
		 *  sflg: need "proc" (might call kill(2))
		 */
		if (uflg && sflg) {
			if (pledge("stdio rpath getpw proc", NULL) == -1)
				err(1, "pledge");
		} else if (uflg) {
			if (pledge("stdio rpath getpw", NULL) == -1)
				err(1, "pledge");
		} else if (sflg) {
			if (pledge("stdio rpath proc", NULL) == -1)
				err(1, "pledge");
		} else {
			if (pledge("stdio rpath", NULL) == -1)
				err(1, "pledge");
		}
	} else {
		/* fstat */
		if (pledge("stdio rpath getpw", NULL) == -1)
			err(1, "pledge");
	}

	find_splices(kf, cnt);
	if (!fuser)
		fstat_header();
	for (kflast = &kf[cnt]; kf < kflast; ++kf) {
		if (fuser)
			fuser_check(kf);
		else
			fstat_dofile(kf);
	}
	if (fuser)
		fuser_run();

	exit(0);
}

void
fstat_header(void)
{
	if (nflg)
		printf("%s",
"USER     CMD          PID   FD  DEV      INUM        MODE   R/W    SZ|DV");
	else
		printf("%s",
"USER     CMD          PID   FD MOUNT        INUM  MODE         R/W    SZ|DV");
	if (oflg)
		printf("%s", ":OFFSET  ");
	if (checkfile && fsflg == 0)
		printf(" NAME");
	if (sflg)
		printf("    XFERS   KBYTES");
	putchar('\n');
}

const char *Uname, *Comm;
uid_t	*procuid;
pid_t	Pid;

#define PREFIX(i) do { \
	printf("%-8.8s %-10s %5ld", Uname, Comm, (long)Pid); \
	switch (i) { \
	case KERN_FILE_TEXT: \
		printf(" text"); \
		break; \
	case KERN_FILE_CDIR: \
		printf("   wd"); \
		break; \
	case KERN_FILE_RDIR: \
		printf(" root"); \
		break; \
	case KERN_FILE_TRACE: \
		printf("   tr"); \
		break; \
	default: \
		printf(" %4d", i); \
		break; \
	} \
} while (0)

/*
 * print open files attributed to this process
 */
void
fstat_dofile(struct kinfo_file *kf)
{

	Uname = user_from_uid(kf->p_uid, 0);
	procuid = &kf->p_uid;
	Pid = kf->p_pid;
	Comm = kf->p_comm;

	switch (kf->f_type) {
	case DTYPE_VNODE:
		vtrans(kf);
		break;
	case DTYPE_SOCKET:
		if (checkfile == 0)
			socktrans(kf);
		break;
	case DTYPE_PIPE:
		if (checkfile == 0)
			pipetrans(kf);
		break;
	case DTYPE_KQUEUE:
		if (checkfile == 0)
			kqueuetrans(kf);
		break;
	default:
		if (vflg) {
			warnx("unknown file type %d for file %d of pid %ld",
			    kf->f_type, kf->fd_fd, (long)Pid);
		}
		break;
	}
}

void
vtrans(struct kinfo_file *kf)
{
	const char *badtype = NULL;
	char rwep[5], mode[12];
	char *filename = NULL;

	if (kf->v_type == VNON)
		badtype = "none";
	else if (kf->v_type == VBAD)
		badtype = "bad";
	else if (kf->v_tag == VT_NON && !(kf->v_flag & VCLONE))
		badtype = "none";	/* not a clone */

	if (checkfile) {
		int fsmatch = 0;
		struct filearg *fa;

		if (badtype)
			return;
		SLIST_FOREACH(fa, &fileargs, next) {
			if (fa->dev == kf->va_fsid) {
				fsmatch = 1;
				if (fa->ino == kf->va_fileid) {
					filename = fa->name;
					break;
				}
			}
		}
		if (fsmatch == 0 || (filename == NULL && fsflg == 0))
			return;
	}
	PREFIX(kf->fd_fd);
	if (badtype) {
		(void)printf(" -           -  %10s    -\n", badtype);
		return;
	}

	if (nflg)
		(void)printf(" %2ld,%-2ld", (long)major(kf->va_fsid),
		    (long)minor(kf->va_fsid));
	else if (!(kf->v_flag & VCLONE))
		(void)printf(" %-8s", kf->f_mntonname);
	else
		(void)printf(" clone   ");
	if (nflg)
		(void)snprintf(mode, sizeof(mode), "%o", kf->va_mode);
	else
		strmode(kf->va_mode, mode);
	printf(" %8llu%s %11s", kf->va_fileid,
	    kf->va_nlink == 0 ? "*" : " ",
	    mode);
	rwep[0] = '\0';
	if (kf->f_flag & FREAD)
		strlcat(rwep, "r", sizeof rwep);
	if (kf->f_flag & FWRITE)
		strlcat(rwep, "w", sizeof rwep);
	if (kf->fd_ofileflags & UF_EXCLOSE)
		strlcat(rwep, "e", sizeof rwep);
	if (kf->fd_ofileflags & UF_PLEDGED)
		strlcat(rwep, "p", sizeof rwep);
	printf(" %4s", rwep);
	switch (kf->v_type) {
	case VBLK:
	case VCHR: {
		char *name;

		if (nflg || ((name = devname(kf->va_rdev,
		    kf->v_type == VCHR ?  S_IFCHR : S_IFBLK)) == NULL))
			printf("   %2d,%-3d", major(kf->va_rdev), minor(kf->va_rdev));
		else
			printf("  %7s", name);
		if (oflg)
			printf("         ");
		break;
	}
	default:
		printf(" %8llu", kf->va_size);
		if (oflg) {
			if (uid == 0 || uid == *procuid)
				printf(":%-8llu", kf->f_offset);
			else
				printf(":%-8s", "*");
		}
	}
	if (sflg) {
		if (uid == 0 || uid == *procuid) {
			printf(" %8llu %8llu",
			    (kf->f_rxfer + kf->f_rwfer),
			    (kf->f_rbytes + kf->f_wbytes) / 1024);
		} else {
			printf(" %8s %8s", "*", "*");
		}
	}
	if (filename && !fsflg)
		printf(" %s", filename);
	putchar('\n');
}

void
pipetrans(struct kinfo_file *kf)
{
	void *maxaddr;

	PREFIX(kf->fd_fd);

	printf(" ");

	/*
	 * We don't have enough space to fit both peer and own address, so
	 * we select the higher address so both ends of the pipe have the
	 * same visible addr. (it's the higher address because when the other
	 * end closes, it becomes 0)
	 */
	maxaddr = (void *)(uintptr_t)MAXIMUM(kf->f_data, kf->pipe_peer);

	printf("pipe ");
	hide(maxaddr);
	printf(" state: %s%s%s",
	    (kf->pipe_state & PIPE_WANTR) ? "R" : "",
	    (kf->pipe_state & PIPE_WANTW) ? "W" : "",
	    (kf->pipe_state & PIPE_EOF) ? "E" : "");
	if (sflg)
		printf("\t%8llu %8llu",
		    (kf->f_rxfer + kf->f_rwfer),
		    (kf->f_rbytes + kf->f_wbytes) / 1024);
	printf("\n");
	return;
}

void
kqueuetrans(struct kinfo_file *kf)
{
	PREFIX(kf->fd_fd);

	printf(" ");

	printf("kqueue ");
	hide((void *)(uintptr_t)kf->f_data);
	printf(" %d state: %s%s\n",
	    kf->kq_count,
	    (kf->kq_state & KQ_SEL) ? "S" : "",
	    (kf->kq_state & KQ_SLEEP) ? "W" : "");
	return;
}

const char *
inet6_addrstr(struct in6_addr *p)
{
	struct sockaddr_in6 sin6;
	static char hbuf[NI_MAXHOST];
	const int niflags = NI_NUMERICHOST;

	memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_family = AF_INET6;
	sin6.sin6_len = sizeof(struct sockaddr_in6);
	sin6.sin6_addr = *p;
	if (IN6_IS_ADDR_LINKLOCAL(p) &&
	    *(u_int16_t *)&sin6.sin6_addr.s6_addr[2] != 0) {
		sin6.sin6_scope_id =
		    ntohs(*(u_int16_t *)&sin6.sin6_addr.s6_addr[2]);
		sin6.sin6_addr.s6_addr[2] = sin6.sin6_addr.s6_addr[3] = 0;
	}

	if (getnameinfo((struct sockaddr *)&sin6, sin6.sin6_len,
	    hbuf, sizeof(hbuf), NULL, 0, niflags))
		return "invalid";

	return hbuf;
}

void
splice_insert(char type, u_int64_t ptr, struct kinfo_file *data)
{
	ENTRY entry, *found;

	if (asprintf(&entry.key, "%c%llx", type, hideroot ? 0 : ptr) == -1)
		err(1, NULL);
	entry.data = data;
	if ((found = hsearch(entry, ENTER)) == NULL)
		err(1, "hsearch");
	/* if it's ambiguous, set the data to NULL */
	if (found->data != data)
		found->data = NULL;
}

struct kinfo_file *
splice_find(char type, u_int64_t ptr)
{
	ENTRY entry, *found;
	char buf[20];

	snprintf(buf, sizeof(buf), "%c%llx", type, hideroot ? 0 : ptr);
	entry.key = buf;
	found = hsearch(entry, FIND);
	return (found != NULL ? found->data : NULL);
}

void
find_splices(struct kinfo_file *kf, int cnt)
{
	int i, created;

	created = 0;
	for (i = 0; i < cnt; i++) {
		if (kf[i].f_type != DTYPE_SOCKET ||
		    (kf[i].so_splice == 0 && kf[i].so_splicelen != -1))
			continue;
		if (created++ == 0) {
			if (hcreate(1000) == 0)
				err(1, "hcreate");
		}
		splice_insert('>', kf[i].f_data, &kf[i]);
		if (kf[i].so_splice != 0)
			splice_insert('<', kf[i].so_splice, &kf[i]);
	}
}

void
print_inet_details(struct kinfo_file *kf)
{
	struct in_addr laddr, faddr;

	memcpy(&laddr, kf->inp_laddru, sizeof(laddr));
	memcpy(&faddr, kf->inp_faddru, sizeof(faddr));
	if (kf->so_protocol == IPPROTO_TCP) {
		printf(" ");
		hide((void *)(uintptr_t)kf->inp_ppcb);
		printf(" %s:%d", laddr.s_addr == INADDR_ANY ? "*" :
		    inet_ntoa(laddr), ntohs(kf->inp_lport));
		if (kf->inp_fport) {
			if (kf->so_state & SS_CONNECTOUT)
				printf(" --> ");
			else
				printf(" <-- ");
			printf("%s:%d",
			    faddr.s_addr == INADDR_ANY ? "*" :
			    inet_ntoa(faddr), ntohs(kf->inp_fport));
		}
	} else if (kf->so_protocol == IPPROTO_UDP) {
		printf(" %s:%d", laddr.s_addr == INADDR_ANY ? "*" :
		    inet_ntoa(laddr), ntohs(kf->inp_lport));
		if (kf->inp_fport) {
			printf(" <-> %s:%d",
			    faddr.s_addr == INADDR_ANY ? "*" :
			    inet_ntoa(faddr), ntohs(kf->inp_fport));
		}
	} else if (kf->so_pcb) {
		printf(" ");
		hide((void *)(uintptr_t)kf->so_pcb);
	}
}

void
print_inet6_details(struct kinfo_file *kf)
{
	char xaddrbuf[NI_MAXHOST + 2];
	struct in6_addr laddr6, faddr6;

	memcpy(&laddr6, kf->inp_laddru, sizeof(laddr6));
	memcpy(&faddr6, kf->inp_faddru, sizeof(faddr6));
	if (kf->so_protocol == IPPROTO_TCP) {
		printf(" ");
		hide((void *)(uintptr_t)kf->inp_ppcb);
		snprintf(xaddrbuf, sizeof(xaddrbuf), "[%s]",
		    inet6_addrstr(&laddr6));
		printf(" %s:%d",
		    IN6_IS_ADDR_UNSPECIFIED(&laddr6) ? "*" :
		    xaddrbuf, ntohs(kf->inp_lport));
		if (kf->inp_fport) {
			if (kf->so_state & SS_CONNECTOUT)
				printf(" --> ");
			else
				printf(" <-- ");
			snprintf(xaddrbuf, sizeof(xaddrbuf), "[%s]",
			    inet6_addrstr(&faddr6));
			printf("%s:%d",
			    IN6_IS_ADDR_UNSPECIFIED(&faddr6) ? "*" :
			    xaddrbuf, ntohs(kf->inp_fport));
		}
	} else if (kf->so_protocol == IPPROTO_UDP) {
		snprintf(xaddrbuf, sizeof(xaddrbuf), "[%s]",
		    inet6_addrstr(&laddr6));
		printf(" %s:%d",
		    IN6_IS_ADDR_UNSPECIFIED(&laddr6) ? "*" :
		    xaddrbuf, ntohs(kf->inp_lport));
		if (kf->inp_fport) {
			snprintf(xaddrbuf, sizeof(xaddrbuf), "[%s]",
			    inet6_addrstr(&faddr6));
			printf(" <-> %s:%d",
			    IN6_IS_ADDR_UNSPECIFIED(&faddr6) ? "*" :
			    xaddrbuf, ntohs(kf->inp_fport));
		}
	} else if (kf->so_pcb) {
		printf(" ");
		hide((void *)(uintptr_t)kf->so_pcb);
	}
}

void
print_sock_details(struct kinfo_file *kf)
{
	if (kf->so_family == AF_INET)
		print_inet_details(kf);
	else if (kf->so_family == AF_INET6)
		print_inet6_details(kf);
}

void
socktrans(struct kinfo_file *kf)
{
	static char *stypename[] = {
		"unused",	/* 0 */
		"stream",	/* 1 */
		"dgram",	/* 2 */
		"raw",		/* 3 */
		"rdm",		/* 4 */
		"seqpak"	/* 5 */
	};
#define	STYPEMAX 5
	char *stype, stypebuf[24];

	PREFIX(kf->fd_fd);

	if (kf->so_type > STYPEMAX) {
		snprintf(stypebuf, sizeof(stypebuf), "?%d", kf->so_type);
		stype = stypebuf;
	} else {
		stype = stypename[kf->so_type];
	}

	/*
	 * protocol specific formatting
	 *
	 * Try to find interesting things to print.  For tcp, the interesting
	 * thing is the address of the tcpcb, for udp and others, just the
	 * inpcb (socket pcb).  For unix domain, its the address of the socket
	 * pcb and the address of the connected pcb (if connected).  Otherwise
	 * just print the protocol number and address of the socket itself.
	 * The idea is not to duplicate netstat, but to make available enough
	 * information for further analysis.
	 */
	switch (kf->so_family) {
	case AF_INET:
		printf("* internet %s", stype);
		getinetproto(kf->so_protocol);
		print_inet_details(kf);
		if (kf->inp_rtableid)
			printf(" rtable %u", kf->inp_rtableid);
		break;
	case AF_INET6:
		printf("* internet6 %s", stype);
		getinetproto(kf->so_protocol);
		print_inet6_details(kf);
		if (kf->inp_rtableid)
			printf(" rtable %u", kf->inp_rtableid);
		break;
	case AF_UNIX:
		/* print address of pcb and connected pcb */
		printf("* unix %s", stype);
		if (kf->so_pcb) {
			printf(" ");
			hide((void *)(uintptr_t)kf->so_pcb);
			if (kf->unp_conn) {
				char shoconn[4], *cp;

				cp = shoconn;
				if (!(kf->so_state & SS_CANTRCVMORE))
					*cp++ = '<';
				*cp++ = '-';
				if (!(kf->so_state & SS_CANTSENDMORE))
					*cp++ = '>';
				*cp = '\0';
				printf(" %s ", shoconn);
				hide((void *)(uintptr_t)kf->unp_conn);
			}
		}
		break;
	case AF_MPLS:
		/* print protocol number and socket address */
		printf("* mpls %s", stype);
		printf(" %d ", kf->so_protocol);
		hide((void *)(uintptr_t)kf->f_data);
		break;
	case AF_ROUTE:
		/* print protocol number and socket address */
		printf("* route %s", stype);
		printf(" %d ", kf->so_protocol);
		hide((void *)(uintptr_t)kf->f_data);
		break;
	default:
		/* print protocol number and socket address */
		printf("* %d %s", kf->so_family, stype);
		printf(" %d ", kf->so_protocol);
		hide((void *)(uintptr_t)kf->f_data);
	}
	if (kf->so_splice != 0 || kf->so_splicelen == -1) {
		struct kinfo_file *from, *to;

		from = splice_find('<', kf->f_data);
		to = NULL;
		if (kf->so_splice != 0)
			to = splice_find('>', kf->so_splice);

		if (to != NULL && from == to) {
			printf(" <==>");
			print_sock_details(to);
		} else if (kf->so_splice != 0) {
			printf(" ==>");
			if (to != NULL)
				print_sock_details(to);
		} else if (kf->so_splicelen == -1) {
			printf(" <==");
			if (from != NULL)
				print_sock_details(from);
		}
	}
	if (sflg)
		printf("\t%8llu %8llu",
		    (kf->f_rxfer + kf->f_rwfer),
		    (kf->f_rbytes + kf->f_wbytes) / 1024);
	printf("\n");
}

/*
 * getinetproto --
 *	print name of protocol number
 */
void
getinetproto(int number)
{
	static int isopen;
	struct protoent *pe;

	if (!isopen)
		setprotoent(++isopen);
	if ((pe = getprotobynumber(number)) != NULL)
		printf(" %s", pe->p_name);
	else
		printf(" %d", number);
}

int
getfname(char *filename)
{
	static struct statfs *mntbuf;
	static int nmounts;
	int i;
	struct stat sb;
	struct filearg *cur;

	if (stat(filename, &sb)) {
		warn("%s", filename);
		return (0);
	}

	/*
	 * POSIX specifies "For block special devices, all processes using any
	 * file on that device are listed".  However the -f flag description
	 * states "The report shall be only for the named files", so we only
	 * look up a block device if the -f flag has not be specified.
	 */
	if (fuser && !fsflg && S_ISBLK(sb.st_mode)) {
		if (mntbuf == NULL) {
			nmounts = getmntinfo(&mntbuf, MNT_NOWAIT);
			if (nmounts == -1)
				err(1, "getmntinfo");
		}
		for (i = 0; i < nmounts; i++) {
			if (!strcmp(mntbuf[i].f_mntfromname, filename)) {
				if (stat(mntbuf[i].f_mntonname, &sb) == -1) {
					warn("%s", filename);
					return (0);
				}
				cflg = 1;
				break;
			}
		}
	}

	if ((cur = malloc(sizeof(*cur))) == NULL)
		err(1, NULL);

	cur->ino = sb.st_ino;
	cur->dev = sb.st_dev & 0xffff;
	cur->name = filename;
	TAILQ_INIT(&cur->fusers);
	SLIST_INSERT_HEAD(&fileargs, cur, next);
	return (1);
}

int
signame_to_signum(char *sig)
{
	int n;
	const char *errstr = NULL;

	if (isdigit((unsigned char)*sig)) {
		n = strtonum(sig, 0, NSIG - 1, &errstr);
		return (errstr ? -1 : n);
	}
	if (!strncasecmp(sig, "sig", 3))
		sig += 3;
	for (n = 1; n < NSIG; n++) {
		if (!strcasecmp(sys_signame[n], sig))
			return (n);
	}
	return (-1);
}

void
usage(void)
{
	if (fuser) {
		fprintf(stderr, "usage: fuser [-cfku] [-M core] "
		    "[-N system] [-s signal] file ...\n");
	} else {
		fprintf(stderr, "usage: fstat [-fnosv] [-M core] [-N system] "
		    "[-p pid] [-u user] [file ...]\n");
	}
	exit(1);
}
