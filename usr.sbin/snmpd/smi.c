/*	$OpenBSD: smi.c,v 1.6 2009/12/16 22:17:53 deraadt Exp $	*/

/*
 * Copyright (c) 2007, 2008 Reyk Floeter <reyk@vantronix.net>
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

#include <sys/queue.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/tree.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_arp.h>
#include <net/if_media.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <arpa/inet.h>

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>

#include "snmpd.h"
#include "mib.h"

extern struct snmpd *env;

RB_HEAD(oidtree, oid);
RB_PROTOTYPE(oidtree, oid, o_element, smi_oid_cmp);
struct oidtree smi_oidtree;

u_long
smi_getticks(void)
{
	struct timeval	 now, run;
	u_long		 ticks;

	gettimeofday(&now, NULL);
	if (timercmp(&now, &env->sc_starttime, <=))
		return (0);
	timersub(&now, &env->sc_starttime, &run);
	ticks = run.tv_sec * 100;
	if (run.tv_usec)
		ticks += run.tv_usec / 10000;

	return (ticks);
}

void
smi_oidlen(struct ber_oid *o)
{
	size_t	 i;

	for (i = 0; i < BER_MAX_OID_LEN && o->bo_id[i] != 0; i++)
		;
	o->bo_n = i;
}

char *
smi_oidstring(struct ber_oid *o, char *buf, size_t len)
{
	char		 str[256];
	struct oid	*value, key;
	size_t		 i, lookup = 1;

	bzero(buf, len);
	bzero(&key, sizeof(key));
	bcopy(o, &key.o_id, sizeof(struct ber_oid));
	key.o_flags |= OID_KEY;		/* do not match wildcards */

	if (env->sc_flags & SNMPD_F_NONAMES)
		lookup = 0;

	for (i = 0; i < o->bo_n; i++) {
		key.o_oidlen = i + 1;
		if (lookup &&
		    (value = RB_FIND(oidtree, &smi_oidtree, &key)) != NULL)
			snprintf(str, sizeof(str), "%s", value->o_name);
		else
			snprintf(str, sizeof(str), "%d", key.o_oid[i]);
		strlcat(buf, str, len);
		if (i < (o->bo_n - 1))
			strlcat(buf, ".", len);
	}

	return (buf);
}

void
smi_delete(struct oid *oid)
{
	struct oid	 key, *value;

	bcopy(&oid->o_id, &key.o_id, sizeof(struct ber_oid));
	if ((value = RB_FIND(oidtree, &smi_oidtree, &key)) != NULL &&
	    value == oid)
		RB_REMOVE(oidtree, &smi_oidtree, value);

	if (oid->o_data != NULL)
		free(oid->o_data);
	if (oid->o_flags & OID_DYNAMIC) {
		free(oid->o_name);
		free(oid);
	}
}

void
smi_insert(struct oid *oid)
{
	struct oid		 key, *value;

	if ((oid->o_flags & OID_TABLE) && oid->o_get == NULL)
		fatalx("smi_insert: invalid MIB table");

	bcopy(&oid->o_id, &key.o_id, sizeof(struct ber_oid));
	value = RB_FIND(oidtree, &smi_oidtree, &key);
	if (value != NULL)
		smi_delete(value);

	RB_INSERT(oidtree, &smi_oidtree, oid);
}

void
smi_mibtree(struct oid *oids)
{
	struct oid	*oid, *decl;
	size_t		 i;

	for (i = 0; oids[i].o_oid[0] != 0; i++) {
		oid = &oids[i];
		smi_oidlen(&oid->o_id);
		if (oid->o_name != NULL) {
			if ((oid->o_flags & OID_TABLE) && oid->o_get == NULL)
				fatalx("smi_mibtree: invalid MIB table");
			RB_INSERT(oidtree, &smi_oidtree, oid);
			continue;
		}
		decl = RB_FIND(oidtree, &smi_oidtree, oid);
		if (decl == NULL)
			fatalx("smi_mibtree: undeclared MIB");
		decl->o_flags = oid->o_flags;
		decl->o_get = oid->o_get;
		decl->o_set = oid->o_set;
		decl->o_table = oid->o_table;
		decl->o_val = oid->o_val;
		decl->o_data = oid->o_data;
	}
}

int
smi_init(void)
{
	RB_INIT(&smi_oidtree);
	mib_init();
	return (0);
}

struct oid *
smi_find(struct oid *oid)
{
	return (RB_FIND(oidtree, &smi_oidtree, oid));
}

struct oid *
smi_next(struct oid *oid)
{
	return (RB_NEXT(oidtree, &smi_oidtree, oid));
}

struct oid *
smi_foreach(struct oid *oid, u_int flags)
{
	/*
	 * Traverse the tree of MIBs with the option to check
	 * for specific OID flags.
	 */
	if (oid == NULL) {
		oid = RB_MIN(oidtree, &smi_oidtree);
		if (oid == NULL)
			return (NULL);
		if (flags == 0 || (oid->o_flags & flags))
			return (oid);
	}
	for (;;) {
		oid = RB_NEXT(oidtree, &smi_oidtree, oid);
		if (oid == NULL)
			break;
		if (flags == 0 || (oid->o_flags & flags))
			return (oid);
	}

	return (oid);
}

int
smi_oid_cmp(struct oid *a, struct oid *b)
{
	size_t	 i;

	for (i = 0; i < MIN(a->o_oidlen, b->o_oidlen); i++)
		if (a->o_oid[i] != b->o_oid[i])
			return (a->o_oid[i] - b->o_oid[i]);

	/*
	 * Return success if the matched object is a table
	 * (it will match any sub-elements)
	 */
	if ((b->o_flags & OID_TABLE) &&
	    (a->o_flags & OID_KEY) == 0 &&
	    (a->o_oidlen > b->o_oidlen))
		return (0);

	return (a->o_oidlen - b->o_oidlen);
}

RB_GENERATE(oidtree, oid, o_element, smi_oid_cmp);
