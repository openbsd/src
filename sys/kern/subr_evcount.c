/*	$OpenBSD: subr_evcount.c,v 1.4 2004/09/29 07:37:04 miod Exp $ */
/*
 * Copyright (c) 2004 Artur Grabowski <art@openbsd.org>
 * Copyright (c) 2004 Aaron Campbell <aaron@openbsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL  DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/evcount.h>
#include <sys/timeout.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/sysctl.h>

#ifdef __HAVE_EVCOUNT

static TAILQ_HEAD(,evcount) evcount_list;
static struct evcount *evcount_next_sync;

/*
 * Standard evcount parents.
 */
struct evcount evcount_intr;

void evcount_timeout(void *);
void evcount_init(void);

void
evcount_init(void)
{
	TAILQ_INIT(&evcount_list);

	evcount_attach(&evcount_intr, "intr", NULL, NULL);
}


void
evcount_attach(ec, name, data, parent)
	struct evcount *ec;
	const char *name;
	void *data;
	struct evcount *parent;
{
	static int nextid = 0;

	if (nextid == 0) {
		nextid++;		/* start with 1 */
		evcount_init();
	}

	memset(ec, 0, sizeof(*ec));
	ec->ec_name = name;
	ec->ec_parent = parent;
	ec->ec_id = nextid++;
	ec->ec_data = data;
	TAILQ_INSERT_TAIL(&evcount_list, ec, next);
}

void
evcount_detach(ec)
	struct evcount *ec;
{
	if (evcount_next_sync == ec)
		evcount_next_sync = TAILQ_NEXT(ec, next);

	TAILQ_REMOVE(&evcount_list, ec, next);
}

int
evcount_sysctl(name, namelen, oldp, oldlenp, newp, newlen)
	int *name;
	u_int namelen;
	void *oldp;
	size_t *oldlenp;
	void *newp;
	size_t newlen;
{
	int error = 0, s, nintr, i;
	struct evcount *ec;
	u_int64_t count;

	if (newp != NULL)
		return (EPERM);

	if (name[0] != KERN_INTRCNT_NUM) {
		if (namelen != 2)
			return (ENOTDIR);
		if (name[1] < 0)
			return (EINVAL);
		i = name[1];
	} else
		i = -1;

	nintr = 0;
	TAILQ_FOREACH(ec, &evcount_list, next) {
		if (ec->ec_parent != &evcount_intr)
			continue;
		if (nintr++ == i)
			break;
	}

	switch (name[0]) {
	case KERN_INTRCNT_NUM:
		error = sysctl_rdint(oldp, oldlenp, NULL, nintr);
		break;
	case KERN_INTRCNT_CNT:
		if (ec == NULL)
			return (ENOENT);
		s = splhigh();
		count = ec->ec_count;
		splx(s);
		error = sysctl_rdquad(oldp, oldlenp, NULL, count);
		break;
	case KERN_INTRCNT_NAME:
		if (ec == NULL)
			return (ENOENT);
		error = sysctl_rdstring(oldp, oldlenp, NULL, ec->ec_name);
		break;
	case KERN_INTRCNT_VECTOR:
		if (ec == NULL || ec->ec_data == NULL)
			return (ENOENT);
		error = sysctl_rdint(oldp, oldlenp, NULL,
		    *((int *)ec->ec_data));
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}

	return (error);
}

#endif /* __HAVE_EVCOUNT */
