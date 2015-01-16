

/*
 * Copyright (c) 2007, 2008 Reyk Floeter <reyk@openbsd.org>
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
#include <limits.h>
#include <pwd.h>
#include <vis.h>

#include "snmpd.h"
#include "mib.h"

#define MINIMUM(a, b)	(((a) < (b)) ? (a) : (b))

extern struct snmpd *env;

RB_HEAD(oidtree, oid);
RB_PROTOTYPE(oidtree, oid, o_element, smi_oid_cmp);
struct oidtree smi_oidtree;

RB_HEAD(keytree, oid);
RB_PROTOTYPE(keytree, oid, o_keyword, smi_key_cmp);
struct keytree smi_keytree;

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

void
smi_scalar_oidlen(struct ber_oid *o)
{
	smi_oidlen(o);

	/* Append .0. */
	if (o->bo_n < BER_MAX_OID_LEN)
		o->bo_n++;
}

char *
smi_oid2string(struct ber_oid *o, char *buf, size_t len, size_t skip)
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
		if (lookup && skip > i)
			continue;
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

int
smi_string2oid(const char *oidstr, struct ber_oid *o)
{
	char			*sp, *p, str[BUFSIZ];
	const char		*errstr;
	struct oid		*oid;
	struct ber_oid		 ko;

	if (strlcpy(str, oidstr, sizeof(str)) >= sizeof(str))
		return (-1);
	bzero(o, sizeof(*o));

	/*
	 * Parse OID strings in the common form n.n.n or n-n-n.
	 * Based on ber_string2oid with additional support for symbolic names.
	 */
	for (p = sp = str; p != NULL; sp = p) {
		if ((p = strpbrk(p, ".-")) != NULL)
			*p++ = '\0';
		if ((oid = smi_findkey(sp)) != NULL) {
			bcopy(&oid->o_id, &ko, sizeof(ko));
			if (o->bo_n && ber_oid_cmp(o, &ko) != 2)
				return (-1);
			bcopy(&ko, o, sizeof(*o));
			errstr = NULL;
		} else {
			o->bo_id[o->bo_n++] =
			    strtonum(sp, 0, UINT_MAX, &errstr);
		}
		if (errstr || o->bo_n > BER_MAX_OID_LEN)
			return (-1);
	}

	return (0);
}

void
smi_delete(struct oid *oid)
{
	struct oid	 key, *value;

	bzero(&key, sizeof(key));
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

int
smi_insert(struct oid *oid)
{
	struct oid		 key, *value;

	if ((oid->o_flags & OID_TABLE) && oid->o_get == NULL)
		fatalx("smi_insert: invalid MIB table");

	bzero(&key, sizeof(key));
	bcopy(&oid->o_id, &key.o_id, sizeof(struct ber_oid));
	value = RB_FIND(oidtree, &smi_oidtree, &key);
	if (value != NULL)
		return (-1);

	RB_INSERT(oidtree, &smi_oidtree, oid);
	return (0);
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
			RB_INSERT(keytree, &smi_keytree, oid);
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
	/* Initialize the Structure of Managed Information (SMI) */
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
smi_findkey(char *name)
{
	struct oid	oid;
	if (name == NULL)
		return (NULL);
	oid.o_name = name;
	return (RB_FIND(keytree, &smi_keytree, &oid));
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

#ifdef DEBUG
void
smi_debug_elements(struct ber_element *root)
{
	static int	 indent = 0;
	char		*value;
	int		 constructed;

	/* calculate lengths */
	ber_calc_len(root);

	switch (root->be_encoding) {
	case BER_TYPE_SEQUENCE:
	case BER_TYPE_SET:
		constructed = root->be_encoding;
		break;
	default:
		constructed = 0;
		break;
	}

	fprintf(stderr, "%*slen %lu ", indent, "", root->be_len);
	switch (root->be_class) {
	case BER_CLASS_UNIVERSAL:
		fprintf(stderr, "class: universal(%u) type: ", root->be_class);
		switch (root->be_type) {
		case BER_TYPE_EOC:
			fprintf(stderr, "end-of-content");
			break;
		case BER_TYPE_BOOLEAN:
			fprintf(stderr, "boolean");
			break;
		case BER_TYPE_INTEGER:
			fprintf(stderr, "integer");
			break;
		case BER_TYPE_BITSTRING:
			fprintf(stderr, "bit-string");
			break;
		case BER_TYPE_OCTETSTRING:
			fprintf(stderr, "octet-string");
			break;
		case BER_TYPE_NULL:
			fprintf(stderr, "null");
			break;
		case BER_TYPE_OBJECT:
			fprintf(stderr, "object");
			break;
		case BER_TYPE_ENUMERATED:
			fprintf(stderr, "enumerated");
			break;
		case BER_TYPE_SEQUENCE:
			fprintf(stderr, "sequence");
			break;
		case BER_TYPE_SET:
			fprintf(stderr, "set");
			break;
		}
		break;
	case BER_CLASS_APPLICATION:
		fprintf(stderr, "class: application(%u) type: ",
		    root->be_class);
		switch (root->be_type) {
		case SNMP_T_IPADDR:
			fprintf(stderr, "ipaddr");
			break;
		case SNMP_T_COUNTER32:
			fprintf(stderr, "counter32");
			break;
		case SNMP_T_GAUGE32:
			fprintf(stderr, "gauge32");
			break;
		case SNMP_T_TIMETICKS:
			fprintf(stderr, "timeticks");
			break;
		case SNMP_T_OPAQUE:
			fprintf(stderr, "opaque");
			break;
		case SNMP_T_COUNTER64:
			fprintf(stderr, "counter64");
			break;
		}
		break;
	case BER_CLASS_CONTEXT:
		fprintf(stderr, "class: context(%u) type: ",
		    root->be_class);
		switch (root->be_type) {
		case SNMP_C_GETREQ:
			fprintf(stderr, "getreq");
			break;
		case SNMP_C_GETNEXTREQ:
			fprintf(stderr, "nextreq");
			break;
		case SNMP_C_GETRESP:
			fprintf(stderr, "getresp");
			break;
		case SNMP_C_SETREQ:
			fprintf(stderr, "setreq");
			break;
		case SNMP_C_TRAP:
			fprintf(stderr, "trap");
			break;
		case SNMP_C_GETBULKREQ:
			fprintf(stderr, "getbulkreq");
			break;
		case SNMP_C_INFORMREQ:
			fprintf(stderr, "informreq");
			break;
		case SNMP_C_TRAPV2:
			fprintf(stderr, "trapv2");
			break;
		case SNMP_C_REPORT:
			fprintf(stderr, "report");
			break;
		}
		break;
	case BER_CLASS_PRIVATE:
		fprintf(stderr, "class: private(%u) type: ", root->be_class);
		break;
	default:
		fprintf(stderr, "class: <INVALID>(%u) type: ", root->be_class);
		break;
	}
	fprintf(stderr, "(%lu) encoding %lu ",
	    root->be_type, root->be_encoding);

	if ((value = smi_print_element(root)) == NULL)
		goto invalid;

	switch (root->be_encoding) {
	case BER_TYPE_BOOLEAN:
		fprintf(stderr, "%s", value);
		break;
	case BER_TYPE_INTEGER:
	case BER_TYPE_ENUMERATED:
		fprintf(stderr, "value %s", value);
		break;
	case BER_TYPE_BITSTRING:
		fprintf(stderr, "hexdump %s", value);
		break;
	case BER_TYPE_OBJECT:
		fprintf(stderr, "oid %s", value);
		break;
	case BER_TYPE_OCTETSTRING:
		if (root->be_class == BER_CLASS_APPLICATION &&
		    root->be_type == SNMP_T_IPADDR) {
			fprintf(stderr, "addr %s", value);
		} else {
			fprintf(stderr, "string %s", value);
		}
		break;
	case BER_TYPE_NULL:	/* no payload */
	case BER_TYPE_EOC:
	case BER_TYPE_SEQUENCE:
	case BER_TYPE_SET:
	default:
		fprintf(stderr, "%s", value);
		break;
	}

 invalid:
	if (value == NULL)
		fprintf(stderr, "<INVALID>");
	else
		free(value);
	fprintf(stderr, "\n");

	if (constructed)
		root->be_encoding = constructed;

	if (constructed && root->be_sub) {
		indent += 2;
		smi_debug_elements(root->be_sub);
		indent -= 2;
	}
	if (root->be_next)
		smi_debug_elements(root->be_next);
}
#endif

char *
smi_print_element(struct ber_element *root)
{
	char		*str = NULL, *buf, *p;
	size_t		 len, i;
	long long	 v;
	int		 d;
	struct ber_oid	 o;
	char		 strbuf[BUFSIZ];

	switch (root->be_encoding) {
	case BER_TYPE_BOOLEAN:
		if (ber_get_boolean(root, &d) == -1)
			goto fail;
		if (asprintf(&str, "%s(%d)", d ? "true" : "false", d) == -1)
			goto fail;
		break;
	case BER_TYPE_INTEGER:
	case BER_TYPE_ENUMERATED:
		if (ber_get_integer(root, &v) == -1)
			goto fail;
		if (asprintf(&str, "%lld", v) == -1)
			goto fail;
		break;
	case BER_TYPE_BITSTRING:
		if (ber_get_bitstring(root, (void *)&buf, &len) == -1)
			goto fail;
		if ((str = calloc(1, len * 2 + 1)) == NULL)
			goto fail;
		for (p = str, i = 0; i < len; i++) {
			snprintf(p, 3, "%02x", buf[i]);
			p += 2;
		}
		break;
	case BER_TYPE_OBJECT:
		if (ber_get_oid(root, &o) == -1)
			goto fail;
		if (asprintf(&str, "%s",
		    smi_oid2string(&o, strbuf, sizeof(strbuf), 0)) == -1)
			goto fail;
		break;
	case BER_TYPE_OCTETSTRING:
		if (ber_get_string(root, &buf) == -1)
			goto fail;
		if (root->be_class == BER_CLASS_APPLICATION &&
		    root->be_type == SNMP_T_IPADDR) {
			if (asprintf(&str, "%s",
			    inet_ntoa(*(struct in_addr *)buf)) == -1)
				goto fail;
		} else {
			if ((p = malloc(root->be_len * 4 + 1)) == NULL)
				goto fail;
			strvisx(p, buf, root->be_len, VIS_NL);
			if (asprintf(&str, "\"%s\"", p) == -1) {
				free(p);
				goto fail;
			}
			free(p);
		}
		break;
	case BER_TYPE_NULL:	/* no payload */
	case BER_TYPE_EOC:
	case BER_TYPE_SEQUENCE:
	case BER_TYPE_SET:
	default:
		str = strdup("");
		break;
	}

	return (str);

 fail:
	if (str != NULL)
		free(str);
	return (NULL);
}

unsigned long
smi_application(struct ber_element *elm)
{
	if (elm->be_class != BER_CLASS_APPLICATION)
		return (BER_TYPE_OCTETSTRING);

	switch (elm->be_type) {
	case SNMP_T_IPADDR:
		return (BER_TYPE_OCTETSTRING);
	case SNMP_T_COUNTER32:
	case SNMP_T_GAUGE32:
	case SNMP_T_TIMETICKS:
	case SNMP_T_OPAQUE:
	case SNMP_T_COUNTER64:
		return (BER_TYPE_INTEGER);
	default:
		break;
	}
	return (BER_TYPE_OCTETSTRING);
}

int
smi_oid_cmp(struct oid *a, struct oid *b)
{
	size_t	 i;

	for (i = 0; i < MINIMUM(a->o_oidlen, b->o_oidlen); i++)
		if (a->o_oid[i] != b->o_oid[i])
			return (a->o_oid[i] - b->o_oid[i]);

	/*
	 * Return success if the matched object is a table
	 * or a MIB registered by a subagent
	 * (it will match any sub-elements)
	 */
	if ((b->o_flags & OID_TABLE ||
	    b->o_flags & OID_REGISTERED) &&
	    (a->o_flags & OID_KEY) == 0 &&
	    (a->o_oidlen > b->o_oidlen))
		return (0);

	return (a->o_oidlen - b->o_oidlen);
}

RB_GENERATE(oidtree, oid, o_element, smi_oid_cmp);

int
smi_key_cmp(struct oid *a, struct oid *b)
{
	if (a->o_name == NULL || b->o_name == NULL)
		return (-1);
	return (strcasecmp(a->o_name, b->o_name));
}

RB_GENERATE(keytree, oid, o_keyword, smi_key_cmp);
