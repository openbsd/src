/*	$OpenBSD: mps.c,v 1.6 2007/12/28 16:27:51 reyk Exp $	*/

/*
 * Copyright (c) 2007 Reyk Floeter <reyk@vantronix.net>
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

RB_HEAD(oidtree, oid);
RB_PROTOTYPE(oidtree, oid, o_element, mps_oid_cmp);

extern struct snmpd *env;
struct oidtree mps_oidtree;

struct ber_oid *
	 mps_table(struct oid *, struct ber_oid *, struct ber_oid *);

u_long
mps_getticks(void)
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

int
mps_getstr(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	char			*s = oid->o_data;

	if (s == NULL)
		return (-1);
	*elm = ber_add_string(*elm, s);
	return (0);
}

int
mps_setstr(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	struct ber_element	*ber = *elm;
	char			*s;

	if ((oid->o_flags & OID_WR) == 0)
		return (-1);

	if (ber->be_class != BER_CLASS_UNIVERSAL ||
	    ber->be_type != BER_TYPE_OCTETSTRING)
		return (-1);
	if (ber_get_string(ber, &s) == -1)
		return (-1);
	if ((oid->o_data = (void *)strdup(s)) == NULL)
		return (-1);
	oid->o_val = strlen((char *)oid->o_data);

	return (0);
}

int
mps_getint(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	*elm = ber_add_integer(*elm, oid->o_val);
	return (0);
}

int
mps_setint(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	long long	 i;

	if (ber_get_integer(*elm, &i) == -1)
		return (-1);
	oid->o_val = i;

	return (0);
}

int
mps_getts(struct oid *oid, struct ber_oid *o, struct ber_element **elm)
{
	*elm = ber_add_integer(*elm, oid->o_val);
	ber_set_header(*elm, BER_CLASS_APPLICATION, SNMP_T_TIMETICKS);
	return (0);
}

void
mps_oidlen(struct ber_oid *o)
{
	size_t	 i;
	for (i = 0; i < BER_MAX_OID_LEN && o->bo_id[i] != 0; i++);
	o->bo_n = i;
}

char *
mps_oidstring(struct ber_oid *o, char *buf, size_t len)
{
	char		 str[256];
	struct oid	*value, key;
	size_t		 i, lookup = 1;

	bzero(buf, len);
	bcopy(o, &key.o_id, sizeof(struct ber_oid));
	key.o_flags |= OID_TABLE;	/* do not match wildcards */

	if (env->sc_flags & SNMPD_F_NONAMES)
		lookup = 0;

	for (i = 0; i < o->bo_n; i++) {
		key.o_oidlen = i + 1;
		if (lookup &&
		    (value = RB_FIND(oidtree, &mps_oidtree, &key)) != NULL)
			snprintf(str, sizeof(str), "%s", value->o_name);
		else
			snprintf(str, sizeof(str), "%d", key.o_oid[i]);
		strlcat(buf, str, len);
		if (i < (o->bo_n - 1))
			strlcat(buf, ".", len);
	}

	return (buf);
}

struct ber_element *
mps_getreq(struct ber_element *root, struct ber_oid *o)
{
	struct ber_element	*elm = root;
	struct oid		 key, *value;

	mps_oidlen(o);
	if (o->bo_n > BER_MAX_OID_LEN)
		return (NULL);
	bcopy(o, &key.o_id, sizeof(struct ber_oid));
	value = RB_FIND(oidtree, &mps_oidtree, &key);
	if (value == NULL)
		return (NULL);
	if (OID_NOTSET(value))
		return (NULL);
	if ((value->o_flags & OID_TABLE) == 0)
		elm = ber_add_oid(elm, &value->o_id);
	if (value->o_get == NULL)
		elm = ber_add_null(elm);
	else
		if (value->o_get(value, o, &elm) != 0)
			return (NULL);

	return (elm);
}

int
mps_setreq(struct ber_element *ber, struct ber_oid *o)
{
	struct oid		 key, *value;

	mps_oidlen(o);
	if (o->bo_n > BER_MAX_OID_LEN)
		return (-1);
	bcopy(o, &key.o_id, sizeof(struct ber_oid));
	value = RB_FIND(oidtree, &mps_oidtree, &key);
	if (value == NULL)
		return (-1);
	if ((value->o_flags & OID_WR) == 0 ||
	    value->o_set == NULL)
		return (-1);
	return (value->o_set(value, o, &ber));
}

struct ber_element *
mps_getnextreq(struct ber_element *root, struct ber_oid *o)
{
	struct oid		*next = NULL;
	struct ber_element	*ber = root;
	struct oid		 key, *value;
	int			 ret;
	struct ber_oid		 no;

	mps_oidlen(o);
	if (o->bo_n > BER_MAX_OID_LEN)
		return (NULL);
	bcopy(o, &key.o_id, sizeof(struct ber_oid));
	value = RB_FIND(oidtree, &mps_oidtree, &key);
	if (value == NULL)
		return (NULL);
	if (value->o_flags & OID_TABLE) {
		/* Get the next table row for this column */
		if (mps_table(value, o, &no) == NULL)
			return (NULL);
		bcopy(&no, o, sizeof(*o));
		ret = value->o_get(value, o, &ber);
		switch (ret) {
		case 0:
			return (ber);
		case -1:
			return (NULL);
		case 1:	/* end-of-rows */
			break;
		}
	}
	for (next = value; next != NULL;) {
		next = RB_NEXT(oidtree, &mps_oidtree, next);
		if (next == NULL)
			break;
		if (!OID_NOTSET(next) && next->o_get != NULL)
			break;
	}
	if (next == NULL || next->o_get == NULL)
		return (NULL);

	if (next->o_flags & OID_TABLE) {
		/* Get the next table row for this column */
		if (mps_table(next, o, &no) == NULL)
			return (NULL);
		bcopy(&no, o, sizeof(*o));
		if ((ret = next->o_get(next, o, &ber)) != 0)
			return (NULL);
	} else {
		bcopy(&next->o_id, o, sizeof(*o));
		ber = ber_add_oid(ber, &next->o_id);
		if ((ret = next->o_get(next, o, &ber)) != 0)
			return (NULL);
	}

	return (ber);
}

int
mps_set(struct ber_oid *o, void *p, long long len)
{
	struct oid		 key, *value;

	mps_oidlen(o);
	if (o->bo_n > BER_MAX_OID_LEN)
		return (-1);
	bcopy(o, &key.o_id, sizeof(struct ber_oid));
	value = RB_FIND(oidtree, &mps_oidtree, &key);
	if (value == NULL)
		return (-1);
	if (value->o_data != NULL)
		free(value->o_data);
	value->o_data = p;
	value->o_val = len;

	return (0);
}

struct ber_oid *
mps_table(struct oid *oid, struct ber_oid *o, struct ber_oid *no)
{
	u_int32_t		 col, idx = 1, id, subid;
	struct oid		 a, b;

	/*
	 * This function is being used to iterate through elements
	 * in a SMI "table". It is called by the mps_getnext() handler.
	 * For example, if the input is sysORIndex, it will return
	 * sysORIndex.1. If the input is sysORIndex.2, it will return
	 * sysORIndex.3 etc.. The MIB code has to verify the index,
	 * see mib_sysor() as an example.
	 */

	id = oid->o_oidlen - 1;
	subid = oid->o_oidlen;
	bcopy(&oid->o_id, no, sizeof(*no));

	if (o->bo_n >= oid->o_oidlen) {
		/*
		 * Compare the requested and the matched OID to see
		 * if we have to iterate to the next element.
		 */
		bzero(&a, sizeof(a));
		bcopy(o, &a.o_id, sizeof(struct ber_oid));
		bzero(&b, sizeof(b));
		bcopy(&oid->o_id, &b.o_id, sizeof(struct ber_oid));
		b.o_oidlen--;
		b.o_flags |= OID_TABLE;
		if (mps_oid_cmp(&a, &b) == 0) {
			col = oid->o_oid[id];
			if (col > o->bo_id[id])
				idx = 1;
			else
				idx = o->bo_id[subid] + 1;
			o->bo_id[subid] = idx;
			o->bo_id[id] = col;
			bcopy(o, no, sizeof(*no));
		}
	}

	/* The root element ends with a 0, iterate to the first element */
	if (!no->bo_id[subid])
		no->bo_id[subid]++;

	mps_oidlen(no);

	return (no);
}

void
mps_delete(struct oid *oid)
{
	struct oid	 key, *value;

	bcopy(&oid->o_id, &key.o_id, sizeof(struct ber_oid));
	if ((value = RB_FIND(oidtree, &mps_oidtree, &key)) != NULL &&
	    value == oid)
		RB_REMOVE(oidtree, &mps_oidtree, value);

	if (oid->o_data != NULL)
		free(oid->o_data);
	if (oid->o_flags & OID_DYNAMIC) {
		free(oid->o_name);
		free(oid);
	}
}

void
mps_insert(struct oid *oid)
{
	struct oid		 key, *value;

	if ((oid->o_flags & OID_TABLE) && oid->o_get == NULL)
		fatalx("mps_insert: invalid MIB table");

	bcopy(&oid->o_id, &key.o_id, sizeof(struct ber_oid));
	value = RB_FIND(oidtree, &mps_oidtree, &key);
	if (value != NULL)
		mps_delete(value);

	RB_INSERT(oidtree, &mps_oidtree, oid);
}

void
mps_mibtree(struct oid *oids)
{
	struct oid	*oid, *decl;
	size_t		 i;

	for (i = 0; oids[i].o_oid[0] != 0; i++) {
		oid = &oids[i];
		mps_oidlen(&oid->o_id);
		if (oid->o_name != NULL) {
			if ((oid->o_flags & OID_TABLE) && oid->o_get == NULL)
				fatalx("mps_mibtree: invalid MIB table");
			RB_INSERT(oidtree, &mps_oidtree, oid);
			continue;
		}
		decl = RB_FIND(oidtree, &mps_oidtree, oid);
		if (decl == NULL)
			fatalx("mps_mibtree: undeclared MIB");
		decl->o_flags = oid->o_flags;
		decl->o_get = oid->o_get;
		decl->o_set = oid->o_set;
		decl->o_val = oid->o_val;
		decl->o_data = oid->o_data;
	}
}

int
mps_init(void)
{
	RB_INIT(&mps_oidtree);
	mib_init();
	return (0);
}

struct oid *
mps_foreach(struct oid *oid, u_int flags)
{
	/*
	 * Traverse the tree of MIBs with the option to check
	 * for specific OID flags.
	 */
	if (oid == NULL) {
		oid = RB_MIN(oidtree, &mps_oidtree);
		if (oid == NULL)
			return (NULL);
		if (flags == 0 || (oid->o_flags & flags))
			return (oid);
	}
	for (;;) {
		oid = RB_NEXT(oidtree, &mps_oidtree, oid);
		if (oid == NULL)
			break;
		if (flags == 0 || (oid->o_flags & flags))
			return (oid);
	}

	return (oid);
}

long
mps_oid_cmp(struct oid *a, struct oid *b)
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
	    (a->o_flags & OID_TABLE) == 0)
		return (0);

	return (a->o_oidlen - b->o_oidlen);
}

RB_GENERATE(oidtree, oid, o_element, mps_oid_cmp);
