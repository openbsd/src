/*	$OpenBSD: mbuf.c,v 1.21 2004/04/16 23:06:56 tedu Exp $	*/
/*	$NetBSD: mbuf.c,v 1.9 1996/05/07 02:55:03 thorpej Exp $	*/

/*
 * Copyright (c) 1983, 1988, 1993
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

#ifndef lint
#if 0
static char sccsid[] = "from: @(#)mbuf.c	8.1 (Berkeley) 6/6/93";
#else
static char *rcsid = "$OpenBSD: mbuf.c,v 1.21 2004/04/16 23:06:56 tedu Exp $";
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/mbuf.h>
#include <sys/pool.h>
#include <sys/sysctl.h>
#include <sys/errno.h>

#include <kvm.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "netstat.h"

#define	YES	1
typedef int bool;

struct	mbstat mbstat;
struct pool mbpool, mclpool;

extern kvm_t *kvmd;

static struct mbtypes {
	int	mt_type;
	char	*mt_name;
} mbtypes[] = {
	{ MT_DATA,	"data" },
	{ MT_OOBDATA,	"oob data" },
	{ MT_CONTROL,	"ancillary data" },
	{ MT_HEADER,	"packet headers" },
	{ MT_FTABLE,	"fragment reassembly queue headers" },	/* XXX */
	{ MT_SONAME,	"socket names and addresses" },
	{ MT_SOOPTS,	"socket options" },
	{ 0, 0 }
};

int nmbtypes = sizeof(mbstat.m_mtypes) / sizeof(short);
bool seen[256];			/* "have we seen this type yet?" */

/*
 * Print mbuf statistics.
 */
void
mbpr(u_long mbaddr, u_long mbpooladdr, u_long mclpooladdr)
{
	int totmem, totused, totmbufs, totpct;
	int i, mib[4], npools, flag = 0;
	struct pool pool;
	struct mbtypes *mp;
	size_t size;
	int page_size = getpagesize();

	if (nmbtypes != 256) {
		fprintf(stderr,
		    "%s: unexpected change to mbstat; check source\n",
		    __progname);
		return;
	}

	if (kvmd == NULL) {
		if (mbaddr == 0) {
			fprintf(stderr, "%s: mbstat: symbol not in namelist\n",
			    __progname);
			return;
		}

		if (kread(mbaddr, (char *)&mbstat, sizeof (mbstat)))
			return;
		if (kread(mbpooladdr, (char *)&mbpool, sizeof (mbpool)))
			return;

		if (kread(mclpooladdr, (char *)&mclpool, sizeof (mclpool)))
			return;
	} else {
		mib[0] = CTL_KERN;
		mib[1] = KERN_MBSTAT;
		size = sizeof(mbstat);

		if (sysctl(mib, 2, &mbstat, &size, NULL, 0) < 0) {
			printf("Can't retrieve mbuf statistics from the kernel: %s\n",
			    strerror(errno));
			return;
		}

		mib[0] = CTL_KERN;
		mib[1] = KERN_POOL;
		mib[2] = KERN_POOL_NPOOLS;
		size = sizeof(npools);

		if (sysctl(mib, 3, &npools, &size, NULL, 0) < 0) {
			printf("Can't figure out number of pools in kernel: %s\n",
			    strerror(errno));
			return;
		}

		for (i = 1; npools; i++) {
			char name[32];

			mib[0] = CTL_KERN;
			mib[1] = KERN_POOL;
			mib[2] = KERN_POOL_POOL;
			mib[3] = i;
			size = sizeof(struct pool);
			if (sysctl(mib, 4, &pool, &size, NULL, 0) < 0) {
				if (errno == ENOENT)
					continue;
				printf("error getting pool: %s\n",
				    strerror(errno));
				return;
			}
			npools--;
			mib[2] = KERN_POOL_NAME;
			size = sizeof(name);
			if (sysctl(mib, 4, &name, &size, NULL, 0) < 0) {
				printf("error getting pool name: %s\n",
				    strerror(errno));
				return;
			}

			if (!strncmp(name, "mbpl", strlen("mbpl"))) {
				bcopy(&pool, &mbpool, sizeof(struct pool));
				flag++;
			} else {
				if (!strncmp(name, "mclpl", strlen("mclpl"))) {
					bcopy(&pool, &mclpool,
					    sizeof(struct pool));
					flag++;
				}
			}

			if (flag == 2)
				break;
		}
	}

	totmbufs = 0;
	for (mp = mbtypes; mp->mt_name; mp++)
		totmbufs += mbstat.m_mtypes[mp->mt_type];
	printf("%u mbuf%s in use:\n", totmbufs, plural(totmbufs));
	for (mp = mbtypes; mp->mt_name; mp++)
		if (mbstat.m_mtypes[mp->mt_type]) {
			seen[mp->mt_type] = YES;
			printf("\t%u mbuf%s allocated to %s\n",
			    mbstat.m_mtypes[mp->mt_type],
			    plural((int)mbstat.m_mtypes[mp->mt_type]),
			    mp->mt_name);
		}
	seen[MT_FREE] = YES;
	for (i = 0; i < nmbtypes; i++)
		if (!seen[i] && mbstat.m_mtypes[i]) {
			printf("\t%u mbuf%s allocated to <mbuf type %d>\n",
			    mbstat.m_mtypes[i],
			    plural((int)mbstat.m_mtypes[i]), i);
		}
	printf("%lu/%lu/%lu mbuf clusters in use (current/peak/max)\n",
	    (u_long)(mclpool.pr_nout),
	    (u_long)(mclpool.pr_hiwat * mclpool.pr_itemsperpage),
	    (u_long)(mclpool.pr_maxpages * mclpool.pr_itemsperpage));
	totmem = (mbpool.pr_npages * page_size) +
	    (mclpool.pr_npages * page_size);
	totused = mbpool.pr_nout * mbpool.pr_size +
	    mclpool.pr_nout * mclpool.pr_size;
	totpct = (totmem == 0)? 0 : ((totused * 100)/totmem);
	printf("%u Kbytes allocated to network (%d%% in use)\n",
	    totmem / 1024, totpct);
	printf("%lu requests for memory denied\n", mbstat.m_drops);
	printf("%lu requests for memory delayed\n", mbstat.m_wait);
	printf("%lu calls to protocol drain routines\n", mbstat.m_drain);
}
