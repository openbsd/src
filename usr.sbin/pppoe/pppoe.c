/*	$OpenBSD: pppoe.c,v 1.15 2004/09/20 17:51:07 miod Exp $	*/

/*
 * Copyright (c) 2000 Network Security Technologies, Inc. http://www.netsec.net
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <net/bpf.h>
#include <errno.h>
#include <string.h>
#include <err.h>
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <unistd.h>
#include <sysexits.h>
#include <stdlib.h>
#include <signal.h>
#include <ifaddrs.h>

#include "pppoe.h"

int option_verbose = 0;
u_char etherbroadcastaddr[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

int main(int, char **);
void usage(void);
int getifhwaddr(char *, char *, struct ether_addr *);
int setupfilter(char *, struct ether_addr *, int);
int setup_rfilter(struct bpf_insn *, struct ether_addr *, int);
int setup_wfilter(struct bpf_insn *, int);
void child_handler(int);
int signal_init(void);
void drop_privs(struct passwd *, int);

int
main(int argc, char **argv)
{
	char *ifname = NULL;
	u_int8_t *sysname = NULL, *srvname = NULL;
	char ifnambuf[IFNAMSIZ];
	struct ether_addr ea;
	int bpffd, smode = 0, c;
	struct passwd *pw;

	if ((pw = getpwnam("_ppp")) == NULL)
		err(EX_CONFIG, "getpwnam(\"_ppp\")");

	while ((c = getopt(argc, argv, "svi:n:p:")) != -1) {
		switch (c) {
		case 'i':
			if (ifname != NULL) {
				usage();
				return (EX_USAGE);
			}
			ifname = optarg;
			break;
		case 'n':
			if (srvname != NULL) {
				usage();
				return (EX_USAGE);
			}
			srvname = (u_int8_t *)optarg;
			break;
		case 'p':
			if (sysname != NULL) {
				usage();
				return (EX_USAGE);
			}
			sysname = (u_int8_t *)optarg;
			break;
		case 's':
			if (smode) {
				usage();
				return (EX_USAGE);
			}
			smode = 1;
			break;
		case 'v':
			option_verbose++;
			break;
		default:
			usage();
			return (EX_USAGE);
		}
	}

	argc -= optind;
	if (argc != 0) {
		usage();
		return (EX_USAGE);
	}

	if (getifhwaddr(ifname, ifnambuf, &ea) < 0)
		return (EX_IOERR);

	bpffd = setupfilter(ifnambuf, &ea, smode);
	if (bpffd < 0)
		return (EX_IOERR);

	drop_privs(pw, smode);

	signal_init();

	if (smode)
		server_mode(bpffd, sysname, srvname, &ea);
	else
		client_mode(bpffd, sysname, srvname, &ea);

	return (0);
}

#define MAX_INSNS	20

/* bpf read filter */
int
setup_rfilter(struct bpf_insn *insns, struct ether_addr *ea, int server_mode)
{
	u_int8_t *ep = (u_int8_t *)ea;
	int idx = 0;

	/* allow session or discovery packets */
	insns[idx].code = BPF_LD | BPF_H | BPF_ABS;
	insns[idx].k = 12;
	insns[idx].jt = insns[idx].jf = 0;
	idx++;

	insns[idx].code = BPF_JMP | BPF_JEQ | BPF_K;
	insns[idx].k = ETHERTYPE_PPPOE;
	insns[idx].jt = 1;
	insns[idx].jf = 0;
	idx++;

	insns[idx].code = BPF_JMP | BPF_JEQ | BPF_K;
	insns[idx].k = ETHERTYPE_PPPOEDISC;
	insns[idx].jt = 0;
	insns[idx].jf = 4;
	idx++;

	/* reject packets containing our address as source */
	insns[idx].code = BPF_LD | BPF_W | BPF_ABS;
	insns[idx].k = 6;
	insns[idx].jt = insns[idx].jf = 0;
	idx++;

	insns[idx].code = BPF_JMP | BPF_JEQ | BPF_K;
	insns[idx].k =
	    (ep[0] << 24) | (ep[1] << 16) | (ep[2] << 8) | (ep[3] << 0);
	insns[idx].jt = 0;
	insns[idx].jf = 3;
	idx++;

	insns[idx].code = BPF_LD | BPF_H | BPF_ABS;
	insns[idx].k = 10;
	insns[idx].jt = insns[idx].jf = 0;
	idx++;

	insns[idx].code = BPF_JMP | BPF_JEQ | BPF_K;
	insns[idx].k = (ep[4] << 8) | (ep[5] << 0);
	insns[idx].jt = 0;
	insns[idx].jf = 1;
	idx++;

	insns[idx].code = BPF_RET | BPF_K;
	insns[idx].k = insns[idx].jt = insns[idx].jf = 0;
	idx++;

	if (server_mode) {
		/* if server mode, allow broadcast as destination */
		insns[idx].code = BPF_LD | BPF_W | BPF_ABS;
		insns[idx].k = insns[idx].jt = insns[idx].jf = 0;
		idx++;

		insns[idx].code = BPF_JMP | BPF_JEQ | BPF_K;
		insns[idx].k = 0xffffffff;
		insns[idx].jt = 0;
		insns[idx].jf = 3;
		idx++;

		insns[idx].code = BPF_LD | BPF_H | BPF_ABS;
		insns[idx].k = 4;
		insns[idx].jt = insns[idx].jf = 0;
		idx++;

		insns[idx].code = BPF_JMP | BPF_JEQ | BPF_K;
		insns[idx].k = 0xffff;
		insns[idx].jt = 4;
		insns[idx].jf = 0;
		idx++;
	}

	/* make sure packet is destined to our address */
	insns[idx].code = BPF_LD | BPF_W | BPF_ABS;
	insns[idx].k = insns[idx].jt = insns[idx].jf = 0;
	idx++;

	insns[idx].code = BPF_JMP | BPF_JEQ | BPF_K;
	insns[idx].k =
	    (ep[0] << 24) | (ep[1] << 16) | (ep[2] << 8) | (ep[3] << 0);
	insns[idx].jt = 0;
	insns[idx].jf = 3;
	idx++;

	insns[idx].code = BPF_LD | BPF_H | BPF_ABS;
	insns[idx].k = 4;
	insns[idx].jt = insns[idx].jf = 0;
	idx++;

	insns[idx].code = BPF_JMP | BPF_JEQ | BPF_K;
	insns[idx].k = (ep[4] << 8) | (ep[5] << 0);
	insns[idx].jt = 0;
	insns[idx].jf = 1;
	idx++;

	insns[idx].code = BPF_RET | BPF_K;
	insns[idx].k = (u_int)-1;
	insns[idx].jt = insns[idx].jf = 0;
	idx++;

	insns[idx].code = BPF_RET | BPF_K;
	insns[idx].k = insns[idx].jt = insns[idx].jf = 0;
	idx++;

	return idx;
}

/* bpf write filter */
int
setup_wfilter(struct bpf_insn *insns, int server_mode)
{
	int idx = 0;

	/* check if dest is broadcast */
	insns[idx].code = BPF_LD | BPF_W | BPF_ABS;
	insns[idx].k = insns[idx].jt = insns[idx].jf = 0;
	idx++;

	insns[idx].code = BPF_JMP | BPF_JEQ | BPF_K;
	insns[idx].k = 0xffffffff;
	insns[idx].jt = 0;
	insns[idx].jf = 2;
	idx++;

	insns[idx].code = BPF_LD | BPF_H | BPF_ABS;
	insns[idx].k = 4;
	insns[idx].jt = insns[idx].jf = 0;
	idx++;

	insns[idx].code = BPF_JMP | BPF_JEQ | BPF_K;
	insns[idx].k = 0xffff;
	insns[idx].jt = 4;
	insns[idx].jf = 0;
	idx++;

	/* dest not broadcast, check type for session or discovery */
	insns[idx].code = BPF_LD | BPF_H | BPF_ABS;
	insns[idx].k = 12;
	insns[idx].jt = insns[idx].jf = 0;
	idx++;

	insns[idx].code = BPF_JMP | BPF_JEQ | BPF_K;
	insns[idx].k = ETHERTYPE_PPPOE;
	insns[idx].jt = 1;
	insns[idx].jf = 0;
	idx++;

	insns[idx].code = BPF_JMP | BPF_JEQ | BPF_K;
	insns[idx].k = ETHERTYPE_PPPOEDISC;
	insns[idx].jt = 0;
	insns[idx].jf = (server_mode) ? 1 : 4;
	idx++;

	insns[idx].code = BPF_RET | BPF_K;
	insns[idx].k = (u_int)-1;
	insns[idx].jt = insns[idx].jf = 0;
	idx++;

	/* packet is broadcast */
	if (! server_mode) {
		/* only allowed for discovery in client mode */
		insns[idx].code = BPF_LD | BPF_H | BPF_ABS;
		insns[idx].k = 12;
		insns[idx].jt = insns[idx].jf = 0;
		idx++;

		insns[idx].code = BPF_JMP | BPF_JEQ | BPF_K;
		insns[idx].k = ETHERTYPE_PPPOEDISC;
		insns[idx].jt = 0;
		insns[idx].jf = 1;
		idx++;

		insns[idx].code = BPF_RET | BPF_K;
		insns[idx].k = (u_int)-1;
		insns[idx].jt = insns[idx].jf = 0;
		idx++;
	}

	insns[idx].code = BPF_RET | BPF_K;
	insns[idx].k = insns[idx].jt = insns[idx].jf = 0;
	idx++;

	return idx;
}

int
setupfilter(char *ifn, struct ether_addr *ea, int server_mode)
{
	char device[sizeof "/dev/bpf0000000000"];
	int fd, idx = 0;
	u_int u, i;
	struct ifreq ifr;
	struct bpf_insn insns[MAX_INSNS];
	struct bpf_program filter;

	for (i = 0; ; i++) {
		snprintf(device, sizeof(device), "/dev/bpf%d", i);
		fd = open(device, O_RDWR);
		if (fd < 0) {
			if (errno != EBUSY)
				err(EX_IOERR, "%s", device);
		}
		else
			break;
	}

	u = PPPOE_BPF_BUFSIZ;
	if (ioctl(fd, BIOCSBLEN, &u) < 0) {
		close(fd);
		err(EX_IOERR, "set snaplength");
	}

	u = 1;
	if (ioctl(fd, BIOCIMMEDIATE, &u) < 0) {
		close(fd);
		err(EX_IOERR, "set immediate");
	}

	strlcpy(ifr.ifr_name, ifn, IFNAMSIZ);
	if (ioctl(fd, BIOCSETIF, &ifr) < 0) {
		close(fd);
		err(EX_IOERR, "set interface");
	}

	if (ioctl(fd, BIOCGDLT, &u) < 0) {
		close(fd);
		err(EX_IOERR, "get interface type");
	}
	if (u != DLT_EN10MB)
		err(EX_IOERR, "%s is not ethernet", ifn);


	idx = setup_rfilter(insns, ea, server_mode);

	filter.bf_len = idx;
	filter.bf_insns = insns;

	if (ioctl(fd, BIOCSETF, &filter) < 0) {
		close(fd);
		err(EX_IOERR, "BIOCSETF");
	}

	idx = setup_wfilter(insns, server_mode);

	filter.bf_len = idx;
	filter.bf_insns = insns;

	if (ioctl(fd, BIOCSETWF, &filter) < 0) {
		close(fd);
		err(EX_IOERR, "BIOCSETWF");
	}

	/* lock the descriptor against changes */
	if (ioctl(fd, BIOCLOCK) < 0) {
		close(fd);
		err(EX_IOERR, "BIOCLOCK");
	}

	return (fd);
}

int
getifhwaddr(char *ifnhint, char *ifnambuf, struct ether_addr *ea)
{
	struct sockaddr_dl *dl;
	struct ifaddrs *ifap, *ifa;

	if (getifaddrs(&ifap) != 0) {
		perror("getifaddrs");
		return (-1);
	}

	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr->sa_family != AF_LINK)
			continue;
		if (ifnhint != NULL && strcmp(ifnhint, ifa->ifa_name))
			continue;
		if (ifnhint == NULL) {
			if ((ifa->ifa_flags & IFF_UP) == 0)
				continue;
		}
		dl = (struct sockaddr_dl *)ifa->ifa_addr;
		if (dl->sdl_type != IFT_ETHER) {
			if (ifnhint == NULL)
				continue;
			fprintf(stderr, "not ethernet interface: %s\n",
				ifnhint);
			freeifaddrs(ifap);
			return (-1);
		}
		if (dl->sdl_alen != ETHER_ADDR_LEN) {
			fprintf(stderr, "invalid hwaddr len: %u\n",
				dl->sdl_alen);
			freeifaddrs(ifap);
			return (-1);
		}
		bcopy(dl->sdl_data + dl->sdl_nlen, ea, sizeof(*ea));
		strlcpy(ifnambuf, ifa->ifa_name, IFNAMSIZ);
		freeifaddrs(ifap);
		return (0);
	}
	freeifaddrs(ifap);
	if (ifnhint == NULL)
		fprintf(stderr, "no running ethernet found\n");
	else
		fprintf(stderr, "no such interface: %s\n", ifnhint);
	return (-1);
}

void
usage(void)
{
	extern char *__progname;

	fprintf(stderr,"%s [-sv] [-i interface] [-n service] [-p system]\n",
	    __progname);
}

void
child_handler(int sig)
{
	int save_errno = errno;
	int status;

	while (wait3(&status, WNOHANG, NULL) > 0)
		;
	errno = save_errno;
}

int
signal_init(void)
{
	struct sigaction act;

	if (sigemptyset(&act.sa_mask) < 0)
		return (-1);
	act.sa_flags = SA_RESTART;
	act.sa_handler = child_handler;
	if (sigaction(SIGCHLD, &act, NULL) < 0)
		return (-1);

	if (sigemptyset(&act.sa_mask) < 0)
		return (-1);
	act.sa_flags = 0;
	act.sa_handler = SIG_IGN;
	if (sigaction(SIGPIPE, &act, NULL) < 0)
		return (-1);

	return (0);
}

void
drop_privs(struct passwd *pw, int server_mode)
{
	int groups[2], ng = 1;
	struct group *gr;

	groups[0] = pw->pw_gid;

	if (server_mode) {
		if ((gr = getgrnam("network")) == NULL)
			err(EX_CONFIG, "getgrnam(\"network\")");
		groups[ng++] = gr->gr_gid;
	} else {
		if (chroot(pw->pw_dir) == -1)
			err(EX_OSERR, "chroot: %s", pw->pw_dir);
		if (chdir("/") == -1)
			err(EX_OSERR, "chdir");
	}

	if (setgroups(ng, groups))
		err(EX_OSERR, "setgroups");

	if (setegid(pw->pw_gid))
		err(EX_OSERR, "setegid");

	if (setgid(pw->pw_gid))
		err(EX_OSERR, "setgid");

	if (seteuid(pw->pw_uid))
		err(EX_OSERR, "seteuid");

	if (setuid(pw->pw_uid))
		err(EX_OSERR, "setuid");

	endpwent();
}
