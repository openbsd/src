/*	$OpenBSD: showmount.c,v 1.17 2009/10/27 23:59:43 deraadt Exp $	*/
/*	$NetBSD: showmount.c,v 1.7 1996/05/01 18:14:10 cgd Exp $	*/

/*
 * Copyright (c) 1989, 1993, 1995
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
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
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/socketvar.h>

#include <netdb.h>
#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include <rpc/pmap_prot.h>
#include <nfs/rpcv2.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vis.h>

/* Constant defs */
#define	ALL	1
#define	DIRS	2

#define	DODUMP		0x1
#define	DOEXPORTS	0x2

struct mountlist {
	struct mountlist *ml_left;
	struct mountlist *ml_right;
	char	ml_host[RPCMNT_NAMELEN+1];
	char	ml_dirp[RPCMNT_PATHLEN+1];
};

struct grouplist {
	struct grouplist *gr_next;
	char	gr_name[RPCMNT_NAMELEN+1];
};

struct exportslist {
	struct exportslist *ex_next;
	struct grouplist *ex_groups;
	char	ex_dirp[RPCMNT_PATHLEN+1];
};

static struct mountlist *mntdump;
static struct exportslist *exports;
static int type = 0;

void	print_dump(struct mountlist *);
void	usage(void);
int	xdr_mntdump(XDR *, struct mountlist **);
int	xdr_exports(XDR *, struct exportslist **);

/*
 * This command queries the NFS mount daemon for it's mount list and/or
 * it's exports list and prints them out.
 * See "NFS: Network File System Protocol Specification, RFC1094, Appendix A"
 * and the "Network File System Protocol XXX.."
 * for detailed information on the protocol.
 */
int
main(int argc, char *argv[])
{
	struct exportslist *exp;
	struct grouplist *grp;
	struct sockaddr_in clnt_sin;
	struct hostent *hp;
	struct timeval timeout;
	int rpcs = 0, mntvers = 1;
	enum clnt_stat estat;
	CLIENT *client;
	char *host;
	int ch, clnt_sock;

	while ((ch = getopt(argc, argv, "ade3")) != -1)
		switch (ch) {
		case 'a':
			if (type == 0) {
				type = ALL;
				rpcs |= DODUMP;
			} else
				usage();
			break;
		case 'd':
			if (type == 0) {
				type = DIRS;
				rpcs |= DODUMP;
			} else
				usage();
			break;
		case 'e':
			rpcs |= DOEXPORTS;
			break;
		case '3':
			mntvers = 3;
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc > 0)
		host = *argv;
	else
		host = "localhost";

	if (rpcs == 0)
		rpcs = DODUMP;

	if ((hp = gethostbyname(host)) == NULL) {
		fprintf(stderr, "showmount: unknown host %s\n", host);
		exit(1);
	}
	bzero(&clnt_sin, sizeof clnt_sin);
	clnt_sin.sin_len = sizeof clnt_sin;
	clnt_sin.sin_family = AF_INET;
	bcopy(hp->h_addr, (char *)&clnt_sin.sin_addr, hp->h_length);
	clnt_sock = RPC_ANYSOCK;
	client = clnttcp_create(&clnt_sin, RPCPROG_MNT, mntvers,
	    &clnt_sock, 0, 0);
	if (client == NULL) {
		clnt_pcreateerror("showmount: clnttcp_create");
		exit(1);
	}
	timeout.tv_sec = 30;
	timeout.tv_usec = 0;

	if (rpcs & DODUMP) {
		estat = clnt_call(client, RPCMNT_DUMP, xdr_void, (char *)0,
		    xdr_mntdump, (char *)&mntdump, timeout);
		if (estat != RPC_SUCCESS) {
			fprintf(stderr, "showmount: Can't do Mountdump rpc: ");
			clnt_perrno(estat);
			exit(1);
		}
	}
	if (rpcs & DOEXPORTS) {
		estat = clnt_call(client, RPCMNT_EXPORT, xdr_void, (char *)0,
		    xdr_exports, (char *)&exports, timeout);
		if (estat != RPC_SUCCESS) {
			fprintf(stderr, "showmount: Can't do Exports rpc: ");
			clnt_perrno(estat);
			exit(1);
		}
	}

	/* Now just print out the results */
	if (rpcs & DODUMP) {
		switch (type) {
		case ALL:
			printf("All mount points on %s:\n", host);
			break;
		case DIRS:
			printf("Directories on %s:\n", host);
			break;
		default:
			printf("Hosts on %s:\n", host);
			break;
		}
		print_dump(mntdump);
	}
	if (rpcs & DOEXPORTS) {
		char	vp[(RPCMNT_PATHLEN+1)*4];
		char	vn[(RPCMNT_NAMELEN+1)*4];

		printf("Exports list on %s:\n", host);
		exp = exports;
		while (exp) {
			strnvis(vp, exp->ex_dirp, sizeof vp, VIS_CSTYLE);
			printf("%-34s ", vp);
			grp = exp->ex_groups;
			if (grp == NULL) {
				printf("Everyone\n");
			} else {
				while (grp) {
					strnvis(vn, grp->gr_name, sizeof vn,
					    VIS_CSTYLE);
					printf("%s ", vn);
					grp = grp->gr_next;
				}
				printf("\n");
			}
			exp = exp->ex_next;
		}
	}

	exit(0);
}

/*
 * Xdr routine for retrieving the mount dump list
 */
int
xdr_mntdump(XDR *xdrsp, struct mountlist **mlp)
{
	struct mountlist *mp, **otp = NULL, *tp;
	int bool, val, val2;
	char *strp;

	*mlp = (struct mountlist *)0;
	if (!xdr_bool(xdrsp, &bool))
		return (0);
	while (bool) {
		mp = (struct mountlist *)malloc(sizeof(struct mountlist));
		if (mp == NULL)
			return (0);
		mp->ml_left = mp->ml_right = (struct mountlist *)0;
		strp = mp->ml_host;
		if (!xdr_string(xdrsp, &strp, RPCMNT_NAMELEN))
			return (0);
		strp = mp->ml_dirp;
		if (!xdr_string(xdrsp, &strp, RPCMNT_PATHLEN))
			return (0);

		/*
		 * Build a binary tree on sorted order of either host or dirp.
		 * Drop any duplications.
		 */
		if (*mlp == NULL) {
			*mlp = mp;
		} else {
			tp = *mlp;
			while (tp) {
				val = strcmp(mp->ml_host, tp->ml_host);
				val2 = strcmp(mp->ml_dirp, tp->ml_dirp);
				switch (type) {
				case ALL:
					if (val == 0) {
						if (val2 == 0) {
							free((caddr_t)mp);
							goto next;
						}
						val = val2;
					}
					break;
				case DIRS:
					if (val2 == 0) {
						free((caddr_t)mp);
						goto next;
					}
					val = val2;
					break;
				default:
					if (val == 0) {
						free((caddr_t)mp);
						goto next;
					}
					break;
				}
				if (val < 0) {
					otp = &tp->ml_left;
					tp = tp->ml_left;
				} else {
					otp = &tp->ml_right;
					tp = tp->ml_right;
				}
			}
			*otp = mp;
		}
next:
		if (!xdr_bool(xdrsp, &bool))
			return (0);
	}
	return (1);
}

/*
 * Xdr routine to retrieve exports list
 */
int
xdr_exports(XDR *xdrsp, struct exportslist **exp)
{
	struct exportslist *ep;
	struct grouplist *gp;
	int bool, grpbool;
	char *strp;

	*exp = (struct exportslist *)0;
	if (!xdr_bool(xdrsp, &bool))
		return (0);
	while (bool) {
		ep = (struct exportslist *)malloc(sizeof(struct exportslist));
		if (ep == NULL)
			return (0);
		ep->ex_groups = (struct grouplist *)0;
		strp = ep->ex_dirp;
		if (!xdr_string(xdrsp, &strp, RPCMNT_PATHLEN))
			return (0);
		if (!xdr_bool(xdrsp, &grpbool))
			return (0);
		while (grpbool) {
			gp = (struct grouplist *)malloc(
			    sizeof(struct grouplist));
			if (gp == NULL)
				return (0);
			strp = gp->gr_name;
			if (!xdr_string(xdrsp, &strp, RPCMNT_NAMELEN))
				return (0);
			gp->gr_next = ep->ex_groups;
			ep->ex_groups = gp;
			if (!xdr_bool(xdrsp, &grpbool))
				return (0);
		}
		ep->ex_next = *exp;
		*exp = ep;
		if (!xdr_bool(xdrsp, &bool))
			return (0);
	}
	return (1);
}

void
usage(void)
{

	fprintf(stderr, "usage: showmount [-3ade] [host]\n");
	exit(1);
}

/*
 * Print the binary tree in inorder so that output is sorted.
 */
void
print_dump(struct mountlist *mp)
{
	char	vn[(RPCMNT_NAMELEN+1)*4];
	char	vp[(RPCMNT_PATHLEN+1)*4];

	if (mp == NULL)
		return;
	if (mp->ml_left)
		print_dump(mp->ml_left);
	switch (type) {
	case ALL:
		strvis(vn, mp->ml_host, VIS_CSTYLE);
		strvis(vp, mp->ml_dirp, VIS_CSTYLE);
		printf("%s:%s\n", vn, vp);
		break;
	case DIRS:
		strvis(vp, mp->ml_dirp, VIS_CSTYLE);
		printf("%s\n", vp);
		break;
	default:
		strvis(vn, mp->ml_host, VIS_CSTYLE);
		printf("%s\n", vn);
		break;
	}
	if (mp->ml_right)
		print_dump(mp->ml_right);
}
