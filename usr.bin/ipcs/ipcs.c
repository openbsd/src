/*	$OpenBSD: ipcs.c,v 1.26 2015/01/16 06:40:08 deraadt Exp $	*/
/*	$NetBSD: ipcs.c,v 1.25 2000/06/16 03:58:20 simonb Exp $	*/

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Simon Burge.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1994 SigmaSoft, Th. Lockert <tholo@sigmasoft.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/sysctl.h>
#define _KERNEL			/* XXX */
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/msg.h>
#undef _KERNEL

#include <err.h>
#include <fcntl.h>
#include <grp.h>
#include <kvm.h>
#include <limits.h>
#include <nlist.h>
#include <paths.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

void	cvt_time(time_t, char *, size_t);
char   *fmt_perm(mode_t);
void	ipcs_kvm(void);
int	main(int, char **);
void	msg_sysctl(void);
void	sem_sysctl(void);
void	shm_sysctl(void);
void	show_msginfo(time_t, time_t, time_t, int, key_t, mode_t, uid_t,
	    gid_t, uid_t, gid_t, u_long, u_long, u_long, pid_t, pid_t);
void	show_msginfo_hdr(void);
void	show_msgtotal(struct msginfo *);
void	show_seminfo_hdr(void);
void	show_seminfo(time_t, time_t, int, key_t, mode_t, uid_t, gid_t,
	    uid_t, gid_t, int16_t);
void	show_semtotal(struct seminfo *);
void	show_shminfo(time_t, time_t, time_t, int, key_t, mode_t, uid_t,
	    gid_t, uid_t, gid_t, u_int32_t, int, pid_t, pid_t);
void	show_shminfo_hdr(void);
void	show_shmtotal(struct shminfo *);
__dead	void usage(void);

char *
fmt_perm(mode_t mode)
{
	static char buffer[12];

	buffer[0] = '-';
	buffer[1] = '-';
	buffer[2] = ((mode & 0400) ? 'r' : '-');
	buffer[3] = ((mode & 0200) ? 'w' : '-');
	buffer[4] = ((mode & 0100) ? 'a' : '-');
	buffer[5] = ((mode & 0040) ? 'r' : '-');
	buffer[6] = ((mode & 0020) ? 'w' : '-');
	buffer[7] = ((mode & 0010) ? 'a' : '-');
	buffer[8] = ((mode & 0004) ? 'r' : '-');
	buffer[9] = ((mode & 0002) ? 'w' : '-');
	buffer[10] = ((mode & 0001) ? 'a' : '-');
	buffer[11] = '\0';
	return (&buffer[0]);
}

void
cvt_time(time_t t, char *buf, size_t buflen)
{
	struct tm *tm;

	if (t == 0)
		(void)strlcpy(buf, "no-entry", buflen);
	else {
		tm = localtime(&t);
		(void)snprintf(buf, buflen, "%2d:%02d:%02d",
			tm->tm_hour, tm->tm_min, tm->tm_sec);
	}
}
#define	SHMINFO		1
#define	SHMTOTAL	2
#define	MSGINFO		4
#define	MSGTOTAL	8
#define	SEMINFO		16
#define	SEMTOTAL	32

#define BIGGEST		1
#define CREATOR		2
#define OUTSTANDING	4
#define PID		8
#define TIME		16

char	*core = NULL, *namelist = NULL;
int	display = SHMINFO | MSGINFO | SEMINFO;
int	option = 0;

int
main(int argc, char **argv)
{
	int ch;

	while ((ch = getopt(argc, argv, "MmQqSsabC:cN:optT")) != -1)
		switch (ch) {
		case 'M':
			display = SHMTOTAL;
			break;
		case 'm':
			display = SHMINFO;
			break;
		case 'Q':
			display = MSGTOTAL;
			break;
		case 'q':
			display = MSGINFO;
			break;
		case 'S':
			display = SEMTOTAL;
			break;
		case 's':
			display = SEMINFO;
			break;
		case 'T':
			display = SHMTOTAL | MSGTOTAL | SEMTOTAL;
			break;
		case 'a':
			option |= BIGGEST | CREATOR | OUTSTANDING | PID | TIME;
			break;
		case 'b':
			option |= BIGGEST;
			break;
		case 'C':
			core = optarg;
			break;
		case 'c':
			option |= CREATOR;
			break;
		case 'N':
			namelist = optarg;
			break;
		case 'o':
			option |= OUTSTANDING;
			break;
		case 'p':
			option |= PID;
			break;
		case 't':
			option |= TIME;
			break;
		default:
			usage();
		}

	if (argc - optind > 0)
		usage();

	if (namelist == NULL && core == NULL) {
		if (display & (MSGINFO | MSGTOTAL))
			msg_sysctl();
		if (display & (SHMINFO | SHMTOTAL))
			shm_sysctl();
		if (display & (SEMINFO | SEMTOTAL))
			sem_sysctl();
	} else
		ipcs_kvm();

	exit(0);
}

void
show_msgtotal(struct msginfo *msginfo)
{

	printf("msginfo:\n");
	printf("\tmsgmax: %6d\t(max characters in a message)\n",
	    msginfo->msgmax);
	printf("\tmsgmni: %6d\t(# of message queues)\n",
	    msginfo->msgmni);
	printf("\tmsgmnb: %6d\t(max characters in a message queue)\n",
	    msginfo->msgmnb);
	printf("\tmsgtql: %6d\t(max # of messages in system)\n",
	    msginfo->msgtql);
	printf("\tmsgssz: %6d\t(size of a message segment)\n",
	    msginfo->msgssz);
	printf("\tmsgseg: %6d\t(# of message segments in system)\n\n",
	    msginfo->msgseg);
}

void
show_shmtotal(struct shminfo *shminfo)
{

	printf("shminfo:\n");
	printf("\tshmmax: %7d\t(max shared memory segment size)\n",
	    shminfo->shmmax);
	printf("\tshmmin: %7d\t(min shared memory segment size)\n",
	    shminfo->shmmin);
	printf("\tshmmni: %7d\t(max number of shared memory identifiers)\n",
	    shminfo->shmmni);
	printf("\tshmseg: %7d\t(max shared memory segments per process)\n",
	    shminfo->shmseg);
	printf("\tshmall: %7d\t(max amount of shared memory in pages)\n\n",
	    shminfo->shmall);
}

void
show_semtotal(struct seminfo *seminfo)
{

	printf("seminfo:\n");
	printf("\tsemmni: %6d\t(# of semaphore identifiers)\n",
	    seminfo->semmni);
	printf("\tsemmns: %6d\t(# of semaphores in system)\n",
	    seminfo->semmns);
	printf("\tsemmnu: %6d\t(# of undo structures in system)\n",
	    seminfo->semmnu);
	printf("\tsemmsl: %6d\t(max # of semaphores per id)\n",
	    seminfo->semmsl);
	printf("\tsemopm: %6d\t(max # of operations per semop call)\n",
	    seminfo->semopm);
	printf("\tsemume: %6d\t(max # of undo entries per process)\n",
	    seminfo->semume);
	printf("\tsemusz: %6d\t(size in bytes of undo structure)\n",
	    seminfo->semusz);
	printf("\tsemvmx: %6d\t(semaphore maximum value)\n",
	    seminfo->semvmx);
	printf("\tsemaem: %6d\t(adjust on exit max value)\n\n",
	    seminfo->semaem);
}

void
show_msginfo_hdr(void)
{

	printf("Message Queues:\n");
	printf("T       ID     KEY        MODE       OWNER    GROUP");
	if (option & CREATOR)
		printf("  CREATOR   CGROUP");
	if (option & OUTSTANDING)
		printf(" CBYTES  QNUM");
	if (option & BIGGEST)
		printf(" QBYTES");
	if (option & PID)
		printf(" LSPID LRPID");
	if (option & TIME)
		printf("    STIME    RTIME    CTIME");
	printf("\n");
}

void
show_msginfo(time_t stime, time_t rtime, time_t ctime, int ipcid, key_t key,
    mode_t mode, uid_t uid, gid_t gid, uid_t cuid, gid_t cgid,
    u_long cbytes, u_long qnum, u_long qbytes, pid_t lspid,
    pid_t lrpid)
{
	char stime_buf[100], rtime_buf[100], ctime_buf[100];

	if (option & TIME) {
		cvt_time(stime, stime_buf, sizeof(stime_buf));
		cvt_time(rtime, rtime_buf, sizeof(rtime_buf));
		cvt_time(ctime, ctime_buf, sizeof(ctime_buf));
	}

	printf("q %8d %10ld %s %8s %8s", ipcid, key, fmt_perm(mode),
	    user_from_uid(uid, 0), group_from_gid(gid, 0));

	if (option & CREATOR)
		printf(" %8s %8s", user_from_uid(cuid, 0),
		    group_from_gid(cgid, 0));

	if (option & OUTSTANDING)
		printf(" %6lu %5lu", cbytes, qnum);

	if (option & BIGGEST)
		printf(" %6lu", qbytes);

	if (option & PID)
		printf(" %5ld %5ld", (long)lspid, (long)lrpid);

	if (option & TIME)
		printf(" %s %s %s", stime_buf, rtime_buf, ctime_buf);

	printf("\n");
}

void
show_shminfo_hdr(void)
{

	printf("Shared Memory:\n");
	printf("T       ID     KEY        MODE       OWNER    GROUP");
	if (option & CREATOR)
		printf("  CREATOR   CGROUP");
	if (option & OUTSTANDING)
		printf(" NATTCH");
	if (option & BIGGEST)
		printf("   SEGSZ");
	if (option & PID)
		printf("  CPID  LPID");
	if (option & TIME)
		printf("    ATIME    DTIME    CTIME");
	printf("\n");
}

void
show_shminfo(time_t atime, time_t dtime, time_t ctime, int ipcid, key_t key,
    mode_t mode, uid_t uid, gid_t gid, uid_t cuid, gid_t cgid,
    u_int32_t nattch, int segsz, pid_t cpid, pid_t lpid)
{
	char atime_buf[100], dtime_buf[100], ctime_buf[100];

	if (option & TIME) {
		cvt_time(atime, atime_buf, sizeof(atime_buf));
		cvt_time(dtime, dtime_buf, sizeof(dtime_buf));
		cvt_time(ctime, ctime_buf, sizeof(ctime_buf));
	}

	printf("m %8d %10ld %s %8s %8s", ipcid, key, fmt_perm(mode),
	    user_from_uid(uid, 0), group_from_gid(gid, 0));

	if (option & CREATOR)
		printf(" %8s %8s", user_from_uid(cuid, 0),
		    group_from_gid(cgid, 0));

	if (option & OUTSTANDING)
		printf(" %6d", nattch);

	if (option & BIGGEST)
		printf(" %7d", segsz);

	if (option & PID)
		printf(" %5d %5d", cpid, lpid);

	if (option & TIME)
		printf(" %s %s %s",
		    atime_buf,
		    dtime_buf,
		    ctime_buf);

	printf("\n");
}

void
show_seminfo_hdr(void)
{

	printf("Semaphores:\n");
	printf("T       ID     KEY        MODE       OWNER    GROUP");
	if (option & CREATOR)
		printf("  CREATOR   CGROUP");
	if (option & BIGGEST)
		printf(" NSEMS");
	if (option & TIME)
		printf("    OTIME    CTIME");
	printf("\n");
}

void
show_seminfo(time_t otime, time_t ctime, int ipcid, key_t key, mode_t mode,
    uid_t uid, gid_t gid, uid_t cuid, gid_t cgid, int16_t nsems)
{
	char ctime_buf[100], otime_buf[100];

	if (option & TIME) {
		cvt_time(otime, otime_buf, sizeof(otime_buf));
		cvt_time(ctime, ctime_buf, sizeof(ctime_buf));
	}

	printf("s %8d %10ld %s %8s %8s", ipcid, key, fmt_perm(mode),
	    user_from_uid(uid, 0), group_from_gid(gid, 0));

	if (option & CREATOR)
		printf(" %8s %8s", user_from_uid(cuid, 0),
		    group_from_gid(cgid, 0));

	if (option & BIGGEST)
		printf(" %5d", nsems);

	if (option & TIME)
		printf(" %s %s", otime_buf, ctime_buf);

	printf("\n");
}

void
msg_sysctl(void)
{
	struct msg_sysctl_info *msgsi;
	char *buf;
	int mib[3];
	size_t len;
	int i, valid;

	mib[0] = CTL_KERN;
	mib[1] = KERN_SYSVMSG;
	len = sizeof(valid);
	if (sysctl(mib, 2, &valid, &len, NULL, 0) < 0) {
		warn("sysctl(KERN_SYSVMSG)");
		return;
	}
	if (!valid) {
		warnx("SVID messages facility not configured in the system");
		return;
	}

	mib[0] = CTL_KERN;
	mib[1] = KERN_SYSVIPC_INFO;
	mib[2] = KERN_SYSVIPC_MSG_INFO;

	if (!(display & MSGINFO)) {
		/* totals only */
		len = sizeof(struct msginfo);
	} else {
		if (sysctl(mib, 3, NULL, &len, NULL, 0) < 0) {
			warn("sysctl(KERN_SYSVIPC_MSG_INFO)");
			return;
		}
	}

	if ((buf = malloc(len)) == NULL)
		err(1, "malloc");
	msgsi = (struct msg_sysctl_info *)buf;
	if (sysctl(mib, 3, msgsi, &len, NULL, 0) < 0) {
		warn("sysctl(KERN_SYSVIPC_MSG_INFO)");
		return;
	}

	if (display & MSGTOTAL)
		show_msgtotal(&msgsi->msginfo);

	if (display & MSGINFO) {
		show_msginfo_hdr();
		for (i = 0; i < msgsi->msginfo.msgmni; i++) {
			struct msqid_ds *msqptr = &msgsi->msgids[i];
			if (msqptr->msg_qbytes != 0)
				show_msginfo(msqptr->msg_stime,
				    msqptr->msg_rtime,
				    msqptr->msg_ctime,
				    IXSEQ_TO_IPCID(i, msqptr->msg_perm),
				    msqptr->msg_perm.key,
				    msqptr->msg_perm.mode,
				    msqptr->msg_perm.uid,
				    msqptr->msg_perm.gid,
				    msqptr->msg_perm.cuid,
				    msqptr->msg_perm.cgid,
				    msqptr->msg_cbytes,
				    msqptr->msg_qnum,
				    msqptr->msg_qbytes,
				    msqptr->msg_lspid,
				    msqptr->msg_lrpid);
		}
		printf("\n");
	}
}

void
shm_sysctl(void)
{
	struct shm_sysctl_info *shmsi;
	char *buf;
	int mib[3];
	size_t len;
	int i, valid;

	mib[0] = CTL_KERN;
	mib[1] = KERN_SYSVSHM;
	len = sizeof(valid);
	if (sysctl(mib, 2, &valid, &len, NULL, 0) < 0) {
		warn("sysctl(KERN_SYSVSHM)");
		return;
	}
	if (!valid) {
		warnx("SVID shared memory facility not configured in "
		    "the system");
		return;
	}

	mib[0] = CTL_KERN;
	mib[1] = KERN_SYSVIPC_INFO;
	mib[2] = KERN_SYSVIPC_SHM_INFO;

	if (!(display & SHMINFO)) {
		/* totals only */
		len = sizeof(struct shminfo);
	} else {
		if (sysctl(mib, 3, NULL, &len, NULL, 0) < 0) {
			warn("sysctl(KERN_SYSVIPC_SHM_INFO)");
			return;
		}
	}

	if ((buf = malloc(len)) == NULL)
		err(1, "malloc");
	shmsi = (struct shm_sysctl_info *)buf;
	if (sysctl(mib, 3, shmsi, &len, NULL, 0) < 0) {
		warn("sysctl(KERN_SYSVIPC_SHM_INFO)");
		return;
	}

	if (display & SHMTOTAL)
		show_shmtotal(&shmsi->shminfo);

	if (display & SHMINFO) {
		show_shminfo_hdr();
		for (i = 0; i < shmsi->shminfo.shmmni; i++) {
			struct shmid_ds *shmptr = &shmsi->shmids[i];
			if (shmptr->shm_internal)
				show_shminfo(shmptr->shm_atime,
				    shmptr->shm_dtime,
				    shmptr->shm_ctime,
				    IXSEQ_TO_IPCID(i, shmptr->shm_perm),
				    shmptr->shm_perm.key,
				    shmptr->shm_perm.mode,
				    shmptr->shm_perm.uid,
				    shmptr->shm_perm.gid,
				    shmptr->shm_perm.cuid,
				    shmptr->shm_perm.cgid,
				    shmptr->shm_nattch,
				    shmptr->shm_segsz,
				    shmptr->shm_cpid,
				    shmptr->shm_lpid);
		}
		printf("\n");
	}
}

void
sem_sysctl(void)
{
	struct sem_sysctl_info *semsi;
	char *buf;
	int mib[3];
	size_t len;
	int i, valid;

	mib[0] = CTL_KERN;
	mib[1] = KERN_SYSVSEM;
	len = sizeof(valid);
	if (sysctl(mib, 2, &valid, &len, NULL, 0) < 0) {
		warn("sysctl(KERN_SYSVSEM)");
		return;
	}
	if (!valid) {
		warnx("SVID shared memory facility not configured in "
		    "the system");
		return;
	}

	mib[0] = CTL_KERN;
	mib[1] = KERN_SYSVIPC_INFO;
	mib[2] = KERN_SYSVIPC_SEM_INFO;

	if (!(display & SEMINFO)) {
		/* totals only */
		len = sizeof(struct seminfo);
	} else {
		if (sysctl(mib, 3, NULL, &len, NULL, 0) < 0) {
			warn("sysctl(KERN_SYSVIPC_SEM_INFO)");
			return;
		}
	}

	if ((buf = malloc(len)) == NULL)
		err(1, "malloc");
	semsi = (struct sem_sysctl_info *)buf;
	if (sysctl(mib, 3, semsi, &len, NULL, 0) < 0) {
		warn("sysctl(KERN_SYSVIPC_SEM_INFO)");
		return;
	}

	if (display & SEMTOTAL)
		show_semtotal(&semsi->seminfo);

	if (display & SEMINFO) {
		show_seminfo_hdr();
		for (i = 0; i < semsi->seminfo.semmni; i++) {
			struct semid_ds *semaptr = &semsi->semids[i];

			if (semaptr->sem_base != NULL)
				show_seminfo(semaptr->sem_otime,
				    semaptr->sem_ctime,
				    IXSEQ_TO_IPCID(i, semaptr->sem_perm),
				    semaptr->sem_perm.key,
				    semaptr->sem_perm.mode,
				    semaptr->sem_perm.uid,
				    semaptr->sem_perm.gid,
				    semaptr->sem_perm.cuid,
				    semaptr->sem_perm.cgid,
				    semaptr->sem_nsems);
		}
		printf("\n");
	}
}

void
ipcs_kvm(void)
{
	struct msginfo msginfo;
	struct que msgque;
	struct msqid_ds *msqids;
	struct seminfo seminfo;
	struct semid_ds sem, **sema;
	struct shminfo shminfo;
	struct shmid_ds shmseg, **shmsegs;
	char errbuf[_POSIX2_LINE_MAX];
	u_long addr;
	kvm_t *kd;
	int i;
	struct nlist symbols[] = {
		{"_sema"},
	#define X_SEMA		0
		{"_seminfo"},
	#define X_SEMINFO	1
		{"_semu"},
	#define X_SEMU		2
		{"_msginfo"},
	#define X_MSGINFO	3
		{"_msg_queues"},
	#define X_MSG_QUEUES	4
		{"_shminfo"},
	#define X_SHMINFO	5
		{"_shmsegs"},
	#define X_SHMSEGS	6
		{NULL}
	};

	if ((kd = kvm_openfiles(namelist, core, NULL, O_RDONLY,
	    errbuf)) == NULL)
		errx(1, "can't open kvm: %s", errbuf);

	switch (kvm_nlist(kd, symbols)) {
	case 0:
		break;
	case -1:
		errx(1, "%s: unable to read symbol table.",
		    namelist == NULL ? _PATH_UNIX : namelist);
		/* NOTREACHED */
	default:
#ifdef notdef		/* they'll be told more civilly later */
		warnx("nlist failed");
		for (i = 0; symbols[i].n_name != NULL; i++)
			if (symbols[i].n_value == 0)
				warnx("symbol %s not found", symbols[i].n_name);
#endif
		break;
	}

	if ((display & (MSGINFO | MSGTOTAL)) &&
	    (kvm_read(kd, symbols[X_MSGINFO].n_value,
	     &msginfo, sizeof(msginfo)) == sizeof(msginfo))) {

		if (display & MSGTOTAL)
			show_msgtotal(&msginfo);

		if (display & MSGINFO) {
			if (kvm_read(kd, symbols[X_MSG_QUEUES].n_value,
			    &addr, sizeof(addr)) != sizeof(addr))
				errx(1, "kvm_read (%s): %s",
				    symbols[X_MSG_QUEUES].n_name,
				    kvm_geterr(kd));

			msqids = calloc(sizeof(struct msqid_ds),
			    msginfo.msgmni);
			if (msqids == NULL)
				err(1, "calloc");

			/* walk the TAILQ */
			while (addr != 0) {
				if (kvm_read(kd, addr, &msgque, sizeof(msgque))
				    != sizeof(msgque))
					errx(1, "kvm_read (%s): %s",
					    "msg que", kvm_geterr(kd));
				msqids[msgque.que_ix] = msgque.msqid_ds;
				addr = (u_long)TAILQ_NEXT(&msgque, que_next);
			}

			show_msginfo_hdr();
			for (i = 0; i < msginfo.msgmni; i++) {
				struct msqid_ds *msqptr = &msqids[i];
				if (msqptr->msg_qbytes != 0)
					show_msginfo(msqptr->msg_stime,
					    msqptr->msg_rtime,
					    msqptr->msg_ctime,
					    IXSEQ_TO_IPCID(i, msqptr->msg_perm),
					    msqptr->msg_perm.key,
					    msqptr->msg_perm.mode,
					    msqptr->msg_perm.uid,
					    msqptr->msg_perm.gid,
					    msqptr->msg_perm.cuid,
					    msqptr->msg_perm.cgid,
					    msqptr->msg_cbytes,
					    msqptr->msg_qnum,
					    msqptr->msg_qbytes,
					    msqptr->msg_lspid,
					    msqptr->msg_lrpid);
			}
			printf("\n");
		}
	} else {
		if (display & (MSGINFO | MSGTOTAL)) {
			warnx("SVID messages facility not configured in "
			    "the system");
		}
	}
	if ((display & (SHMINFO | SHMTOTAL)) &&
	    (kvm_read(kd, symbols[X_SHMINFO].n_value, &shminfo,
	     sizeof(shminfo)) == sizeof(shminfo))) {

		if (display & SHMTOTAL)
			show_shmtotal(&shminfo);

		if (display & SHMINFO) {
			if (kvm_read(kd, symbols[X_SHMSEGS].n_value, &addr,
			    sizeof(addr)) != sizeof(addr))
				errx(1, "kvm_read (%s): %s",
				    symbols[X_SHMSEGS].n_name, kvm_geterr(kd));

			shmsegs = calloc(sizeof(struct shmid_ds *),
			    shminfo.shmmni);
			if (shmsegs == NULL)
				err(1, "calloc");

			if (kvm_read(kd, addr, shmsegs,
			    sizeof(struct shmid_ds *) * shminfo.shmmni) !=
			    sizeof(struct shmid_ds *) * shminfo.shmmni)
				errx(1, "kvm_read (shmsegs): %s",
				    kvm_geterr(kd));

			show_shminfo_hdr();
			for (i = 0; i < shminfo.shmmni; i++) {
				if (shmsegs[i] == NULL)
					continue;

				if (kvm_read(kd, (u_long)shmsegs[i], &shmseg,
				    sizeof(shmseg)) != sizeof(shmseg))
					errx(1, "kvm_read (shmseg): %s",
					    kvm_geterr(kd));
				show_shminfo(shmseg.shm_atime,
				    shmseg.shm_dtime,
				    shmseg.shm_ctime,
				    IXSEQ_TO_IPCID(i, shmseg.shm_perm),
				    shmseg.shm_perm.key,
				    shmseg.shm_perm.mode,
				    shmseg.shm_perm.uid,
				    shmseg.shm_perm.gid,
				    shmseg.shm_perm.cuid,
				    shmseg.shm_perm.cgid,
				    shmseg.shm_nattch,
				    shmseg.shm_segsz,
				    shmseg.shm_cpid,
				    shmseg.shm_lpid);
			}
			printf("\n");
		}
	} else
		if (display & (SHMINFO | SHMTOTAL)) {
			warnx("SVID shared memory facility not configured in "
			    "the system");
		}
	if ((display & (SEMINFO | SEMTOTAL)) &&
	    (kvm_read(kd, symbols[X_SEMINFO].n_value, &seminfo,
	     sizeof(seminfo)) == sizeof(seminfo))) {
		if (display & SEMTOTAL)
			show_semtotal(&seminfo);

		if (display & SEMINFO) {
			if (kvm_read(kd, symbols[X_SEMA].n_value, &addr,
			    sizeof(addr)) != sizeof(addr))
				errx(1, "kvm_read (%s): %s",
				    symbols[X_SEMA].n_name, kvm_geterr(kd));

			sema = calloc(sizeof(struct semid_ds *),
			    seminfo.semmni);
			if (sema == NULL)
				err(1, "calloc");

			if (kvm_read(kd, addr, sema,
			    sizeof(struct semid_ds *) * seminfo.semmni) !=
			    sizeof(struct semid_ds *) * seminfo.semmni)
				errx(1, "kvm_read (sema): %s",
				    kvm_geterr(kd));

			show_seminfo_hdr();
			for (i = 0; i < seminfo.semmni; i++) {
				if (sema[i] == NULL)
					continue;

				if (kvm_read(kd, (u_long)sema[i], &sem,
				    sizeof(sem)) != sizeof(sem))
					errx(1, "kvm_read (sem): %s",
					    kvm_geterr(kd));
				show_seminfo(sem.sem_otime,
				    sem.sem_ctime,
				    IXSEQ_TO_IPCID(i, sem.sem_perm),
				    sem.sem_perm.key,
				    sem.sem_perm.mode,
				    sem.sem_perm.uid,
				    sem.sem_perm.gid,
				    sem.sem_perm.cuid,
				    sem.sem_perm.cgid,
				    sem.sem_nsems);
			}
			printf("\n");
		}
	} else
		if (display & (SEMINFO | SEMTOTAL)) {
			warnx("SVID semaphores facility not configured in "
			    "the system");
		}
	kvm_close(kd);
}

void
usage(void)
{
	extern char *__progname;

	fprintf(stderr,
	    "usage: %s [-abcMmopQqSsTt] [-C core] [-N system]\n",
	    __progname);
	exit(1);
}
